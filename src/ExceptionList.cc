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
#include <utility>

namespace Material
{

ExceptionList::ExceptionList(const InternalSettingsList &exceptions)
    : m_exceptions(exceptions)
{
}

void ExceptionList::readConfig(KSharedConfig::Ptr config)
{
    m_exceptions.clear();

    QString groupName;
    for (int index = 0; config->hasGroup(groupName = exceptionGroupName(index)); ++index) {
        InternalSettingsPtr configuration(new InternalSettings());
        configuration->load();
        readConfig(configuration.data(), config.data(), groupName);
        m_exceptions.append(configuration);
    }
}

void ExceptionList::writeConfig(KSharedConfig::Ptr config)
{
    QString groupName;
    for (int index = 0; config->hasGroup(groupName = exceptionGroupName(index)); ++index) {
        config->deleteGroup(groupName);
    }

    int index = 0;
    for (const InternalSettingsPtr &exception : std::as_const(m_exceptions)) {
        writeConfig(exception.data(), config.data(), exceptionGroupName(index));
        ++index;
    }
}

QString ExceptionList::exceptionGroupName(int index)
{
    return QStringLiteral("Windeco Exception %1").arg(index);
}

void ExceptionList::writeConfig(KCoreConfigSkeleton *skeleton, KConfig *config, const QString &groupName)
{
    const QStringList keys = {
        QStringLiteral("Enabled"),
        QStringLiteral("ExceptionPattern"),
        QStringLiteral("ExceptionType"),
        QStringLiteral("Mask"),
        QStringLiteral("HideTitleBar"),
        QStringLiteral("TitleAlignment"),
        QStringLiteral("ButtonSize"),
        QStringLiteral("CornerRadius"),
        QStringLiteral("ActiveOpacity"),
        QStringLiteral("InactiveOpacity"),
        QStringLiteral("OutlineActive"),
        QStringLiteral("ShadowSize"),
        QStringLiteral("MenuAlwaysShow"),
        QStringLiteral("HamburgerMenu")
    };

    for (const auto &key : keys) {
        KConfigSkeletonItem *item = skeleton->findItem(key);
        if (!item) {
            continue;
        }

        if (!groupName.isEmpty()) {
            item->setGroup(groupName);
        }
        KConfigGroup configGroup(config, item->group());
        configGroup.writeEntry(item->key(), item->property());
    }
}

void ExceptionList::readConfig(KCoreConfigSkeleton *skeleton, KConfig *config, const QString &groupName)
{
    const auto items = skeleton->items();
    for (KConfigSkeletonItem *item : items) {
        if (!groupName.isEmpty()) {
            item->setGroup(groupName);
        }
        item->readConfig(config);
    }
}

} // namespace Material
