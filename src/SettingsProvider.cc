/*
 * Copyright (C) 2026 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "SettingsProvider.h"
#include "Decoration.h"

#include <KDecoration3/DecoratedWindow>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>

namespace Material
{

SettingsProvider *SettingsProvider::self()
{
    static SettingsProvider s_self;
    return &s_self;
}

SettingsProvider::SettingsProvider()
{
    reconfigure();

    QDBusConnection::sessionBus().connect(
        QString(),
        QStringLiteral("/KGlobalSettings"),
        QStringLiteral("org.kde.KGlobalSettings"),
        QStringLiteral("notifyChange"),
        this,
        SLOT(reconfigure()));
}

void SettingsProvider::reconfigure()
{
    m_defaultSettings = InternalSettingsPtr(new InternalSettings());
    m_defaultSettings->load();

    auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
    config->reparseConfiguration();
    m_exceptions.readConfig(config);

    m_compiledExceptions.clear();

    for (const auto &exceptionSettings : m_exceptions.exceptions()) {
        if (!exceptionSettings->enabled()) {
            continue;
        }

        CompiledException compiled;
        compiled.type = exceptionSettings->exceptionType();
        compiled.matchingMode = exceptionSettings->matchingMode();
        compiled.pattern = exceptionSettings->exceptionPattern().trimmed();
        compiled.enabled = exceptionSettings->enabled();

        if (compiled.pattern.isEmpty()) {
            continue;
        }

        if (compiled.matchingMode == 1) { // Regular Expression
            QRegularExpression regex(compiled.pattern, QRegularExpression::CaseInsensitiveOption);
            if (!regex.isValid()) {
                qWarning() << "Invalid exception regular expression pattern:" << compiled.pattern << regex.errorString();
                continue;
            }
            compiled.regex = regex;
        }

        compiled.mergedSettings = createMergedSettings(m_defaultSettings, exceptionSettings);
        m_compiledExceptions.append(compiled);
    }

    emit configChanged();
}

InternalSettingsPtr SettingsProvider::createMergedSettings(const InternalSettingsPtr &defaultSettings,
                                                             const InternalSettingsPtr &exceptionSettings)
{
    InternalSettingsPtr merged(new InternalSettings());

    merged->setTitleAlignment(defaultSettings->titleAlignment());
    merged->setButtonSize(defaultSettings->buttonSize());
    merged->setActiveOpacity(defaultSettings->activeOpacity());
    merged->setInactiveOpacity(defaultSettings->inactiveOpacity());
    merged->setCornerRadius(defaultSettings->cornerRadius());
    merged->setBottomCornerRadiusFlag(defaultSettings->bottomCornerRadiusFlag());
    merged->setOutlineActive(defaultSettings->outlineActive());
    merged->setUseSystemColors(defaultSettings->useSystemColors());
    merged->setUseCustomBorderColors(defaultSettings->useCustomBorderColors());
    merged->setActiveBorderColor(defaultSettings->activeBorderColor());
    merged->setInactiveBorderColor(defaultSettings->inactiveBorderColor());
    merged->setHideCaptionWhenLimitedSpace(defaultSettings->hideCaptionWhenLimitedSpace());
    merged->setShowCaptionOnHover(defaultSettings->showCaptionOnHover());
    merged->setMinWidthForCaption(defaultSettings->minWidthForCaption());
    merged->setMenuAlwaysShow(defaultSettings->menuAlwaysShow());
    merged->setSearchEnabled(defaultSettings->searchEnabled());
    merged->setHamburgerMenu(defaultSettings->hamburgerMenu());
    merged->setShowDisabledActions(defaultSettings->showDisabledActions());
    merged->setSearchIgnoreTopLevel(defaultSettings->searchIgnoreTopLevel());
    merged->setSearchIgnoreSubMenus(defaultSettings->searchIgnoreSubMenus());
    merged->setMenuButtonHorzPadding(defaultSettings->menuButtonHorzPadding());
    merged->setUseSystemMenuFont(defaultSettings->useSystemMenuFont());
    merged->setAnimationsEnabled(defaultSettings->animationsEnabled());
    merged->setAnimationsDuration(defaultSettings->animationsDuration());
    merged->setShadowSize(defaultSettings->shadowSize());
    merged->setShadowColor(defaultSettings->shadowColor());
    merged->setShadowStrength(defaultSettings->shadowStrength());
    merged->setLongPressEnabled(defaultSettings->longPressEnabled());
    merged->setLongPressDuration(defaultSettings->longPressDuration());
    merged->setDragFromButtonsEnabled(defaultSettings->dragFromButtonsEnabled());

    const int mask = exceptionSettings->mask();

    if ((mask & ExceptionMask::HideTitleBar) || exceptionSettings->hideTitleBar()) {
        merged->setHideTitleBar(exceptionSettings->hideTitleBar());
    }

    if ((mask & ExceptionMask::HideApplicationMenu) || exceptionSettings->hideApplicationMenu()) {
        merged->setHideApplicationMenu(exceptionSettings->hideApplicationMenu());
        if (exceptionSettings->hideApplicationMenu()) {
            merged->setMenuAlwaysShow(false);
        }
    }

    if ((mask & ExceptionMask::HamburgerMenu) || exceptionSettings->hamburgerMenu()) {
        merged->setHamburgerMenu(exceptionSettings->hamburgerMenu());
    }

    if ((mask & ExceptionMask::HideShadow) || exceptionSettings->hideShadow()) {
        merged->setHideShadow(exceptionSettings->hideShadow());
        if (exceptionSettings->hideShadow()) {
            merged->setShadowSize(InternalSettings::ShadowNone);
        }
    }

    if ((mask & ExceptionMask::SquareCorners) || exceptionSettings->squareCorners()) {
        merged->setSquareCorners(exceptionSettings->squareCorners());
        if (exceptionSettings->squareCorners()) {
            merged->setCornerRadius(0);
        }
    }

    return merged;
}

InternalSettingsPtr SettingsProvider::internalSettings(Decoration *decoration)
{
    if (!decoration || !decoration->window()) {
        return m_defaultSettings;
    }

    const QString caption = decoration->window()->caption();
    const QString windowClass = decoration->window()->windowClass();

    for (const auto &compiled : m_compiledExceptions) {
        if (!compiled.enabled || compiled.pattern.isEmpty()) {
            continue;
        }

        const QString valueToMatch = (compiled.type == 0) ? caption : windowClass;
        bool matches = false;

        if (compiled.matchingMode == 0) { // Exact Match
            if (valueToMatch.compare(compiled.pattern, Qt::CaseInsensitive) == 0) {
                matches = true;
            } else if (compiled.type == 1) { // Window Class component match
                const QStringList components = valueToMatch.split(QRegularExpression(QStringLiteral("[\\s\\r\\n\\t\\x00]+")), Qt::SkipEmptyParts);
                for (const QString &comp : components) {
                    if (comp.compare(compiled.pattern, Qt::CaseInsensitive) == 0) {
                        matches = true;
                        break;
                    }
                }
            } else if (compiled.type == 0) { // Window Title match
                matches = valueToMatch.contains(compiled.pattern, Qt::CaseInsensitive);
            }
        } else if (compiled.matchingMode == 1) { // Regular Expression
            matches = compiled.regex.match(valueToMatch).hasMatch();
        }

        if (matches) {
            return compiled.mergedSettings;
        }
    }

    return m_defaultSettings;
}

} // namespace Material
