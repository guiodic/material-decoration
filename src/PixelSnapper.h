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

/**
 * @brief Utility for snapping painter coordinates and pen widths to physical pixel boundaries.
 *
 * PixelSnapper uses the painter's active transformation matrix and device pixel ratio (DPR)
 * to align coordinates to physical pixel boundaries, preventing antialiasing blur on crisp lines and shapes.
 */
class PixelSnapper
{
public:
    /**
     * @brief Constructs a PixelSnapper instance for a painter.
     * @param painter The QPainter whose transformation matrix and DPR will be used for snapping.
     */
    explicit PixelSnapper(QPainter *painter);

    /**
     * @brief Snaps a point in painter local coordinates to the nearest physical pixel boundary.
     * @param p Point in local coordinates.
     * @return Snapped point in local coordinates.
     */
    QPointF snap(const QPointF &p) const;

    /**
     * @brief Snaps a rectangle in painter local coordinates to physical pixel boundaries.
     * @param rect Rectangle in local coordinates.
     * @return Snapped rectangle in local coordinates.
     */
    QRectF snap(const QRectF &rect) const;

    /**
     * @brief Snaps stroked point geometry to the physical pixel grid considering pen stroke alignment.
     *
     * A stroke with an odd physical-pixel width is centered between pixels;
     * an even-width stroke is centered on a pixel. This keeps the complete
     * stroke on physical pixel boundaries instead of distributing it over
     * adjacent pixels during antialiasing.
     *
     * @param p Point in local coordinates.
     * @param penWidth Pen width in local units.
     * @return Snapped point in local coordinates.
     */
    QPointF snapForPen(const QPointF &p, qreal penWidth) const;

    /**
     * @brief Snaps stroked rectangle geometry to the physical pixel grid considering pen stroke alignment.
     * @param rect Rectangle in local coordinates.
     * @param penWidth Pen width in local units.
     * @return Snapped rectangle in local coordinates.
     */
    QRectF snapForPen(const QRectF &rect, qreal penWidth) const;

    /**
     * @brief Computes the integer physical pixel width for a given local pen width.
     * @param localPenWidth Nominal pen width in painter local coordinates.
     * @return Integer physical pixel width (at least 1).
     */
    qint64 physicalPenWidth(qreal localPenWidth) const;

    /**
     * @brief Computes the snapped local pen width corresponding to an integer physical pixel width.
     * @param nominalLocalPenWidth Nominal pen width in painter local coordinates.
     * @return Snapped pen width in painter local units.
     */
    qreal snappedPenWidth(qreal nominalLocalPenWidth) const;

    /**
     * @brief Computes the scaling factor from local coordinates to physical device pixels.
     *
     * @return Combined scaling factor (transformation scale * DPR).
     * @note Semantics & Assumptions:
     * - Assumes uniform scaling (X axis scaling identical to Y axis).
     * - Assumes no rotation or shear in the transformation matrix.
     * - In the presence of non-uniform scaling or rotation/shear, returns the scaling factor
     *   along the transformed X axis (magnitude of the first matrix column) multiplied by DPR.
     */
    qreal localToPhysicalScale() const;

    /**
     * @brief Returns the device pixel ratio (DPR) associated with the target device.
     * @return Device pixel ratio.
     */
    qreal dpr() const { return m_dpr; }

private:
    QTransform m_trans;
    QTransform m_inv;
    qreal m_dpr;
    bool m_invertible;
};

} // namespace Material
