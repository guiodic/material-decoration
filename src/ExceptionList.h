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

#pragma once

#include "InternalSettings.h"
#include <KSharedConfig>
#include <QList>
#include <QSharedPointer>

namespace Material
{

typedef QSharedPointer<InternalSettings> InternalSettingsPtr;
typedef QList<InternalSettingsPtr> InternalSettingsList;

enum ExceptionMask {
    None = 0,
    TitleAlignment = 1 << 0,
    ButtonSize = 1 << 1,
    CornerRadius = 1 << 2,
    Opacity = 1 << 3,
    HideTitleBar = 1 << 4,
    OutlineActive = 1 << 5,
    ShadowSize = 1 << 6,
    MenuAlwaysShow = 1 << 7,
    HamburgerMenu = 1 << 8,
    All = TitleAlignment | ButtonSize | CornerRadius | Opacity | HideTitleBar | OutlineActive | ShadowSize | MenuAlwaysShow | HamburgerMenu
};

class ExceptionList
{
public:
    explicit ExceptionList(const InternalSettingsList &exceptions = InternalSettingsList());

    const InternalSettingsList &get() const { return m_exceptions; }

    void readConfig(KSharedConfig::Ptr config);
    void writeConfig(KSharedConfig::Ptr config);

    static QString exceptionGroupName(int index);

protected:
    static void readConfig(KCoreConfigSkeleton *skeleton, KConfig *config, const QString &groupName);
    static void writeConfig(KCoreConfigSkeleton *skeleton, KConfig *config, const QString &groupName);

private:
    InternalSettingsList m_exceptions;
};

} // namespace Material
