// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QColor>
#include <QPainterPath>

class QPainter;

namespace Material
{
void paintRoundedButtonBackground(QPainter *painter, const QRectF &geometry,
                                  const QPainterPath &borderPath, const QPainterPath &titleBarPath,
                                  const QColor &borderColor, const QColor &baseColor,
                                  const QColor &overlayColor, qreal opacity);
}
