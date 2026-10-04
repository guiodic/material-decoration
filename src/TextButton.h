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
#include "AppMenuButton.h"

// Qt
#include <QAction>
#include <QPointer>

namespace Material
{

class Decoration;

/**
 * @brief Text button representing a top-level application menu category (File, Edit, View, Help, etc.).
 */
class TextButton : public AppMenuButton
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a TextButton instance.
     * @param decoration Pointer to parent Decoration.
     * @param buttonIndex Index of the button in AppMenuButtonGroup.
     * @param parent Optional parent object.
     */
    TextButton(Decoration *decoration, const int buttonIndex, QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~TextButton() override;

    Q_PROPERTY(QAction* action READ action WRITE setAction NOTIFY actionChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)

    /**
     * @brief Overridden paintIcon to render category text label using current font and colors.
     * @param painter QPainter instance.
     * @param iconRect Drawing area for label.
     * @param snapper PixelSnapper helper.
     */
    void paintIcon(QPainter *painter, const QRectF &iconRect, const PixelSnapper &snapper) override;

    /**
     * @brief Returns the top-level menu QAction associated with this category button.
     * @return Pointer to QAction.
     */
    QAction* action() const;

    /**
     * @brief Sets the top-level menu QAction associated with this category button.
     * @param set Pointer to QAction.
     */
    void setAction(QAction *set);

    /**
     * @brief Returns button display text.
     * @return Cleaned display label string.
     */
    QString text() const;

    /**
     * @brief Sets button display text and updates button size.
     * @param set Label text.
     */
    void setText(const QString &set);

    /**
     * @brief Adjusts button height and recalculates layout geometry.
     * @param buttonHeight New button height in local units.
     */
    void setHeight(qreal buttonHeight) override;

signals:
    /**
     * @brief Emitted when action property changes.
     */
    void actionChanged();

    /**
     * @brief Emitted when text property changes.
     */
    void textChanged();

private:
    QSizeF getTextSize() const;
    void updateGeometry();

    QPointer<QAction> m_action;
    QString m_text;
};

} // namespace Material
