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
#include "Material.h"
#include "BuildConfig.h"
#include "NavigableMenu.h"

// Qt
#include <QAction>
#include <QDebug>
#include <QLoggingCategory>
#include <QMenu>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QGuiApplication>

// libdbusmenuqt
#include <dbusmenuimporter.h>

namespace Material
{

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
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString & serviceName) {
        if (serviceName == m_serviceName) {
            setMenuAvailable(false);
            emit modelNeedsUpdate();
        }
    });

    connect(this, &AppMenuModel::modelNeedsUpdate, this, [this] {
        if (!m_updatePending) {
            m_updatePending = true;
            QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
        }
    });

    m_staggerTimer = new QTimer(this);
    m_staggerTimer->setSingleShot(true);
    m_staggerTimer->setInterval(10);
    connect(m_staggerTimer, &QTimer::timeout, this, &AppMenuModel::processNext);
}

AppMenuModel::~AppMenuModel() = default;

bool AppMenuModel::menuAvailable() const
{
    return m_menuAvailable;
}

void AppMenuModel::setMenuAvailable(bool set)
{
    if (m_menuAvailable != set) {
        m_menuAvailable = set;
        emit menuAvailableChanged();
    }
}

QMenu *AppMenuModel::menu() const
{
    return m_menu;
}

void AppMenuModel::update()
{
    emit modelReset();
    m_updatePending = false;
}

void AppMenuModel::updateApplicationMenu(const QString &serviceName, const QString &menuObjectPath)
{
    if (m_serviceName == serviceName && m_menuObjectPath == menuObjectPath) {
        if (m_importer) {
            QMetaObject::invokeMethod(m_importer, "updateMenu", Qt::QueuedConnection);
        }
        return;
    }

    // A menu change means any in-progress caching is now invalid.
    stopCaching();
    m_pendingMenuUpdates = 0;

    m_serviceName = serviceName;
    m_serviceWatcher->setWatchedServices(QStringList({m_serviceName}));

    m_menuObjectPath = menuObjectPath;

    if (m_importer) {
        m_importer->deleteLater();
    }

    m_importer = new KDBusMenuImporter(serviceName, menuObjectPath, this);
    QMetaObject::invokeMethod(m_importer, "updateMenu", Qt::QueuedConnection);

    connect(m_importer.data(), &DBusMenuImporter::menuUpdated, this, &AppMenuModel::onMenuUpdated);
}

void AppMenuModel::onMenuUpdated(QMenu *menu)
{
    // This slot is called by the DBusMenuImporter whenever a menu's contents are ready.
    if (m_menu.isNull()) { // First time update, this is the root application menu.
        m_menu = menu;
        if (m_menu.isNull()) {
            return;
        }

        // Connect signals for top-level actions to update the model when they change.
        const auto actions = m_menu->actions();
        for (QAction *a : actions) {
            connect(a, &QAction::destroyed, this, &AppMenuModel::modelNeedsUpdate);
        }

        setMenuAvailable(true);
        emit modelNeedsUpdate();

        // Pre-fetching and deep caching are now handled on-demand.
    } else { // This is an update for a submenu that was previously requested.
        emit subMenuReady(menu);
        const bool wasQueueEmpty = m_menusToDeepCache.isEmpty();
        if (m_isCachingSubtree || m_isCachingEverything) {
            // The deep caching phase has begun. Add the children of the newly-loaded
            // submenu to the processing queue.
            const auto actions = menu->actions();
             for (QAction *a : actions) {
                 if (auto subMenu = a->menu()) {
                     if (m_menusInQueue.contains(subMenu)) {
                         continue;
                     }
                     m_menusInQueue.insert(subMenu);
                     m_menusToDeepCache.append(QPointer(subMenu));
                 }
             }
        }

        // If the processing chain was idle and we've just added new items,
        // we need to kick-start it again.
        if (wasQueueEmpty && !m_menusToDeepCache.isEmpty()) {
            processNext();
        }

        // Track the number of pending submenu updates. When it reaches zero,
        // the entire menu tree has been fetched and is ready for searching.
        if (m_pendingMenuUpdates > 0) {
            m_pendingMenuUpdates--;
            if (m_pendingMenuUpdates == 0) {
                emit menuReadyForSearch();
            }
        }
    }
}

void AppMenuModel::loadSubMenu(QMenu *menu)
{
    if (m_importer && menu) {
        m_importer->updateMenu(menu);
    }
}

void AppMenuModel::cacheSubtree(QMenu *menu)
{
    if (!menu) {
        return;
    }

    m_isCachingSubtree = true;

    const bool wasQueueEmpty = m_menusToDeepCache.isEmpty();
    const auto actions = menu->actions();
    for (QAction *action : actions) {
        if (auto subMenu = action->menu()) {
            if (subMenu->actions().isEmpty()) {
                if (!m_menusInQueue.contains(subMenu)) {
                    m_menusInQueue.insert(subMenu);
                    m_menusToDeepCache.append(QPointer(subMenu));
                }
            }
        }
    }

    if (wasQueueEmpty && !m_menusToDeepCache.isEmpty()) {
        processNext();
    }
}

void AppMenuModel::stopCaching()
{
    if (!m_isCachingSubtree && !m_isCachingEverything) {
        return;
    }

    m_menusToDeepCache.clear();
    m_menusInQueue.clear();
    m_staggerTimer->stop();
    m_isCachingSubtree = false;
    m_isCachingEverything = false;
    m_deepCacheStarted = false;
}

void AppMenuModel::startDeepCaching()
{
    if (m_deepCacheStarted) {
        return;
    }
    m_deepCacheStarted = true;

    m_isCachingEverything = true;
    m_menusToDeepCache.clear();
    m_menusInQueue.clear();

    if (!m_menu) {
        return;
    }

    // Populate the queue with the first level of submenus.
    // The recursive loading will happen as each menu is processed.
    const auto actions = m_menu->actions();
    for (QAction *a : actions) {
        if (auto subMenu = a->menu()) {
             if (m_menusInQueue.contains(subMenu)) {
                 continue;
             }
            m_menusInQueue.insert(subMenu);
            m_menusToDeepCache.append(QPointer(subMenu));
        }
    }

    // Start processing the queue.
    processNext();
}

void AppMenuModel::processNext()
{
    if (m_menusToDeepCache.isEmpty()) {
        // We are done processing all submenus.
        m_isCachingSubtree = false;
        m_isCachingEverything = false;
        m_deepCacheStarted = false;
        // Note: m_pendingMenuUpdates might still be > 0 here if the last
        // few DBus calls are still in flight. The menuReadyForSearch signal
        // will be emitted correctly in onMenuUpdated when the last reply arrives.
        return;
    }

    QPointer<QMenu> menuToProcessPtr = m_menusToDeepCache.takeFirst();
    m_menusInQueue.remove(menuToProcessPtr.data());
    QMenu *menuToProcess = menuToProcessPtr.data();

    if (menuToProcess) {
        m_pendingMenuUpdates++;
        m_importer->updateMenu(menuToProcess);
    }

    // Schedule the next item to be processed. The 0ms delay yields to the
    // event loop, preventing the UI from freezing.
    m_staggerTimer->start();
}


} // namespace Material
