/*
 * Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>
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
#include <QColor>
#include <QPainter>
#include <QPoint>
#include <QRect>

namespace Material
{
namespace BoxShadowHelper
{

void boxShadow(QPainter *p, const QRect &box, const QPoint &offset,
               int radius, const QColor &color);

} // namespace BoxShadowHelper
} // namespace Material
