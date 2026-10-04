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

/**
 * @brief Specifies the window attribute targeted by an exception rule.
 */
enum class ExceptionType {
    WindowTitle = 0, ///< Target window title/caption string
    WindowClass = 1, ///< Target window class/resource string
};

/**
 * @brief Specifies pattern matching mode for an exception rule.
 */
enum class MatchingMode {
    ExactMatch = 0,        ///< Case-insensitive exact component/string matching mode
    RegularExpression = 1, ///< Regular-expression matching mode
};

/// @brief Maximum allowed character length for exception match pattern strings (256).
constexpr int MaxExceptionPatternLength = 256;

/// @brief Maximum allowed character length for evaluated window titles/classes (1024).
constexpr int MaxExceptionValueLength = 1024;

/**
 * @brief Bitmask flags indicating which specific settings are overridden by an exception rule.
 */
enum ExceptionMask {
    None = 0,                      ///< No settings overridden
    HideTitleBar = 1 << 0,         ///< Override hide title bar setting
    HideApplicationMenu = 1 << 1,  ///< Override hide application menu setting
    HamburgerMenu = 1 << 2,        ///< Override hamburger menu setting
    HideShadow = 1 << 3,           ///< Override hide shadow setting
    SquareCorners = 1 << 4,        ///< Override square corners setting
    OutlineActive = 1 << 5,        ///< Override thin outline active setting
};

/**
 * @brief Copies field values from source settings to destination settings.
 * @param src Source settings instance.
 * @param dst Destination settings instance.
 */
void copyInternalSettings(const InternalSettingsPtr &src, const InternalSettingsPtr &dst);

/**
 * @brief Creates a deep copy clone of an InternalSettings instance.
 * @param src Source settings instance.
 * @return Deep copy InternalSettings instance.
 */
InternalSettingsPtr cloneInternalSettings(const InternalSettingsPtr &src);

/**
 * @brief Validates a regular expression pattern for length, syntax, and heuristic ReDoS checks.
 * @param pattern Regular expression string to check.
 * @param errorReason Optional output parameter receiving failure explanation if pattern is unsafe.
 * @return True if the pattern is syntactically valid and passes the heuristic (length and quantifier) ReDoS checks.
 */
bool isSafeRegularExpression(const QString &pattern, QString *errorReason = nullptr);

/**
 * @brief Class managing persistence (reading and writing) of window exception rules in KConfig.
 */
class ExceptionList
{
public:
    /**
     * @brief Default constructor.
     */
    ExceptionList() = default;

    /**
     * @brief Reads all configured exception groups ("Windeco Exception N") from config.
     * @param config KSharedConfig pointer.
     */
    void readConfig(const KSharedConfig::Ptr &config);

    /**
     * @brief Writes all managed exception rules to config groups and cleans up deleted groups.
     * @param config KSharedConfig pointer.
     */
    void writeConfig(KSharedConfig::Ptr config);

    /**
     * @brief Returns all exception rule settings, including disabled rules.
     * @return Reference to internal exception list.
     */
    const InternalSettingsList &exceptions() const { return m_exceptions; }

    /**
     * @brief Sets list of exception rule settings.
     * @param exceptions List of exception settings to store.
     */
    void setExceptions(const InternalSettingsList &exceptions) { m_exceptions = exceptions; }

private:
    InternalSettingsList m_exceptions;
};

} // namespace Material
