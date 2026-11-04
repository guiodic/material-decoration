// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QRectF>

namespace Material
{
// Bounds are the usable titlebar edges, in decoration-local coordinates.
inline Qt::Edges buttonCornerEdges(const QRectF &geometry, qreal left, qreal right, qreal scale)
{
    const qreal tolerance = 0.5 / scale;
    Qt::Edges edges;
    if (qAbs(geometry.left() - left) <= tolerance) {
        edges |= Qt::LeftEdge;
    }
    if (qAbs(geometry.right() - right) <= tolerance) {
        edges |= Qt::RightEdge;
    }
    return edges;
}
}
