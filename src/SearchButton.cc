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

#include "SearchButton.h"
#include "Decoration.h"
#include <QPainter>
#include <QtMath>

namespace Material
{

SearchButton::SearchButton(Decoration *decoration, QObject *parent)
    : Button(KDecoration3::DecorationButtonType::Custom, decoration, parent)
{
    setCheckable(true);
}

SearchButton::~SearchButton() = default;

void SearchButton::paintIcon(QPainter *painter, const QRectF &iconRect, const qreal gridUnit)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    setPenWidth(painter, gridUnit, 1.25);

    const qreal circleRadius = gridUnit * 3.5;
    const qreal handleLength = gridUnit * 5;

    // To center the whole icon, we need to find the bounding box of the final icon
    // and then shift the drawing by an offset.
    // The icon is a circle and a handle at 45 degrees.
    // Let's just offset the center of the circle a bit to the top-left.
    const qreal offset = handleLength / 2 / qSqrt(2);
    const QPointF circleCenter = iconRect.center() - QPointF(offset, offset);

    // Draw the circle
    painter->drawEllipse(circleCenter, circleRadius, circleRadius);

    // Draw the handle
    const qreal handleAttachOffset = circleRadius / qSqrt(2);
    const QPointF handleStart = circleCenter + QPointF(handleAttachOffset, handleAttachOffset);
    const qreal handleEndOffset = (circleRadius + handleLength) / qSqrt(2);
    const QPointF handleEnd = circleCenter + QPointF(handleEndOffset, handleEndOffset);
    painter->drawLine(handleStart, handleEnd);
}

} // namespace Material
