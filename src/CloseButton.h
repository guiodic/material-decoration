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

class CloseButton
{

public:
    static void init(Button *button, KDecoration3::DecoratedWindow *decoratedClient) {
        QObject::connect(decoratedClient, &KDecoration3::DecoratedWindow::closeableChanged,
                button, &Button::setVisible);

        button->setVisible(decoratedClient->isCloseable());
    }
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, const PixelSnapper &snapper) {
        Q_UNUSED(iconRect)
        Q_UNUSED(button)

        const QPointF p1 = snapper.snap(QPointF(-5.0, -5.0));
        const QPointF p2 = snapper.snap(QPointF(5.0, 5.0));
        const QPointF p3 = snapper.snap(QPointF(5.0, -5.0));
        const QPointF p4 = snapper.snap(QPointF(-5.0, 5.0));

        painter->drawLine(p1, p2);
        painter->drawLine(p3, p4);
    }
};

} // namespace Material
