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

class ApplicationMenuButton
{

public:
    static void init(Button *button, KDecoration3::DecoratedWindow *decoratedClient) {
        button->setVisible(decoratedClient->hasApplicationMenu());
    }
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, const qreal) {
        Q_UNUSED(iconRect)
        button->setPenWidth(painter, 1.5);

        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        const qreal scaleX = qAbs(painter->transform().m11());
        const qreal localToPhysical = scaleX * dpr;

        const qreal spacing = 4.0;
        for (int i = -1; i <= 1; ++i) {
            qreal y = i * spacing;
            qreal xLeft = -5.5;
            qreal xRight = 5.5;
            if (localToPhysical > 0.0) {
                y = qRound(y * localToPhysical) / localToPhysical;
                xLeft = qRound(xLeft * localToPhysical) / localToPhysical;
                xRight = qRound(xRight * localToPhysical) / localToPhysical;
            }
            const QPointF left { xLeft, y };
            const QPointF right { xRight, y };

            painter->drawLine(left, right);
        }
    }
};

} // namespace Material
