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
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, const qreal) {
        Q_UNUSED(iconRect)
        const QRectF innerRect(-5, -5, 10, 10);
        if (button->isChecked()) {
            const int offset = 2;
            // Outline of first square, "on top", aligned bottom left.
            painter->drawPolygon(QVector<QPointF> {
                innerRect.bottomLeft(),
                innerRect.topLeft() + QPointF(0, offset),
                innerRect.topRight() + QPointF(-offset, offset),
                innerRect.bottomRight() + QPointF(-offset, 0)
            });

            // Partially occluded square, "below" first square, aligned top right.
            painter->drawPolyline(QVector<QPointF> {
                innerRect.topLeft() + QPointF(offset, offset),
                innerRect.topLeft() + QPointF(offset, 0),
                innerRect.topRight(),
                innerRect.bottomRight() + QPointF(0, -offset),
                innerRect.bottomRight() + QPointF(-offset, -offset)
            });
        } else {
            painter->drawRect(innerRect);
        }
    }
};

} // namespace Material
