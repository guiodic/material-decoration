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
    
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, const PixelSnapper &snapper) {
        Q_UNUSED(iconRect)
        
        const auto penScale = button->penScale();
        button->setPenWidth(painter, penScale, true); // we have horizontal/vertical drawing, so we snap the pen
        const auto penWidth = painter->pen().widthF();

        const qreal localToPhys = snapper.localToPhysicalScale();

        if (localToPhys > 0.0 && snapper.dpr() > 0.0) {
            const qreal nominalPhysBase = 10.0 * localToPhys;
            const qint64 physBase = qMax<qint64>(1, qRound64(nominalPhysBase));
            const qreal d_base = static_cast<qreal>(physBase) / localToPhys;

            if (button->isChecked()) {
                const qreal nominalPhysOffset = penScale * 1.5 * localToPhys;
                const qint64 physOffset = qMax<qint64>(1, qRound64(nominalPhysOffset));
                const qint64 physSquare = qMax<qint64>(1, physBase - physOffset);

                const qreal d_offset = static_cast<qreal>(physOffset) / localToPhys;
                const qreal d_square = static_cast<qreal>(physSquare) / localToPhys;

                // Foreground square: bottom-left aligned reference corner
                const QPointF fgBottomLeft = snapper.snapForPen(QPointF(-d_base / 2.0, d_base / 2.0), penWidth);
                const QPointF fgTopLeft(fgBottomLeft.x(), fgBottomLeft.y() - d_square);
                const QPointF fgTopRight(fgBottomLeft.x() + d_square, fgBottomLeft.y() - d_square);
                const QPointF fgBottomRight(fgBottomLeft.x() + d_square, fgBottomLeft.y());

                // Draw foreground square
                painter->drawLine(fgTopLeft, fgTopRight);
                painter->drawLine(fgTopRight, fgBottomRight);
                painter->drawLine(fgBottomRight, fgBottomLeft);
                painter->drawLine(fgBottomLeft, fgTopLeft);

                // Background square: shifted by (+physOffset, -physOffset) physical pixels
                const QPointF shift(d_offset, -d_offset);
                const QPointF bgTopLeft = fgTopLeft + shift;
                const QPointF bgTopRight = fgTopRight + shift;
                const QPointF bgBottomRight = fgBottomRight + shift;

                // Visible parts of the background square, aligned top-right
                painter->drawLine(QPointF(bgTopLeft.x(), fgTopLeft.y()), bgTopLeft);
                painter->drawLine(bgTopLeft, bgTopRight);
                painter->drawLine(bgTopRight, bgBottomRight);
                painter->drawLine(bgBottomRight, QPointF(fgBottomRight.x(), bgBottomRight.y()));
            } else {
                const QPointF bottomLeft = snapper.snapForPen(QPointF(-d_base / 2.0, d_base / 2.0), penWidth);
                const QPointF topLeft(bottomLeft.x(), bottomLeft.y() - d_base);
                const QPointF topRight(bottomLeft.x() + d_base, bottomLeft.y() - d_base);
                const QPointF bottomRight(bottomLeft.x() + d_base, bottomLeft.y());

                painter->drawLine(topLeft, topRight);
                painter->drawLine(topRight, bottomRight);
                painter->drawLine(bottomRight, bottomLeft);
                painter->drawLine(bottomLeft, topLeft);
            }
        } else {
            const auto snap = [&snapper, penWidth](const QPointF &point) {
                return snapper.snapForPen(point, penWidth);
            };
            const auto drawSegment = [painter, &snap](const QPointF &start, const QPointF &end) {
                painter->drawLine(snap(start), snap(end));
            };
            const auto drawOutline = [&drawSegment](qreal left, qreal top, qreal right, qreal bottom) {
                const QPointF topLeft(left, top);
                const QPointF topRight(right, top);
                const QPointF bottomRight(right, bottom);
                const QPointF bottomLeft(left, bottom);

                drawSegment(topLeft, topRight);
                drawSegment(topRight, bottomRight);
                drawSegment(bottomRight, bottomLeft);
                drawSegment(bottomLeft, topLeft);
            };

            if (button->isChecked()) {
                const qreal offset = penScale * 1.5;

                drawOutline(-5.0, -5.0 + offset, 5.0 - offset, 5.0);

                drawSegment(QPointF(-5.0 + offset, -5.0 + offset), QPointF(-5.0 + offset, -5.0));
                drawSegment(QPointF(-5.0 + offset, -5.0), QPointF(5.0, -5.0));
                drawSegment(QPointF(5.0, -5.0), QPointF(5.0, 5.0 - offset));
                drawSegment(QPointF(5.0, 5.0 - offset), QPointF(5.0 - offset, 5.0 - offset));
            } else {
                drawOutline(-5.0, -5.0, 5.0, 5.0);
            }
        }
        
        button->setPenWidth(painter, penScale, false); // reset to default
    }
};

} // namespace Material
