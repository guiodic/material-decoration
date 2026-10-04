/*
 * Copyright (C) 2025 Guido Iodice <guido[dot]iodice[at]gmail.com>
 * Based on https://invent.kde.org/plasma/breeze/-/merge_requests/529/diffs
 *          Copyright (C) 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QMenu>

namespace Material
{

/**
 * @brief Subclass of QMenu supporting keyboard left/right arrow navigation between top-level menu categories.
 *
 * NavigableMenu emits hitLeft() or hitRight() when the user presses Left or Right arrow keys on a top-level menu
 * item without opening a submenu, allowing the AppMenuButtonGroup to cycle to adjacent application menus.
 */
class NavigableMenu : public QMenu
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a NavigableMenu instance.
     * @param parent Optional parent widget.
     */
    explicit NavigableMenu(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~NavigableMenu() override = default;

Q_SIGNALS:
    /**
     * @brief Emitted when logical navigation Left (RTL: Right) arrow key is pressed at top-level boundary.
     */
    void hitLeft();

    /**
     * @brief Emitted when logical navigation Right (RTL: Left) arrow key is pressed at top-level boundary.
     */
    void hitRight();

protected:
    /**
     * @brief Overridden key press event handler capturing Left/Right navigation.
     * @param event Key event.
     */
    void keyPressEvent(QKeyEvent *event) override;
};

} // namespace Material
