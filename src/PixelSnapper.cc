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

#include "PixelSnapper.h"
#include <QPainter>
#include <cmath>

namespace Material
{

PixelSnapper::PixelSnapper(QPainter *painter)
    : m_trans(painter ? painter->transform() : QTransform())
    , m_dpr(painter && painter->device() ? painter->device()->devicePixelRatioF() : 1.0)
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

QRectF PixelSnapper::snap(const QRectF &rect) const
{
    const QRectF normRect = rect.normalized();
    return QRectF(snap(normRect.topLeft()), snap(normRect.bottomRight()));
}

qreal PixelSnapper::localToPhysicalScale() const
{
    // Semantics & Assumptions check:
    // This assumes uniform scale and no rotation/shear. If those assumptions are violated, 
    // it computes the scale factor of the X-axis mapping under the transformation.
    if (m_dpr > 0.0 && m_invertible) {
        return std::hypot(m_trans.m11(), m_trans.m12()) * m_dpr;
    }
    return 0.0;
}

} // namespace Material
