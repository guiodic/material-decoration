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
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, const qreal dpr) {
        Q_UNUSED(iconRect)
        button->setPenWidth(painter, 1.25);

        PixelSnapper snapper(painter, dpr);
        auto snapPoint = [&snapper](const QPointF &p) -> QPointF {
            return snapper.snap(p);
        };

        const QPointF offset(-5.5, -5.5);

        const QPointF p1 = snapPoint(QPointF( 1.5, 0.5 ) + offset);
        const qreal topCurveW = snapper.snapX(8.0);
        const qreal topCurveH = snapper.snapY(6.0);
        const QRectF topCurveRect(p1, QSizeF(topCurveW, topCurveH));

        QPainterPath path;
        path.moveTo(snapPoint(topCurveRect.center() - QPointF(topCurveRect.width() / 2.0, 0.0)));
        path.arcTo(
            topCurveRect,
            180,
            -180
        );
        path.cubicTo(
            snapPoint(QPointF( 7.8125, 5.9375 ) + offset),
            snapPoint(QPointF( 5.625, 4.6875 ) + offset),
            snapPoint(QPointF( 5.0, 8.0 ) + offset)
        );
        painter->drawPath(path);

        // Dot
        const QPointF dotPos = snapPoint(QPointF( 5.0, 10.0 ) + offset);
        const qreal dotW = snapper.snapX(0.5);
        const qreal dotH = snapper.snapY(0.5);
        painter->drawRect(QRectF(dotPos, QSizeF(dotW, dotH)));
    }
};

} // namespace Material
