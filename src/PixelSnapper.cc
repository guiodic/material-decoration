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

#include "PixelSnapper.h"
#include <QPainter>
#include <cmath>

namespace Material
{

PixelSnapper::PixelSnapper(QPainter *painter, const qreal dpr)
    : m_trans(painter->transform())
    , m_dpr(dpr)
    , m_invertible(false)
{
    m_inv = m_trans.inverted(&m_invertible);
}

QPointF PixelSnapper::snap(const QPointF &p) const
{
    if (m_dpr > 0.0 && m_invertible) {
        const QPointF devInd = m_trans.map(p);
        const QPointF phys(devInd.x() * m_dpr, devInd.y() * m_dpr);
        const QPointF physSnapped(qRound(phys.x()), qRound(phys.y()));
        const QPointF devIndSnapped(physSnapped.x() / m_dpr, physSnapped.y() / m_dpr);
        return m_inv.map(devIndSnapped);
    }
    return p;
}

qreal PixelSnapper::snapX(const qreal v) const
{
    const QPointF p0 = snap(QPointF(0.0, 0.0));
    const QPointF pV = snap(QPointF(v, 0.0));
    const qreal len = std::hypot(pV.x() - p0.x(), pV.y() - p0.y());
    return (v < 0.0) ? -len : len;
}

qreal PixelSnapper::snapY(const qreal v) const
{
    const QPointF p0 = snap(QPointF(0.0, 0.0));
    const QPointF pV = snap(QPointF(0.0, v));
    const qreal len = std::hypot(pV.x() - p0.x(), pV.y() - p0.y());
    return (v < 0.0) ? -len : len;
}

qreal PixelSnapper::snap(const qreal v) const
{
    return snapX(v);
}

qreal PixelSnapper::localToPhysicalScale() const
{
    return std::hypot(m_trans.m11(), m_trans.m12()) * m_dpr;
}

} // namespace Material
