/*
 * Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>
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

// own
#include "Button.h"

// KDecoration
#include <KDecoration3/DecoratedWindow>

// Qt
#include <QPainter>

namespace Material
{

class MaximizeButton
{

public:
    static void init(Button *button, KDecoration3::DecoratedWindow *decoratedClient) {
        QObject::connect(decoratedClient, &KDecoration3::DecoratedWindow::maximizeableChanged,
                button, &Button::setVisible);

        button->setVisible(decoratedClient->isMaximizeable());
    }
    
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, const PixelSnapper &snapper) {
        Q_UNUSED(iconRect)
        Q_UNUSED(button)
        
        const auto penWidth = painter->pen().widthF();

        // We use drawLine() instead of drawPolyline() and drawRect() 
        // to avoid artifacts at corners using snapForPen()
        const auto snap = [&snapper, painter, penWidth](const QPointF &point) {
            return snapper.snapForPen(point, penWidth);
        };
        const auto drawSegment = [painter, &snap](const QPointF &start, const QPointF &end) {
            painter->drawLine(snap(start), snap(end));
        };
        const auto drawOutline = [&drawSegment](qreal left, qreal top, qreal right, qreal bottom) {
            const QPointF topLeft(left, top);
            const QPointF topRight(right, top);
            const QPointF bottomRight(right, bottom);
            const QPointF bottomLeft(left, bottom);

            drawSegment(topLeft, topRight);
            drawSegment(topRight, bottomRight);
            drawSegment(bottomRight, bottomLeft);
            drawSegment(bottomLeft, topLeft);
        };

        if (button->isChecked()) {
            const qreal offset = penWidth * 1.5;

            // Foreground square, aligned bottom-left.
            drawOutline(-5.0, -5.0 + offset, 5.0 - offset, 5.0);

            // Visible parts of the background square, aligned top-right.
            drawSegment(QPointF(-5.0 + offset, -5.0 + offset), QPointF(-5.0 + offset, -5.0));
            drawSegment(QPointF(-5.0 + offset, -5.0), QPointF(5.0, -5.0));
            drawSegment(QPointF(5.0, -5.0), QPointF(5.0, 5.0 - offset));
            drawSegment(QPointF(5.0, 5.0 - offset), QPointF(5.0 - offset, 5.0 - offset));
        } else {
            drawOutline(-5.0, -5.0, 5.0, 5.0);
        }
    }
};

} // namespace Material
