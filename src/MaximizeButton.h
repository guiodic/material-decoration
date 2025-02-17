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
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, qreal gridUnit) {
        if (button->isChecked()) {
            const int offset = qRound(gridUnit * 2);
            // Outline of first square, "on top", aligned bottom left.
            painter->drawPolygon(QVector<QPointF> {
                iconRect.bottomLeft(),
                iconRect.topLeft() + QPointF(0, offset),
                iconRect.topRight() + QPointF(-offset, offset),
                iconRect.bottomRight() + QPointF(-offset, 0)
            });

            // Partially occluded square, "below" first square, aligned top right.
            painter->drawPolyline(QVector<QPointF> {
                iconRect.topLeft() + QPointF(offset, offset),
                iconRect.topLeft() + QPointF(offset, 0),
                iconRect.topRight(),
                iconRect.bottomRight() + QPointF(0, -offset),
                iconRect.bottomRight() + QPointF(-offset, -offset)
            });
        } else {
            painter->drawRect(iconRect);
        }
    }
};

} // namespace Material
