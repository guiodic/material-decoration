// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QPainterPath>
#include <QRectF>

namespace Material
{
class DrawHelper
{
public:
    static QPainterPath getRoundedPath(const QRectF &rect, qreal radius,
                                      bool roundTopLeft = true, bool roundTopRight = true,
                                      bool roundBottomLeft = false, bool roundBottomRight = false);
};
}
