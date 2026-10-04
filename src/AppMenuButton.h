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

namespace Material
{

class Decoration;

/**
 * @brief Base class for application menu buttons (category text buttons, overflow, search).
 */
class AppMenuButton : public Button
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an AppMenuButton.
     * @param decoration Pointer to parent Decoration.
     * @param buttonIndex Index of the button in AppMenuButtonGroup.
     * @param parent Optional parent object.
     */
    AppMenuButton(Decoration *decoration, const int buttonIndex, QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~AppMenuButton() override = default;

    Q_PROPERTY(int buttonIndex READ buttonIndex NOTIFY buttonIndexChanged)

    /**
     * @brief Returns the button index in AppMenuButtonGroup.
     * @return Button index.
     */
    int buttonIndex() const;

    /**
     * @brief Returns background color for button rendering.
     * @return Background color.
     */
    QColor backgroundColor() const override;

    /**
     * @brief Returns foreground color for button text and icons.
     * @return Foreground color.
     */
    QColor foregroundColor() const override;

signals:
    /**
     * @brief Emitted when button index property changes.
     */
    void buttonIndexChanged();

public slots:
    /**
     * @brief Slot invoked when the button is activated.
     */
    virtual void trigger();

private:
    int m_buttonIndex;
};

} // namespace Material
