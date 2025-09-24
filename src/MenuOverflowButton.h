/*
 * Copyright (C) 2020 Chris Holland <zrenfire@gmail.com>
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
#include "AppMenuButton.h"

namespace Material
{

class Decoration;

class MenuOverflowButton : public AppMenuButton
{
    Q_OBJECT

public:
    MenuOverflowButton(Decoration *decoration, const int buttonIndex, QObject *parent = nullptr);
    ~MenuOverflowButton() override;

    void paintIcon(QPainter *painter, const QRectF &iconRect, const qreal) override;
};

} // namespace Material
