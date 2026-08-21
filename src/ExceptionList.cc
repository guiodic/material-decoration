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

#include "ExceptionList.h"

#include <KConfigGroup>
#include <algorithm>

namespace Material
{

void copyInternalSettings(const InternalSettingsPtr &src, const InternalSettingsPtr &dst)
{
    if (!src || !dst) {
        return;
    }
    dst->setTitleAlignment(src->titleAlignment());
    dst->setButtonSize(src->buttonSize());
    dst->setActiveOpacity(src->activeOpacity());
    dst->setInactiveOpacity(src->inactiveOpacity());
    dst->setCornerRadius(src->cornerRadius());
    dst->setBottomCornerRadiusFlag(src->bottomCornerRadiusFlag());
    dst->setOutlineActive(src->outlineActive());
    dst->setUseSystemColors(src->useSystemColors());
    dst->setUseCustomBorderColors(src->useCustomBorderColors());
    dst->setActiveBorderColor(src->activeBorderColor());
    dst->setInactiveBorderColor(src->inactiveBorderColor());
    dst->setHideCaptionWhenLimitedSpace(src->hideCaptionWhenLimitedSpace());
    dst->setShowCaptionOnHover(src->showCaptionOnHover());
    dst->setMinWidthForCaption(src->minWidthForCaption());
    dst->setMenuAlwaysShow(src->menuAlwaysShow());
    dst->setSearchEnabled(src->searchEnabled());
    dst->setHamburgerMenu(src->hamburgerMenu());
    dst->setShowDisabledActions(src->showDisabledActions());
    dst->setSearchIgnoreTopLevel(src->searchIgnoreTopLevel());
    dst->setSearchIgnoreSubMenus(src->searchIgnoreSubMenus());
    dst->setMenuButtonHorzPadding(src->menuButtonHorzPadding());
    dst->setUseSystemMenuFont(src->useSystemMenuFont());
    dst->setAnimationsEnabled(src->animationsEnabled());
    dst->setAnimationsDuration(src->animationsDuration());
    dst->setShadowSize(src->shadowSize());
    dst->setShadowColor(src->shadowColor());
    dst->setShadowStrength(src->shadowStrength());
    dst->setLongPressEnabled(src->longPressEnabled());
    dst->setLongPressDuration(src->longPressDuration());
    dst->setDragFromButtonsEnabled(src->dragFromButtonsEnabled());

    dst->setExceptionPattern(src->exceptionPattern());
    dst->setExceptionType(src->exceptionType());
    dst->setMatchingMode(src->matchingMode());
    dst->setEnabled(src->enabled());
    dst->setMask(src->mask());
    dst->setHideTitleBar(src->hideTitleBar());
    dst->setHideApplicationMenu(src->hideApplicationMenu());
    dst->setHamburgerMenu(src->hamburgerMenu());
    dst->setSquareCorners(src->squareCorners());
    dst->setHideShadow(src->hideShadow());
}

InternalSettingsPtr cloneInternalSettings(const InternalSettingsPtr &src)
{
    if (!src) {
        return nullptr;
    }
    InternalSettingsPtr copy(new InternalSettings());
    copyInternalSettings(src, copy);
    return copy;
}

void ExceptionList::readConfig(const KSharedConfig::Ptr &config)
{
    m_exceptions.clear();

    const QString prefix = QStringLiteral("Windeco Exception ");
    const QStringList groupList = config->groupList();

    struct IndexedGroup {
        int index;
        QString name;
    };
    QList<IndexedGroup> exceptionGroups;

    for (const QString &groupName : groupList) {
        if (groupName.startsWith(prefix)) {
            bool ok = false;
            int index = groupName.mid(prefix.length()).toInt(&ok);
            if (ok) {
                exceptionGroups.append({index, groupName});
            }
        }
    }

    std::sort(exceptionGroups.begin(), exceptionGroups.end(), [](const IndexedGroup &a, const IndexedGroup &b) {
        return a.index < b.index;
    });

    for (const auto &grp : exceptionGroups) {
        InternalSettingsPtr exception(new InternalSettings());
        exception->setCurrentGroup(grp.name);
        exception->load();

        KConfigGroup group = config->group(grp.name);
        exception->setExceptionPattern(group.readEntry("ExceptionPattern", exception->exceptionPattern()));
        exception->setExceptionType(group.readEntry("ExceptionType", exception->exceptionType()));
        exception->setMatchingMode(group.readEntry("MatchingMode", exception->matchingMode()));
        exception->setEnabled(group.readEntry("Enabled", exception->enabled()));
        exception->setMask(group.readEntry("Mask", exception->mask()));
        exception->setHideTitleBar(group.readEntry("HideTitleBar", exception->hideTitleBar()));
        exception->setHideApplicationMenu(group.readEntry("HideApplicationMenu", exception->hideApplicationMenu()));
        exception->setHamburgerMenu(group.readEntry("HamburgerMenu", exception->hamburgerMenu()));
        exception->setHideShadow(group.readEntry("HideShadow", exception->hideShadow()));
        exception->setSquareCorners(group.readEntry("SquareCorners", exception->squareCorners()));
        exception->setOutlineActive(group.readEntry("OutlineActive", exception->outlineActive()));

        m_exceptions.append(exception);
    }
}

void ExceptionList::writeConfig(KSharedConfig::Ptr config)
{
    const QString prefix = QStringLiteral("Windeco Exception ");
    const QStringList groupList = config->groupList();

    for (const QString &groupName : groupList) {
        if (groupName.startsWith(prefix)) {
            bool ok = false;
            groupName.mid(prefix.length()).toInt(&ok);
            if (ok) {
                config->deleteGroup(groupName);
            }
        }
    }

    for (int i = 0; i < m_exceptions.size(); ++i) {
        const auto &exception = m_exceptions.at(i);
        const QString groupName = QStringLiteral("Windeco Exception %1").arg(i);
        KConfigGroup group = config->group(groupName);

        group.writeEntry("ExceptionPattern", exception->exceptionPattern());
        group.writeEntry("ExceptionType", exception->exceptionType());
        group.writeEntry("MatchingMode", exception->matchingMode());
        group.writeEntry("Enabled", exception->enabled());
        group.writeEntry("Mask", exception->mask());
        group.writeEntry("HideTitleBar", exception->hideTitleBar());
        group.writeEntry("HideApplicationMenu", exception->hideApplicationMenu());
        group.writeEntry("HamburgerMenu", exception->hamburgerMenu());
        group.writeEntry("HideShadow", exception->hideShadow());
        group.writeEntry("SquareCorners", exception->squareCorners());
        group.writeEntry("OutlineActive", exception->outlineActive());
    }

    config->sync();
}

} // namespace Material
