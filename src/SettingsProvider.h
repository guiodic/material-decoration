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
#include "InternalSettings.h"

#include <QObject>
#include <QRegularExpression>
#include <QSharedPointer>

namespace Material
{

class Decoration;

class SettingsProvider : public QObject
{
    Q_OBJECT

public:
    static SettingsProvider *self();

    SettingsProvider();
    ~SettingsProvider() override = default;

    void reconfigure();

    InternalSettingsPtr internalSettings(Decoration *decoration);

    InternalSettingsPtr createMergedSettings(const InternalSettingsPtr &defaultSettings,
                                              const InternalSettingsPtr &exceptionSettings);

signals:
    void configChanged();

private:
    struct CompiledException {
        InternalSettingsPtr mergedSettings;
        QRegularExpression regex;
        QString pattern;
        int type = 0; // 0: Window Title, 1: Window Class
        int matchingMode = 0; // 0: Exact Match, 1: Regular Expression
        bool enabled = true;
    };

    InternalSettingsPtr m_defaultSettings;
    ExceptionList m_exceptions;
    QList<CompiledException> m_compiledExceptions;
};

} // namespace Material
