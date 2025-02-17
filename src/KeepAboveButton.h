/*
 * Copyright (C) 2019 Zain Ahmad <zain.x.ahmad@gmail.com>
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

class KeepAboveButton
{

public:
    static void init(Button *button, KDecoration3::DecoratedWindow *decoratedClient) {
        Q_UNUSED(decoratedClient)

        button->setVisible(true);
    }
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, const qreal gridUnit) {
        button->setPenWidth(painter, gridUnit, 1.25);

        painter->translate( iconRect.topLeft() );
        painter->drawPolyline(  QVector<QPointF> {
            QPointF( 0.5, 4.75 ) * gridUnit,
            QPointF( 5.0, 0.25 ) * gridUnit,
            QPointF( 9.5, 4.75 ) * gridUnit
        });

        painter->drawPolyline(  QVector<QPointF> {
            QPointF( 0.5, 9.75 ) * gridUnit,
            QPointF( 5.0, 5.25 ) * gridUnit,
            QPointF( 9.5, 9.75 ) * gridUnit
        });
    }
};

} // namespace Material
