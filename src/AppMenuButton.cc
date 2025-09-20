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
#include "AppMenuButton.h"
#include "Material.h"
#include "Button.h"
#include "Decoration.h"
#include "AppMenuButtonGroup.h"

// KDecoration
#include <KDecoration3/DecoratedWindow>

// KF
#include <KColorUtils>

// Qt
#include <QDebug>


namespace Material
{

AppMenuButton::AppMenuButton(Decoration *decoration, const int buttonIndex, QObject *parent)
    : Button(KDecoration3::DecorationButtonType::Custom, decoration, parent)
    , m_buttonIndex(buttonIndex)
{
    setCheckable(true);

    connect(this, &AppMenuButton::clicked,
        this, &AppMenuButton::trigger);

    const auto *buttonGroup = qobject_cast<AppMenuButtonGroup *>(parent);
    if (buttonGroup) {
        setOpacity(buttonGroup->opacity());
    }
}

int AppMenuButton::buttonIndex() const
{
    return m_buttonIndex;
}

QColor AppMenuButton::backgroundColor() const
{
    const auto *buttonGroup = qobject_cast<AppMenuButtonGroup *>(parent());
    if (buttonGroup
        && buttonGroup->isMenuOpen()
        && buttonGroup->currentIndex() != m_buttonIndex
    ) {
        return Qt::transparent;
    } else {
        return Button::backgroundColor();
    }
}

QColor AppMenuButton::foregroundColor() const
{
    const auto *buttonGroup = qobject_cast<AppMenuButtonGroup *>(parent());
    if (buttonGroup
        && buttonGroup->isMenuOpen()
        && buttonGroup->currentIndex() != m_buttonIndex
    ) {
        const auto *deco = qobject_cast<Decoration *>(decoration());
        if (!deco) {
            return {};
        }
        return KColorUtils::mix(
            deco->titleBarBackgroundColor(),
            deco->titleBarForegroundColor(),
            0.8);
    } else {
        return Button::foregroundColor();
    }
}

void AppMenuButton::trigger() {
    qCDebug(category) << "AppMenuButton::trigger" << m_buttonIndex;

    auto *buttonGroup = qobject_cast<AppMenuButtonGroup *>(parent());
    buttonGroup->trigger(m_buttonIndex);
}

} // namespace Material
