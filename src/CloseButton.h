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
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, const qreal gridUnit) {
        Q_UNUSED(button)
        Q_UNUSED(gridUnit)

        painter->setRenderHints(QPainter::Antialiasing, true);

        button->setPenWidth(painter, gridUnit, 1.10);

        painter->drawLine(iconRect.topLeft(), iconRect.bottomRight());
        painter->drawLine(iconRect.topRight(), iconRect.bottomLeft());
    }
};

} // namespace Material
