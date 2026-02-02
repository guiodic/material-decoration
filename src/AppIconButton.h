/*
 * Copyright (C) 2020 Chris Holland <zrenfire@gmail.com>
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
#include "Decoration.h"

// KDecoration
#include <KDecoration3/DecoratedWindow>

// KF
#include <KIconLoader>

// Qt
#include <QPainter>
#include <QPalette>

namespace Material
{

class AppIconButton
{

public:
    static void init(Button *button, KDecoration3::DecoratedWindow *decoratedClient) {
        QObject::connect(decoratedClient, &KDecoration3::DecoratedWindow::iconChanged,
            button, [button] {
                button->update();
            }
        );
    }
    
    static void paintIcon(Button *button, QPainter *painter, const QRectF &iconRect, const qreal) {
        //const QRectF contentRect = button->contentArea();
        const QSizeF appIconSize(
            iconRect.width() * 0.7,
            iconRect.height() * 0.7
        );
        QRectF appIconRect(QPointF(0,0), appIconSize);
        appIconRect.moveCenter(iconRect.center());

        const auto *deco = qobject_cast<Decoration *>(button->decoration());
        auto *decoratedClient = deco->window();

        const QPalette activePalette = KIconLoader::global()->customPalette();
        QPalette palette = decoratedClient->palette();
        palette.setColor(QPalette::WindowText, deco->titleBarForegroundColor());
        KIconLoader::global()->setCustomPalette(palette);
        decoratedClient->icon().paint(painter, appIconRect.toAlignedRect());
        if (activePalette == QPalette()) {
            KIconLoader::global()->resetPalette();
        } else {
            KIconLoader::global()->setCustomPalette(activePalette);
        }
    }
};

} // namespace Material
