/*
 * * Copyright (C) 2025 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
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

#include "AppMenuButton.h"

namespace Material
{

class Decoration;

/**
 * @brief Search button displayed alongside application menu buttons to trigger menu action search UI.
 */
class SearchButton : public AppMenuButton
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a SearchButton instance.
     * @param decoration Pointer to parent Decoration.
     * @param buttonIndex Index in AppMenuButtonGroup.
     * @param parent Optional parent object.
     */
    explicit SearchButton(Decoration *decoration, const int buttonIndex, QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~SearchButton() override;

protected:
    /**
     * @brief Paints the magnifying glass search icon.
     * @param painter QPainter instance.
     * @param iconRect Drawing bounding box.
     * @param snapper PixelSnapper helper.
     */
    void paintIcon(QPainter *painter, const QRectF &iconRect, const PixelSnapper &snapper) override;
};

} // namespace Material
