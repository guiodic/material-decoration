// SPDX-License-Identifier: GPL-2.0-or-later
#include "MenuMnemonics.h"

#include <KAcceleratorManager>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QScopedValueRollback>
#include <QTimer>

namespace Material
{
namespace
{
class MenuMnemonics : public QObject
{
public:
    explicit MenuMnemonics(QMenu *menu)
        : QObject(menu)
        , m_menu(menu)
    {
        m_timer.setSingleShot(true);
        m_timer.setInterval(0);
        connect(&m_timer, &QTimer::timeout, this, &MenuMnemonics::refresh);
        connect(menu, &QMenu::aboutToShow, this, &MenuMnemonics::refresh);
        menu->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (!m_refreshing && (event->type() == QEvent::ActionAdded
            || event->type() == QEvent::ActionRemoved || event->type() == QEvent::ActionChanged)
            && !m_timer.isActive()) {
            m_timer.start();
        }
        return false;
    }

private:
    void refresh()
    {
        m_timer.stop();
        if (m_refreshing) return;
        const QScopedValueRollback<bool> guard(m_refreshing, true);
        // KDE normally recalculates at aboutToShow, before asynchronous D-Bus
        // updates arrive. A temporary container refreshes the shared actions
        // through the public API without changing their ownership or IDs.
        QMenu container;
        container.addActions(m_menu->actions());
        KAcceleratorManager::manage(&container);
    }

    QMenu *const m_menu;
    QTimer m_timer;
    bool m_refreshing = false;
};
}

void manageMenuMnemonics(QMenu *menu)
{
    new MenuMnemonics(menu);
}

void synchronizeMenuMnemonicVisibility(bool altPressed)
{
    // The initial Alt press goes to the application, before our popup has focus.
    // Notify the style's application event filter through a passive receiver:
    // sending Alt to QMenu itself could close it or change keyboard navigation.
    QObject receiver;
    QKeyEvent event(altPressed ? QEvent::KeyPress : QEvent::KeyRelease,
                    Qt::Key_Alt, altPressed ? Qt::AltModifier : Qt::NoModifier);
    QCoreApplication::sendEvent(&receiver, &event);
}
}
