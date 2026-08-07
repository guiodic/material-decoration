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

namespace Material
{

class ShadeButton
{

public:
    static void init(Button *button, KDecoration3::DecoratedWindow *decoratedClient) {
        QObject::connect(decoratedClient, &KDecoration3::DecoratedWindow::shadeableChanged,
                button, &Button::setVisible);

        button->setVisible(decoratedClient->isShadeable());
    }
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, const qreal) {
        Q_UNUSED(iconRect)
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        const qreal scaleX = qAbs(painter->transform().m11());
        const qreal localToPhysical = scaleX * dpr;

        auto snapPoint = [localToPhysical](const QPointF &p) -> QPointF {
            if (localToPhysical > 0.0) {
                return QPointF(
                    qRound(p.x() * localToPhysical) / localToPhysical,
                    qRound(p.y() * localToPhysical) / localToPhysical
                );
            }
            return p;
        };

        const QPointF offset(-5.0, -5.0);

        if (button->isChecked()) {
            button->setPenWidth(painter, 1.25);
            painter->drawLine( 
                snapPoint(QPointF( 0, 2 ) + offset),
                snapPoint(QPointF( 10, 2 ) + offset)
            );
            button->setPenWidth(painter, 1.25);
            painter->drawPolyline(  QVector<QPointF> {
                snapPoint(QPointF( 0.5, 5.25 ) + offset),
                snapPoint(QPointF( 5.0, 9.75 ) + offset),
                snapPoint(QPointF( 9.5, 5.25 ) + offset)
            });
        } else {
            button->setPenWidth(painter, 1.25);
            painter->drawLine( 
                snapPoint(QPointF( 0, 2 ) + offset),
                snapPoint(QPointF( 10, 2 ) + offset)
            );
            button->setPenWidth(painter, 1.25);
            painter->drawPolyline( QVector<QPointF> {
                snapPoint(QPointF( 0.5, 9.75 ) + offset),
                snapPoint(QPointF( 5.0, 5.25 ) + offset),
                snapPoint(QPointF( 9.5, 5.25 ) + offset)
            });
        }
    }
};

} // namespace Material
