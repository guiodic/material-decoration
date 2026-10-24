// SPDX-License-Identifier: GPL-2.0-or-later
#include "DrawHelper.h"

namespace Material
{
QPainterPath DrawHelper::getRoundedPath(const QRectF &rect, qreal radius,
                                        bool roundTopLeft, bool roundTopRight,
                                        bool roundBottomLeft, bool roundBottomRight)
{
    QPainterPath path;

    const int roundedCorners = roundTopLeft + roundTopRight + roundBottomLeft + roundBottomRight;

    if (radius <= 0 || roundedCorners == 0) {
        path.addRect(rect);
        return path;
    }

    if (roundedCorners == 4) {
        path.addRoundedRect(rect, radius, radius);
        return path;
    }

    // Keep partial corner arcs inside small rectangles.
    radius = qMin(radius, qMin(rect.width(), rect.height()) / 2);

    path.moveTo(rect.topRight() - QPointF(roundTopRight ? radius : 0, 0));

    path.lineTo(rect.topLeft() + QPointF(roundTopLeft ? radius : 0, 0));

    if (roundTopLeft) {
        path.arcTo(QRectF(rect.topLeft(), QSizeF(2 * radius, 2 * radius)),
                   90, 90);
    }

    path.lineTo(rect.bottomLeft() + QPointF(0, -(roundBottomLeft ? radius : 0)));

    if (roundBottomLeft) {
        path.arcTo(QRectF(rect.bottomLeft() - QPointF(0, 2 * radius),
                          QSizeF(2 * radius, 2 * radius)),
                   180, 90);
    }

    path.lineTo(rect.bottomRight() - QPointF(roundBottomRight ? radius : 0, 0));

    if (roundBottomRight) {
        path.arcTo(QRectF(rect.bottomRight() - QPointF(2 * radius, 2 * radius),
                          QSizeF(2 * radius, 2 * radius)),
                   270, 90);
    }

    path.lineTo(rect.topRight() - QPointF(0, -(roundTopRight ? radius : 0)));

    if (roundTopRight) {
        path.arcTo(QRectF(rect.topRight() - QPointF(2 * radius, 0),
                          QSizeF(2 * radius, 2 * radius)),
                   0, 90);
    }

    path.closeSubpath();
    return path;
}
}
