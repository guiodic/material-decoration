/*
 * Copyright (C) 2020 Chris Holland <zrenfire@gmail.com>
 * Copyright (C) 2016 Kai Uwe Broulik <kde@privat.broulik.de>
 * Copyright (C) 2014 by Hugo Pereira Da Costa <hugo.pereira@free.fr>
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

// own
#include "TextButton.h"
#include "Material.h"
#include "AppMenuButton.h"
#include "Decoration.h"

// KDecoration
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/DecorationSettings>

// Qt
#include <QDebug>
#include <QAction>
#include <QFontMetrics>
#include <QPainter>


namespace Material
{

TextButton::TextButton(Decoration *decoration, const int buttonIndex, QObject *parent)
    : AppMenuButton(decoration, buttonIndex, parent)
    , m_action(nullptr)
    , m_text(QStringLiteral("Menu"))
{
    const auto *deco = qobject_cast<Decoration *>(decoration);

    setHorzPadding(deco->appMenuButtonHorzPadding());

    setVisible(true);
}

TextButton::~TextButton()
{
}

void TextButton::paintIcon(QPainter *painter, const QRectF &iconRect, const qreal)
{
    // Font
    painter->setFont(decoration()->settings()->font());
    painter->setPen(foregroundColor());
    painter->setRenderHint(QPainter::TextAntialiasing);
    painter->setOpacity(1.0);

    // TODO: Use Qt::TextShowMnemonic when Alt is pressed
    const bool isAltPressed = false;
    const Qt::TextFlag mnemonicFlag = isAltPressed ? Qt::TextShowMnemonic : Qt::TextHideMnemonic;
    painter->drawText(iconRect, mnemonicFlag | Qt::AlignCenter, m_text);
}

QSizeF TextButton::getTextSize() const
{
    const auto *deco = qobject_cast<const Decoration *>(decoration());
    if (!deco) {
        return QSizeF(0, 0);
    }

    // const QString elidedText = painter->fontMetrics().elidedText(
    //     m_text,
    //     Qt::ElideRight,
    //     100, // Max width TODO: scale by dpi
    // );
    const qreal textWidth = deco->getTextWidth(m_text);
    const qreal titleBarHeight = deco->titleBarHeight();
    const QSizeF size(textWidth, titleBarHeight);
    return size;
}

QAction* TextButton::action() const
{
    return m_action.data();
}

void TextButton::setAction(QAction *set)
{
    if (m_action != set) {
        m_action = set;
        Q_EMIT actionChanged();
    }
}

QString TextButton::text() const
{
    return m_text;
}

void TextButton::setText(const QString set)
{
    if (m_text != set) {
        m_text = set;
        Q_EMIT textChanged();

        updateGeometry();
    }
}

void TextButton::setHeight(qreal buttonHeight)
{
    Q_UNUSED(buttonHeight)

    updateGeometry();
}

void TextButton::updateGeometry()
{
    const QSizeF textSize = getTextSize();
    updateSize(textSize.width(), textSize.height());
}


} // namespace Material
