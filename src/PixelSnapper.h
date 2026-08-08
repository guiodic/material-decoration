/*
 * Copyright (C) 2025 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
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

#include <QTransform>
#include <QPointF>

class QPainter;

namespace Material
{

class PixelSnapper
{
public:
    PixelSnapper(QPainter *painter, const qreal dpr);

    QPointF snap(const QPointF &p) const;
    qreal snap(const qreal v) const;
    qreal snapX(const qreal v) const;
    qreal snapY(const qreal v) const;
    qreal localToPhysicalScale() const;
    qreal dpr() const { return m_dpr; }

private:
    QTransform m_trans;
    QTransform m_inv;
    qreal m_dpr;
    bool m_invertible;
};

} // namespace Material
