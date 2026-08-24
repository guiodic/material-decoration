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

// own
#include "SearchButton.h"

// Qt
#include <QPainter>
#include <QtMath>

namespace Material
{

SearchButton::SearchButton(Decoration *decoration, const int buttonIndex, QObject *parent)
    : AppMenuButton(decoration, buttonIndex, parent)
{
}

SearchButton::~SearchButton() = default;

void SearchButton::paintIcon(QPainter *painter, const QRectF &iconRect, const PixelSnapper &snapper)
{
    Q_UNUSED(iconRect)
    
    const QRectF circleRect = snapper.snap(QRectF(QPointF(-6.0, -6.0), QPointF(2.0, 2.0)));
    painter->drawEllipse(circleRect);

    const qreal sqrt2 = std::sqrt(2.0);
    const qreal handleLength = 5.0;

    const QPointF circleCenter = circleRect.center();
    const qreal rx = circleRect.width() / 2.0;
    const qreal ry = circleRect.height() / 2.0;
    const QPointF handleStart = snapper.snap(
        circleCenter + QPointF(rx / sqrt2, ry / sqrt2));
    const QPointF handleEnd = snapper.snap(
        circleCenter + QPointF(
            (rx + handleLength) / sqrt2,
            (ry + handleLength) / sqrt2));
    painter->drawLine(handleStart, handleEnd);
}

} // namespace Material
