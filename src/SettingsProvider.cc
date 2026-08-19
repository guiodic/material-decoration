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
#include <utility>

namespace Material
{

SettingsProvider *SettingsProvider::s_self = nullptr;

SettingsProvider::SettingsProvider()
    : m_config(KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc")))
{
    reconfigure();
}

SettingsProvider::~SettingsProvider()
{
    s_self = nullptr;
}

SettingsProvider *SettingsProvider::self()
{
    if (!s_self) {
        s_self = new SettingsProvider();
    }
    return s_self;
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
            m_compiledExceptions.push_back({ex, QRegularExpression(ex->exceptionPattern())});
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
