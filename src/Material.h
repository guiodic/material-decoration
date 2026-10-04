/*
 * Copyright (C) 2020 Chris Holland <zrenfire@gmail.com>
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

// Qt
#include <QLoggingCategory>


namespace Material
{

    /// @brief Logging category for the Material decoration plugin ("kdecoration.material").
    static const QLoggingCategory category("kdecoration.material");

    /// @brief Configuration filename for Material theme settings ("kdecoration_materialrc").
    static const QString s_configFilename = QStringLiteral("kdecoration_materialrc");

    /// @brief Scale adjustment factor for top corner rounding.
    static constexpr qreal cornerRadiusAdjustment = 0.7;


    /**
     * @brief Standard pen stroke width constants for rendering decoration icons.
     */
    namespace PenWidth
    {
        /**
         * @brief Standard pen stroke width for vector symbols (1.01).
         *
         * Using 1.01 instead of 1.0 prevents stroke skewing on certain display scaling factors.
         */
        static constexpr qreal Symbol = 1.01;
    }

} // namespace Material
