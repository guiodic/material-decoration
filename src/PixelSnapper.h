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

#include <QTransform>
#include <QPointF>
#include <QRectF>

class QPainter;

namespace Material
{

class PixelSnapper
{
public:
    explicit PixelSnapper(QPainter *painter);

    QPointF snap(const QPointF &p) const;
    QRectF snap(const QRectF &rect) const;

    /**
     * @brief Snaps stroked geometry to the physical pixel grid.
     *
     * A stroke with an odd physical-pixel width is centered between pixels;
     * an even-width stroke is centered on a pixel.  This keeps the complete
     * stroke on physical pixel boundaries instead of distributing it over
     * adjacent pixels during antialiasing.
     */
    QPointF snapForPen(const QPointF &p, qreal penWidth) const;
    QRectF snapForPen(const QRectF &rect, qreal penWidth) const;

    /**
     * @brief Computes the scaling factor from local coordinates to physical device pixels.
     * 
     * @note Semantics & Assumptions:
     * - This method assumes uniform scaling (scaling on the X axis is identical to the Y axis).
     * - It assumes there is no rotation or shear applied to the transformation matrix.
     * - In the presence of non-uniform scaling or rotation/shear, it returns the scaling factor
     *   along the transformed X axis (calculating the magnitude/hypotenuse of the first column
     *   of the transformation matrix), multiplied by the device pixel ratio (DPR).
     */
    qreal localToPhysicalScale() const;
    
    qreal dpr() const { return m_dpr; }

private:
    QTransform m_trans;
    QTransform m_inv;
    qreal m_dpr;
    bool m_invertible;
};

} // namespace Material
