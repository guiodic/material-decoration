/******************************************************************
 * Copyright 2025 Guido Iodice <guido.iodice@gmail.com>
 * Copyright 2016 Kai Uwe Broulik <kde@privat.broulik.de>
 * Copyright 2016 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>
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
 *
 ******************************************************************/

// Based on:
// https://invent.kde.org/plasma/plasma-workspace/-/blob/master/applets/appmenu/plugin/appmenumodel.cpp
// https://github.com/psifidotos/applet-window-appmenu/blob/master/plugin/appmenumodel.cpp

// own
#include "AppMenuModel.h"
#include "NavigableMenu.h"

// Qt
#include <QAction>
#include <QMenu>
#include <QDeadlineTimer>

#include <QDBusConnection>
#include <QDBusServiceWatcher>

// libdbusmenuqt
#include <dbusmenuimporter.h>

//std
#include <utility>

namespace Material
{

namespace
{
// Bound each deep-cache request so an unresponsive D-Bus menu cannot keep
// search initialization waiting indefinitely. Five seconds allows for normal
// asynchronous menu updates while keeping a failed request from stalling search.
constexpr int DeepCacheUpdateTimeoutMs = 5000;
}

class KDBusMenuImporter : public DBusMenuImporter
{

public:
    KDBusMenuImporter(const QString &service, const QString &path, QObject *parent)
        : DBusMenuImporter(service, path, parent) {

    }

protected:
    QIcon iconForName(const QString &name) override {
        return QIcon::fromTheme(name);
    }

    QMenu *createMenu(QWidget *parent) override {
        return new NavigableMenu(parent);
    }
};

AppMenuModel::AppMenuModel(QObject *parent)
    : QObject(parent),
      m_serviceWatcher(new QDBusServiceWatcher(this))
{
    m_serviceWatcher->setConnection(QDBusConnection::sessionBus());
    // If our current DBus connection gets lost, close the menu
    // we'll select the new menu when the focus changes
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &serviceName) {
        if (serviceName == m_serviceName) {
            setMenuAvailable(false);
            stopCaching();
            for (QTimer *timer : std::as_const(m_userMenuWaitTimers)) {
                timer->stop();
                timer->deleteLater();
            }
            m_userMenuWaitTimers.clear();
            m_userMenuWaitRequestIds.clear();
            m_timedOutDeepCacheMenus.clear();
            m_deepCacheCompleted = false;
            m_menu = nullptr;
            m_serviceName.clear();
            m_menuObjectPath.clear();
            if (m_importer) {
                m_importer->deleteLater();
                m_importer = nullptr;
            }
            Q_EMIT modelNeedsUpdate();
        }
    });

    connect(this, &AppMenuModel::modelNeedsUpdate, this, [this] {
        if (!m_updatePending) {
            m_updatePending = true;
            QMetaObject::invokeMethod(this, &AppMenuModel::update, Qt::QueuedConnection);
        }
    });

    m_staggerTimer = new QTimer(this);
    m_staggerTimer->setSingleShot(true);
    m_staggerTimer->setInterval(8);
    connect(m_staggerTimer, &QTimer::timeout, this, &AppMenuModel::processNext);
}

AppMenuModel::~AppMenuModel()
{
    stopCaching();
    // QObjects owned by the importer may destroy their menus during QObject's
    // base destructor, after our members have already been destroyed. Delete
    // popup timers now so their destroyed-menu callbacks cannot access them.
    qDeleteAll(m_userMenuWaitTimers);
    m_userMenuWaitTimers.clear();
    m_userMenuWaitRequestIds.clear();
}

bool AppMenuModel::menuAvailable() const
{
    return m_menuAvailable;
}

void AppMenuModel::setMenuAvailable(bool set)
{
    if (m_menuAvailable != set) {
        m_menuAvailable = set;
        Q_EMIT menuAvailableChanged();
    }
}

QMenu *AppMenuModel::menu() const
{
    return m_menu;
}

void AppMenuModel::update()
{
    Q_EMIT modelReset();
    m_updatePending = false;
}

void AppMenuModel::updateApplicationMenu(const QString &serviceName, const QString &menuObjectPath)
{
    // An update for the same source is not a change of menu identity.
    // Preserve the outstanding recovery markers so late submenu updates can
    // still resume deep caching after the top-level refresh.
    if (m_serviceName == serviceName && m_menuObjectPath == menuObjectPath && m_importer) {
        QMetaObject::invokeMethod(m_importer.data(), qOverload<>(&DBusMenuImporter::updateMenu), Qt::QueuedConnection);
        return;
    }

    stopCaching();
    for (QTimer *timer : std::as_const(m_userMenuWaitTimers)) {
        timer->stop();
        timer->deleteLater();
    }
    m_userMenuWaitTimers.clear();
    m_userMenuWaitRequestIds.clear();
    m_timedOutDeepCacheMenus.clear();
    m_deepCacheCompleted = false;

    m_serviceName = serviceName;
    m_serviceWatcher->setWatchedServices({m_serviceName});

    m_menuObjectPath = menuObjectPath;
    m_menu = nullptr;

    if (m_importer) {
        m_importer->disconnect(this);
        m_importer->deleteLater();
    }

    m_importer = new KDBusMenuImporter(serviceName, menuObjectPath, this);
    QMetaObject::invokeMethod(m_importer.data(), qOverload<>(&DBusMenuImporter::updateMenu), Qt::QueuedConnection);

    connect(m_importer.data(), &DBusMenuImporter::menuUpdated, this, &AppMenuModel::onMenuUpdated);
    connect(m_importer.data(), &DBusMenuImporter::menuRequestFinished, this, &AppMenuModel::onMenuRequestFinished);

    Q_EMIT modelNeedsUpdate();
}

void AppMenuModel::onActionChanged()
{
    // This is called when a top-level action changes (e.g., text, icon, enabled state).
    // We emit modelNeedsUpdate to eventually trigger a modelReset, but we could
    // add more sophisticated filtering here if needed.
    Q_EMIT modelNeedsUpdate();
}

void AppMenuModel::onMenuUpdated(QMenu *menu)
{
    // An importer may emit a queued/late update after it has been replaced or
    // its DBus service has disappeared. Ignore updates from obsolete importers.
    if (sender() != m_importer.data()) {
        return;
    }

    if (!menu) {
        return;
    }

    // A LayoutUpdated notification may complete independently of the user's
    // AboutToShow request. Only menuRequestFinished can complete a popup wait.

    const QPointer<QMenu> menuGuard(menu);
    const QPointer<AppMenuModel> modelGuard(this);
    const QPointer<KDBusMenuImporter> importerGuard(m_importer);

    // This slot is called by the DBusMenuImporter whenever a menu's contents are ready.
    if (m_menu.isNull() || menu == m_menu.data()) { // First time update, or a top-level menu update.
        m_menu = menu;

        // Connect signals for top-level actions to update the model when they change.
        for (QAction *a : m_menu->actions()) {
            connect(a, &QAction::destroyed, this, &AppMenuModel::modelNeedsUpdate, Qt::UniqueConnection);
            connect(a, &QAction::changed, this, &AppMenuModel::onActionChanged, Qt::UniqueConnection);
        }

        setMenuAvailable(true);
        if (!modelGuard) {
            return;
        }
        Q_EMIT modelNeedsUpdate();

        // Emitting signals can synchronously replace the current importer.
        if (modelGuard && menuGuard && importerGuard == m_importer && m_deepCacheRequested) {
            resumeDeepCacheIfIdle(menuGuard.data());
        }
    } else { // This is an update for a submenu that was previously requested.
        if (!m_userMenuWaitTimers.contains(menu)) {
            Q_EMIT subMenuReady(menu);
        }
        if (!modelGuard || !menuGuard || importerGuard != m_importer) {
            return;
        }

        const bool timedOutMenu = takeTimedOutMenu(menu);
        if (m_deepCacheRequested) {
            resumeDeepCacheIfIdle(menuGuard.data());
        } else if (m_deepCacheCompleted && timedOutMenu) {
            // A retry after a timeout succeeded after the original search cache
            // had completed. Scan this subtree so its nested entries are indexed.
            startDeepCachingFrom(menu);
        }

    }
}

void AppMenuModel::onMenuRequestFinished(QMenu *menu, quint64 requestId, bool success)
{
    if (sender() != m_importer.data() || !menu || !requestId) {
        return;
    }
    // Finish internal bookkeeping before emitting any signal: popup slots
    // may synchronously call stopCaching() or replace the importer.
    if (m_pendingDeepCacheRequestIds.value(menu) == requestId) {
        if (!success) {
            addTimedOutMenu(menu);
        }
        checkPendingUpdatesComplete(menu);
    }

    if (m_userMenuWaitRequestIds.value(menu) == requestId) {
        m_userMenuWaitRequestIds.remove(menu);
        if (QTimer *timer = m_userMenuWaitTimers.take(menu)) {
            timer->stop();
            timer->deleteLater();
        }
        if (success) {
            Q_EMIT subMenuReady(menu);
        } else {
            Q_EMIT subMenuLoadFailed(menu);
        }
    }
}

void AppMenuModel::loadSubMenu(QMenu *menu)
{
    if (!m_importer || !menu) {
        return;
    }
    const quint64 requestId = m_importer->requestMenuUpdate(menu);
    if (!requestId) {
        Q_EMIT subMenuLoadFailed(menu);
        return;
    }
    if (!m_userMenuWaitTimers.contains(menu)) {
        auto *timer = new QTimer(this);
        timer->setSingleShot(true);
        const QPointer<QMenu> guard(menu);
        m_userMenuWaitTimers.insert(menu, timer);
        m_userMenuWaitRequestIds.insert(menu, requestId);
        // Use the timer as receiver: stopCaching() disconnects menu -> model
        // deep-cache callbacks, but must not remove this lifetime cleanup.
        connect(menu, &QObject::destroyed, timer, [this, menu]() {
            m_userMenuWaitRequestIds.remove(menu);
            if (QTimer *pending = m_userMenuWaitTimers.take(menu)) {
                pending->stop();
                pending->deleteLater();
            }
        });
        connect(timer, &QTimer::timeout, this, [this, menu, guard, timer, requestId]() {
            if (m_userMenuWaitTimers.value(menu) != timer) {
                return;
            }
            m_userMenuWaitTimers.remove(menu);
            m_userMenuWaitRequestIds.remove(menu);
            timer->deleteLater();
            if (guard) {
                // Deep caching may share this request. Expire it only if no
                // scanner is still awaiting the same operation.
                if (m_importer && m_pendingDeepCacheRequestIds.value(menu) != requestId) {
                    m_importer->expireMenuRequest(guard.data(), requestId);
                }
                Q_EMIT subMenuLoadFailed(guard.data());
            }
        });
        timer->start(DeepCacheUpdateTimeoutMs);
    }
}

void AppMenuModel::stopCaching()
{
    for (QMenu *subMenu : std::as_const(m_seenMenus)) {
        disconnect(subMenu, nullptr, this, nullptr);
    }
    m_seenMenus.clear();

    m_menusToDeepCache.clear();
    m_nextMenuToProcess = 0;
    m_staggerTimer->stop();
    for (QTimer *timer : std::as_const(m_pendingDeepCacheTimers)) {
        timer->stop();
        timer->deleteLater();
    }
    m_pendingDeepCacheTimers.clear();
    m_pendingDeepCacheRequestIds.clear();
    m_deepCacheRequested = false;
    m_deepCacheStarted = false;
    m_pendingDeepCacheUpdates.clear();
}

void AppMenuModel::startDeepCaching()
{
    if (m_deepCacheStarted) {
        return;
    }

    m_deepCacheRequested = true;
    m_deepCacheCompleted = false;

    if (!m_menu) {
        return;
    }

    m_deepCacheStarted = true;
    m_menusToDeepCache.clear();
    m_nextMenuToProcess = 0;
    m_seenMenus.clear();

    // Populate the queue with the first level of submenus.
    // The recursive loading will happen as each menu is processed.
    registerSubMenus(m_menu.data());

    // Start processing the queue.
    processNext();
}

void AppMenuModel::registerSubMenus(QMenu *menu)
{
    if (!menu) {
        return;
    }
    for (QAction *a : menu->actions()) {
        if (auto subMenu = a->menu()) {
            const auto oldSize = m_seenMenus.size();
            m_seenMenus.insert(subMenu);
            if (m_seenMenus.size() > oldSize) {
                connect(subMenu, &QObject::destroyed, this, [this, subMenu]() {
                    m_seenMenus.remove(subMenu);
                    takeTimedOutMenu(subMenu);
                    checkPendingUpdatesComplete(subMenu);
                });
                m_menusToDeepCache.append(QPointer(subMenu));
            }
        }
    }
}

void AppMenuModel::checkPendingUpdatesComplete(QMenu *menu)
{
    if (!m_pendingDeepCacheUpdates.remove(menu)) {
        return;
    }
    m_pendingDeepCacheRequestIds.remove(menu);

    if (QTimer *timer = m_pendingDeepCacheTimers.take(menu)) {
        timer->stop();
        timer->deleteLater();
    }

    if (m_pendingDeepCacheUpdates.isEmpty() && m_nextMenuToProcess >= m_menusToDeepCache.size()) {
        QMetaObject::invokeMethod(this, &AppMenuModel::processNext, Qt::QueuedConnection);
    }
}

void AppMenuModel::finishCaching()
{
    stopCaching();
    m_deepCacheCompleted = true;
    Q_EMIT menuReadyForSearch();
}

void AppMenuModel::addTimedOutMenu(QMenu *menu)
{
    if (!menu) {
        return;
    }

    for (auto it = m_timedOutDeepCacheMenus.begin(); it != m_timedOutDeepCacheMenus.end();) {
        if (it->isNull()) {
            it = m_timedOutDeepCacheMenus.erase(it);
        } else if (it->data() == menu) {
            return;
        } else {
            ++it;
        }
    }
    m_timedOutDeepCacheMenus.append(QPointer<QMenu>(menu));
}

bool AppMenuModel::takeTimedOutMenu(QMenu *menu)
{
    for (auto it = m_timedOutDeepCacheMenus.begin(); it != m_timedOutDeepCacheMenus.end();) {
        if (it->isNull()) {
            it = m_timedOutDeepCacheMenus.erase(it);
        } else if (it->data() == menu) {
            m_timedOutDeepCacheMenus.erase(it);
            return true;
        } else {
            ++it;
        }
    }
    return false;
}

void AppMenuModel::startDeepCachingFrom(QMenu *menu)
{
    if (!menu || !m_importer) {
        return;
    }

    m_deepCacheRequested = true;
    m_deepCacheStarted = true;
    m_deepCacheCompleted = false;
    m_menusToDeepCache.clear();
    m_nextMenuToProcess = 0;
    m_seenMenus.clear();

    registerSubMenus(menu);
    processNext();
}

void AppMenuModel::resumeDeepCacheIfIdle(QMenu *menu)
{
    if (!menu || !m_deepCacheRequested) {
        return;
    }

    const bool wasQueueFinished = (m_nextMenuToProcess >= m_menusToDeepCache.size());

    registerSubMenus(menu);

    if (wasQueueFinished) {
        m_deepCacheStarted = true;
        processNext();
    }
}

void AppMenuModel::processNext()
{
    if (!m_deepCacheRequested) {
        return;
    }

    if (m_nextMenuToProcess >= m_menusToDeepCache.size()) {
        if (!m_pendingDeepCacheUpdates.isEmpty()) {
            return; // Wait for pending updates to finish and potentially add more items
        }
        finishCaching();
        return;
    }

    QDeadlineTimer deadline(std::chrono::milliseconds(8));

    while (m_nextMenuToProcess < m_menusToDeepCache.size()) {
        QPointer<QMenu> menuToProcessPtr = m_menusToDeepCache.at(m_nextMenuToProcess++);
        QMenu *menuToProcess = menuToProcessPtr.data();

        if (menuToProcess) {
            if (!menuToProcess->actions().isEmpty()) {
                registerSubMenus(menuToProcess);
                if (deadline.hasExpired()) {
                    m_staggerTimer->start();
                    return;
                }
                continue; // Process next item immediately
            }

            if (!m_importer) {
                // The importer may have disappeared while caching was being scheduled.
                // Do not create a pending operation that can never complete.
                continue;
            }

            m_pendingDeepCacheUpdates.insert(menuToProcess);

            // The importer normally completes its requests asynchronously, but a
            // failed D-Bus call may never produce menuUpdated(). Bound the wait so
            // deep search can still finish with the menu data available so far.
            auto *timeout = new QTimer(this);
            timeout->setSingleShot(true);
            m_pendingDeepCacheTimers.insert(menuToProcess, timeout);
            const QPointer<QMenu> menuGuard(menuToProcess);
            connect(timeout, &QTimer::timeout, this, [this, menuGuard, menuKey = menuToProcess, timeout]() {
                if (m_pendingDeepCacheTimers.value(menuKey) != timeout) {
                    return;
                }

                // The menu may have been destroyed while the timer was pending.
                // Keep the raw pointer only as the hash key; never dereference it.
                if (!menuGuard) {
                    checkPendingUpdatesComplete(menuKey);
                    return;
                }

                // Stop waiting for deep caching, but do not cancel the shared
                // importer request: a user may also be waiting to open this menu,
                // and LayoutUpdated refreshes must remain independent.
                addTimedOutMenu(menuGuard.data());
                if (m_importer && !m_userMenuWaitTimers.contains(menuKey)) {
                    m_importer->expireMenuRequest(menuGuard.data(),
                        m_pendingDeepCacheRequestIds.value(menuKey));
                }
                checkPendingUpdatesComplete(menuKey);
            });
            timeout->start(DeepCacheUpdateTimeoutMs);

            const quint64 requestId = m_importer->requestMenuUpdate(menuToProcess);
            if (requestId) {
                m_pendingDeepCacheRequestIds.insert(menuToProcess, requestId);
            } else {
                // No request exists to complete this pending operation.
                checkPendingUpdatesComplete(menuToProcess);
            }
            m_staggerTimer->start();
            return; // Wait for async update or timeout
        }
    }

    if (!m_pendingDeepCacheUpdates.isEmpty()) {
        return;
    }

    finishCaching();
}


} // namespace Material
