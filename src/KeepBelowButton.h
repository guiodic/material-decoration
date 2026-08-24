/*
 * Copyright (C) 2020 Chris Holland <zrenfire@gmail.com>
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

class KeepBelowButton
{

public:
    static void init(Button *button, KDecoration3::DecoratedWindow *decoratedClient) {
        Q_UNUSED(decoratedClient)

        button->setVisible(true);
    }
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, const PixelSnapper &snapper) {
        Q_UNUSED(iconRect)
        Q_UNUSED(button)

        const QPointF offset(-5.0, -5.0);
        painter->drawPolyline(  QVector<QPointF> {
            snapper.snap(QPointF( 0.0, 0.0 ) + offset),
            snapper.snap(QPointF( 5.0, 5.0 ) + offset),
            snapper.snap(QPointF( 10.0, 0.0 ) + offset)
        });

        painter->drawPolyline(  QVector<QPointF> {
            snapper.snap(QPointF( 0.0, 5.0 ) + offset),
            snapper.snap(QPointF( 5.0, 10.0 ) + offset),
            snapper.snap(QPointF( 10.0, 5.0 ) + offset)
        });
    }
};

} // namespace Material
