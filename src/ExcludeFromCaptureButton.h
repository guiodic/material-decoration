/*
 * Copyright (C) 2026 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
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
#include "BuildConfig.h"
#include "Button.h"
#include "Decoration.h"
#include "Material.h"

// KDecoration
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/ScaleHelpers>

// Qt
#include <QPainter>
#include <QVector>

namespace Material
{

#if HAVE_EXCLUDE_FROM_CAPTURE
class ExcludeFromCaptureButton
{

public:
    static void init(Button *button, KDecoration3::DecoratedWindow *decoratedClient) {
        Q_UNUSED(button)
        Q_UNUSED(decoratedClient)
        // KDecoration3::DecorationButton already handles the state and visibility 
        // connections for ExcludeFromCapture.
    }
    
    //--- copied from Breeze for now. Copyright goes to KDE Developers
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, const qreal dpr) {
        Q_UNUSED(iconRect)
        const auto *deco = qobject_cast<Decoration *>(button->decoration());
        if (!deco) {
            return;
        }
        button->setPenWidth(painter, 1.25 * KDecoration3::pixelSize(deco->window()->scale()));

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

        // A spy hat (like view-private.svg icon)
        // Hat crown with dip/crease at top (filled)
        const QVector<QPointF> crownPoints {
            snapPoint(QPointF(-4.1, -2.5)),
            snapPoint(QPointF(-3.2, -6.1)),
            snapPoint(QPointF(0, -5)),
            snapPoint(QPointF(3.2, -6.1)),
            snapPoint(QPointF(4.1, -2.5))
        };
        painter->setBrush(painter->pen().color());
        painter->drawPolygon(crownPoints);
        painter->setBrush(Qt::NoBrush);

        // Hat brim
        painter->drawLine(snapPoint(QPointF(-6.2, -0.5)), snapPoint(QPointF(6.2, -0.5)));

        // Glasses' lenses
        qreal rx = 2.3;
        qreal ry = 2.3;
        if (localToPhysical > 0.0) {
            rx = qRound(rx * localToPhysical) / localToPhysical;
            ry = qRound(ry * localToPhysical) / localToPhysical;
        }
        painter->drawEllipse(snapPoint(QPointF(-4.0, 3.8)), rx, ry);
        painter->drawEllipse(snapPoint(QPointF(4.0, 3.8)), rx, ry);

        // Bridge between lenses
        painter->drawLine(snapPoint(QPointF(-1.5, 3.8)), snapPoint(QPointF(1.5, 3.8)));
    }
};
#endif

} // namespace Material
