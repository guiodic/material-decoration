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

#include "NavigableMenu.h"
#include "AltKeyMonitor.h"
#include "Decoration.h"
#include "MenuMnemonics.h"

#include <KDecoration3/DecoratedWindow>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QTimer>

namespace Material
{

NavigableMenu::NavigableMenu(Decoration *decoration, QWidget *parent)
    : QMenu(parent)
    , m_decoration(decoration)
{
    // KWin exposes the Qt window title as caption to the companion script.
    setWindowTitle(QStringLiteral("com.github.guiodic.material-decoration.popup"));
    connect(AltKeyMonitor::instance(), &AltKeyMonitor::altPressedChanged, this, [this](bool pressed) {
        if (isVisible()) synchronizeMenuMnemonicVisibility(pressed);
    });
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this] {
        // Run after the style handles activation, which can clear its Alt state.
        QTimer::singleShot(0, this, [this] {
            if (isVisible()) synchronizeMenuMnemonicVisibility(AltKeyMonitor::instance()->isAltPressed());
        });
    });
}

void NavigableMenu::showEvent(QShowEvent *event)
{
    // An overflow action can present a submenu owned by a different menu.
    // Read the decoration palette directly instead of inheriting its parent.
    setPalette(m_decoration && m_decoration->m_internalSettings->menusFollowDecorationColors()
        ? m_decoration->window()->palette() : QPalette());
    QMenu::showEvent(event);
    synchronizeMenuMnemonicVisibility(AltKeyMonitor::instance()->isAltPressed());
}

void NavigableMenu::keyPressEvent(QKeyEvent *event)
{
    // First, let the base class handle standard key presses (like up, down, enter).
    QMenu::keyPressEvent(event);

    // If the event was not accepted by the base class, check for our custom keys.
    if (!event->isAccepted()) {
        bool isRtl = QGuiApplication::layoutDirection() == Qt::RightToLeft;
        const Qt::Key leftKey = isRtl ? Qt::Key_Right : Qt::Key_Left;
        const Qt::Key rightKey = isRtl ? Qt::Key_Left : Qt::Key_Right;

        const bool isLeft = (event->key() == leftKey);
        const bool isRight = (event->key() == rightKey);

        if ((isLeft || isRight) && event->modifiers() == Qt::NoModifier) {
            // Do not navigate away if a submenu is open
            if (activeAction() && activeAction()->menu() && activeAction()->menu()->isVisible()) {
                return;
            }

            if (isLeft) {
                Q_EMIT hitLeft();
            } else {
                Q_EMIT hitRight();
            }
            event->accept();
        }
    }
}

} // namespace Material
