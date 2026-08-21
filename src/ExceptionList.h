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

using InternalSettingsPtr = QSharedPointer<InternalSettings>;
using InternalSettingsList = QList<InternalSettingsPtr>;

enum class ExceptionType {
    WindowTitle = 0,
    WindowClass = 1,
};

enum class MatchingMode {
    ExactMatch = 0,
    RegularExpression = 1,
};

enum ExceptionMask {
    None = 0,
    HideTitleBar = 1 << 0,
    HideApplicationMenu = 1 << 1,
    HamburgerMenu = 1 << 2,
    HideShadow = 1 << 3,
    SquareCorners = 1 << 4,
};

void copyInternalSettings(const InternalSettingsPtr &src, const InternalSettingsPtr &dst);
InternalSettingsPtr cloneInternalSettings(const InternalSettingsPtr &src);

class ExceptionList
{
public:
    ExceptionList() = default;

    void readConfig(const KSharedConfig::Ptr &config);
    void writeConfig(KSharedConfig::Ptr config);

    const InternalSettingsList &exceptions() const { return m_exceptions; }
    void setExceptions(const InternalSettingsList &exceptions) { m_exceptions = exceptions; }

private:
    InternalSettingsList m_exceptions;
};

} // namespace Material
