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
#include <utility>

namespace Material
{

SettingsProvider::SettingsProvider()
    : m_config(KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc")))
{
    reconfigure();

    auto dbus = QDBusConnection::sessionBus();
    dbus.connect(QString(),
                 QStringLiteral("/KGlobalSettings"),
                 QStringLiteral("org.kde.KGlobalSettings"),
                 QStringLiteral("notifyChange"),
                 this,
                 SLOT(reconfigure()));
}

SettingsProvider::~SettingsProvider() = default;

SettingsProvider *SettingsProvider::self()
{
    static SettingsProvider s_self;
    return &s_self;
}

void SettingsProvider::reconfigure()
{
    if (!m_defaultSettings) {
        m_defaultSettings = InternalSettingsPtr(new InternalSettings());
        m_defaultSettings->setCurrentGroup(QStringLiteral("Windeco"));
    }

    m_config->reparseConfiguration();
    m_defaultSettings->load();

    ExceptionList exceptions;
    exceptions.readConfig(m_config);
    m_exceptions = exceptions.get();

    m_compiledExceptions.clear();
    for (const auto &ex : std::as_const(m_exceptions)) {
        if (ex->enabled() && !ex->exceptionPattern().isEmpty()) {
            InternalSettingsPtr mergedSettings(new InternalSettings());

            mergedSettings->setEnabled(m_defaultSettings->enabled());
            mergedSettings->setHideTitleBar(m_defaultSettings->hideTitleBar());
            mergedSettings->setTitleAlignment(m_defaultSettings->titleAlignment());
            mergedSettings->setButtonSize(m_defaultSettings->buttonSize());
            mergedSettings->setCornerRadius(m_defaultSettings->cornerRadius());
            mergedSettings->setActiveOpacity(m_defaultSettings->activeOpacity());
            mergedSettings->setInactiveOpacity(m_defaultSettings->inactiveOpacity());
            mergedSettings->setOutlineActive(m_defaultSettings->outlineActive());
            mergedSettings->setShadowSize(m_defaultSettings->shadowSize());
            mergedSettings->setMenuAlwaysShow(m_defaultSettings->menuAlwaysShow());
            mergedSettings->setHamburgerMenu(m_defaultSettings->hamburgerMenu());
            mergedSettings->setUseCustomBorderColors(m_defaultSettings->useCustomBorderColors());
            mergedSettings->setActiveBorderColor(m_defaultSettings->activeBorderColor());
            mergedSettings->setInactiveBorderColor(m_defaultSettings->inactiveBorderColor());
            mergedSettings->setBottomCornerRadiusFlag(m_defaultSettings->bottomCornerRadiusFlag());
            mergedSettings->setHideCaptionWhenLimitedSpace(m_defaultSettings->hideCaptionWhenLimitedSpace());
            mergedSettings->setShowCaptionOnHover(m_defaultSettings->showCaptionOnHover());
            mergedSettings->setSearchEnabled(m_defaultSettings->searchEnabled());
            mergedSettings->setShowDisabledActions(m_defaultSettings->showDisabledActions());
            mergedSettings->setSearchIgnoreTopLevel(m_defaultSettings->searchIgnoreTopLevel());
            mergedSettings->setSearchIgnoreSubMenus(m_defaultSettings->searchIgnoreSubMenus());
            mergedSettings->setMenuButtonHorzPadding(m_defaultSettings->menuButtonHorzPadding());
            mergedSettings->setShadowColor(m_defaultSettings->shadowColor());
            mergedSettings->setShadowStrength(m_defaultSettings->shadowStrength());
            mergedSettings->setAnimationsEnabled(m_defaultSettings->animationsEnabled());
            mergedSettings->setAnimationsDuration(m_defaultSettings->animationsDuration());
            mergedSettings->setUseSystemMenuFont(m_defaultSettings->useSystemMenuFont());
            mergedSettings->setMinWidthForCaption(m_defaultSettings->minWidthForCaption());
            mergedSettings->setDragFromButtonsEnabled(m_defaultSettings->dragFromButtonsEnabled());
            mergedSettings->setLongPressEnabled(m_defaultSettings->longPressEnabled());
            mergedSettings->setLongPressDuration(m_defaultSettings->longPressDuration());

            mergedSettings->setExceptionType(ex->exceptionType());
            mergedSettings->setExceptionPattern(ex->exceptionPattern());
            mergedSettings->setMask(ex->mask());

            const int mask = ex->mask();
            if (mask & HideTitleBar) {
                mergedSettings->setHideTitleBar(ex->hideTitleBar());
            }
            if (mask & TitleAlignment) {
                mergedSettings->setTitleAlignment(ex->titleAlignment());
            }
            if (mask & ButtonSize) {
                mergedSettings->setButtonSize(ex->buttonSize());
            }
            if (mask & CornerRadius) {
                mergedSettings->setCornerRadius(ex->cornerRadius());
            }
            if (mask & Opacity) {
                mergedSettings->setActiveOpacity(ex->activeOpacity());
                mergedSettings->setInactiveOpacity(ex->inactiveOpacity());
            }
            if (mask & OutlineActive) {
                mergedSettings->setOutlineActive(ex->outlineActive());
            }
            if (mask & ShadowSize) {
                mergedSettings->setShadowSize(ex->shadowSize());
            }
            if (mask & MenuAlwaysShow) {
                mergedSettings->setMenuAlwaysShow(ex->menuAlwaysShow());
            }
            if (mask & HamburgerMenu) {
                mergedSettings->setHamburgerMenu(ex->hamburgerMenu());
            }

            m_compiledExceptions.push_back({mergedSettings, QRegularExpression(ex->exceptionPattern())});
        }
    }
}

InternalSettingsPtr SettingsProvider::internalSettings(Decoration *decoration) const
{
    if (!decoration || !decoration->window()) {
        return m_defaultSettings;
    }

    QString windowTitle;
    QString windowClass;

    for (const auto &item : m_compiledExceptions) {
        const auto &internalSettings = item.settings;
        if (!internalSettings->enabled() || internalSettings->exceptionPattern().isEmpty()) {
            continue;
        }

        QString value;
        switch (internalSettings->exceptionType()) {
        case InternalSettings::ExceptionWindowTitle: {
            value = windowTitle.isEmpty() ? (windowTitle = decoration->window()->caption()) : windowTitle;
            break;
        }
        default:
        case InternalSettings::ExceptionWindowClassName: {
            value = windowClass.isEmpty() ? (windowClass = decoration->window()->windowClass()) : windowClass;
            break;
        }
        }

        if (item.regex.match(value).hasMatch()) {
            return internalSettings;
        }
    }

    return m_defaultSettings;
}

} // namespace Material
