/******************************************************************
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

// Qt
#include <QAction>
#include <QDebug>
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

};

AppMenuModel::AppMenuModel(QObject *parent)
    : QAbstractListModel(parent),
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

    m_deepCacheTimer = new QTimer(this);
    m_deepCacheTimer->setSingleShot(true);
    m_deepCacheTimer->setInterval(600);
    connect(m_deepCacheTimer, &QTimer::timeout, this, &AppMenuModel::doDeepCaching);
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

int AppMenuModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    if (!m_menuAvailable || !m_menu) {
        return 0;
    }

    return m_menu->actions().count();
}

void AppMenuModel::update()
{
    beginResetModel();
    endResetModel();
    m_updatePending = false;
}

QHash<int, QByteArray> AppMenuModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[MenuRole] = QByteArrayLiteral("activeMenu");
    roleNames[ActionRole] = QByteArrayLiteral("activeActions");
    return roleNames;
}

QVariant AppMenuModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();

    if (row < 0 || !m_menuAvailable || !m_menu) {
        return QVariant();
    }

    const auto actions = m_menu->actions();

    if (row >= actions.count()) {
        return QVariant();
    }

    if (role == MenuRole) { // TODO this should be Qt::DisplayRole
        return actions.at(row)->text();
    } else if (role == ActionRole) {
        return QVariant::fromValue((void *) actions.at(row));
    }

    return QVariant();
}

void AppMenuModel::updateApplicationMenu(const QString &serviceName, const QString &menuObjectPath)
{
    if (m_serviceName == serviceName && m_menuObjectPath == menuObjectPath) {
        if (m_importer) {
            QMetaObject::invokeMethod(m_importer, "updateMenu", Qt::QueuedConnection);
        }
        return;
    }

    m_serviceName = serviceName;
    m_serviceWatcher->setWatchedServices(QStringList({m_serviceName}));

    m_menuObjectPath = menuObjectPath;

    if (m_importer) {
        m_importer->deleteLater();
    }

    m_importer = new KDBusMenuImporter(serviceName, menuObjectPath, this);
    QMetaObject::invokeMethod(m_importer, "updateMenu", Qt::QueuedConnection);

    connect(m_importer.data(), &DBusMenuImporter::menuUpdated, this, &AppMenuModel::onMenuUpdated);

    connect(m_importer.data(), &DBusMenuImporter::actionActivationRequested, this, [this](QAction *action) {
        // TODO submenus
        if (!m_menuAvailable || !m_menu) {
            return;
        }

        const auto actions = m_menu->actions();
        auto it = std::find(actions.begin(), actions.end(), action);
        if (it != actions.end()) {
            emit requestActivateIndex(it - actions.begin());
        }
    });
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
            connect(a, &QAction::changed, this, [this, a] {
                if (m_menuAvailable && m_menu) {
                    const int actionIdx = m_menu->actions().indexOf(a);
                    if (actionIdx > -1) {
                        const QModelIndex modelIdx = index(actionIdx, 0);
                        emit dataChanged(modelIdx, modelIdx);
                    }
                }
            });
            connect(a, &QAction::destroyed, this, &AppMenuModel::modelNeedsUpdate);
        }

        setMenuAvailable(true);
        emit modelNeedsUpdate();

        // --- Two-stage caching ---
        // To avoid UI freezes on startup, we don't fetch the entire menu tree at once.
        // 1. Fetch the first level of submenus immediately for UI responsiveness.
        // fetchImmediateSubmenus(m_menu); // FIXME: This causes a CPU spike on startup.
        // 2. Start a timer to fetch all deeper submenus later, to avoid startup jank.
        m_deepCacheTimer->start();

    } else { // This is an update for a submenu that was previously requested.
        if (m_deepCachingAllowed) {
            // The deep caching phase has begun. Recursively fetch the children of this submenu.
            cacheSubMenus(menu);
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

void AppMenuModel::fetchImmediateSubmenus(QMenu *menu)
{
    // Stage 1 of caching: Fetch only the direct children of the root menu.
    // This is a non-recursive call to quickly populate the top-level menus.
    if (!menu) {
        return;
    }

    const auto actions = menu->actions();
    for (QAction *a : actions) {
        if (auto subMenu = a->menu()) {
            m_importer->updateMenu(subMenu);
        }
    }
}

void AppMenuModel::doDeepCaching()
{
    // Stage 2 of caching begins now that the initial delay has passed.
    // This allows expensive, recursive fetching of the entire menu tree.
    m_deepCachingAllowed = true;

    // Kick off the deep scan from the root menu. `onMenuUpdated` will handle the recursion
    // for submenus that have already been fetched and are waiting.
    cacheSubMenus(m_menu);
}

void AppMenuModel::cacheSubMenus(QMenu *menu)
{
    // This function is the recursive part of the cache engine.
    // It is only called on a menu when its own contents are available.
    // It finds all children of `menu` and triggers a DBus update for them.
    // The recursion continues when `onMenuUpdated` is called for those children.
    if (!menu) {
        return;
    }

    const auto actions = menu->actions();
    for (QAction *a : actions) {
        if (auto subMenu = a->menu()) {
            m_pendingMenuUpdates++;
            m_importer->updateMenu(subMenu);
        }
    }
}

} // namespace Material
