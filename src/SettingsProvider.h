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

#include "ExceptionList.h"
#include <KSharedConfig>
#include <QObject>

namespace Material
{

class Decoration;

class SettingsProvider : public QObject
{
    Q_OBJECT

public:
    ~SettingsProvider() override;

    static SettingsProvider *self();

    InternalSettingsPtr internalSettings(Decoration *decoration) const;

public Q_SLOTS:
    void reconfigure();

private:
    SettingsProvider();

    InternalSettingsPtr m_defaultSettings;
    InternalSettingsList m_exceptions;
    KSharedConfigPtr m_config;

    static SettingsProvider *s_self;
};

} // namespace Material
