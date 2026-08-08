/*
 * Copyright (C) 2021 Chris Holland <zrenfire@gmail.com>
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
#include "Material.h"

// KDecoration
#include <KDecoration3/DecoratedWindow>

// Qt
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSizeF>

namespace Material
{

class ContextHelpButton
{

public:
    static void init(Button *button, KDecoration3::DecoratedWindow *decoratedClient) {
        QObject::connect(decoratedClient, &KDecoration3::DecoratedWindow::providesContextHelpChanged,
                button, &Button::setVisible);

        button->setVisible(decoratedClient->providesContextHelp());
    }
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, const PixelSnapper &snapper) {
        Q_UNUSED(iconRect)
        button->setPenWidth(painter, 1.25);

        const QPointF offset(-5.5, -5.5);

        const QRectF topCurveRect = snapper.snap(QRectF(QPointF(1.5, 0.5) + offset, QPointF(9.5, 6.5) + offset));

        QPainterPath path;
        path.moveTo(QPointF(topCurveRect.left(), topCurveRect.top() + topCurveRect.height() / 2.0));
        path.arcTo(
            topCurveRect,
            180,
            -180
        );
        path.cubicTo(
            snapper.snap(QPointF( 7.8125, 5.9375 ) + offset),
            snapper.snap(QPointF( 5.625, 4.6875 ) + offset),
            snapper.snap(QPointF( 5.0, 8.0 ) + offset)
        );
        painter->drawPath(path);

        // Dot
        painter->drawRect(snapper.snap(QRectF(QPointF(5.0, 10.0) + offset, QPointF(5.5, 10.5) + offset)));
    }
};

} // namespace Material
