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

#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QSharedPointer>

namespace Material
{

class Decoration;

/**
 * @brief Singleton provider managing default decoration settings, window exception rules, and caching.
 *
 * SettingsProvider loads default configuration settings from `kdecoration_materialrc`, parses and validates
 * window exception rules (`ExceptionList`), pre-compiles regular-expression patterns after heuristic checks for ReDoS-prone quantifiers and stores exact-match patterns,
 * merges exception overrides onto default settings, and caches evaluation results per window class/caption.
 */
class SettingsProvider : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Returns the singleton instance of SettingsProvider.
     * @return Pointer to global SettingsProvider instance.
     */
    static SettingsProvider *self();

    /**
     * @brief Constructor.
     */
    SettingsProvider();

    /**
     * @brief Destructor.
     */
    ~SettingsProvider() override = default;

    /**
     * @brief Resolves and returns merged settings for a given Decoration instance.
     * @param decoration Target Decoration instance.
     * @return Shared pointer to resolved InternalSettings.
     */
    InternalSettingsPtr internalSettings(Decoration *decoration);

    /**
     * @brief Resolves and returns merged settings for specified window class and caption.
     * @param windowClass Window class string (e.g. "org.kde.kate").
     * @param caption Optional window title/caption string.
     * @return Shared pointer to resolved InternalSettings.
     */
    InternalSettingsPtr internalSettings(const QString &windowClass, const QString &caption = QString());

    /**
     * @brief Creates a merged InternalSettings object applying exception mask overrides over default settings.
     * @param defaultSettings Baseline default settings.
     * @param exceptionSettings Exception rule settings containing override flags.
     * @return Merged InternalSettings instance.
     */
    InternalSettingsPtr createMergedSettings(const InternalSettingsPtr &defaultSettings,
                                              const InternalSettingsPtr &exceptionSettings);

    /**
     * @brief Clears the internal evaluation result cache.
     */
    void clearCache();

public slots:
    /**
     * @brief Reloads configuration files from disk, re-compiles exception rules, and emits configChanged().
     */
    void reconfigure();

signals:
    /**
     * @brief Emitted when configuration settings or exception rules are modified.
     */
    void configChanged();

private:
    struct CompiledException {
        InternalSettingsPtr mergedSettings;
        QRegularExpression regex;
        QString pattern;
        ExceptionType type = ExceptionType::WindowTitle;
        MatchingMode matchingMode = MatchingMode::ExactMatch;
        bool enabled = true;
    };

    InternalSettingsPtr m_defaultSettings;
    ExceptionList m_exceptions;
    QList<CompiledException> m_compiledExceptions;
    QHash<QString, InternalSettingsPtr> m_cache;
    bool m_hasWindowTitleExceptions = false;
};

} // namespace Material
