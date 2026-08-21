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

        m_exceptions.append(exception);
    }
}

void ExceptionList::writeConfig(KSharedConfig::Ptr config)
{
    const QString prefix = QStringLiteral("Windeco Exception ");
    const QStringList groupList = config->groupList();

    for (const QString &groupName : groupList) {
        if (groupName.startsWith(prefix)) {
            config->deleteGroup(groupName);
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
    }

    config->sync();
}

} // namespace Material
