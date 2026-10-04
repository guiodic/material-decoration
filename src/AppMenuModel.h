/******************************************************************
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

#pragma once

// Qt
#include <QObject>
#include <QAction>
#include <QDBusServiceWatcher>
#include <QMenu>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QtGlobal>

namespace Material
{

class KDBusMenuImporter;

/**
 * @brief Data model wrapping DBus menu importing (`KDBusMenuImporter`) for window application menus.
 *
 * AppMenuModel connects to the DBus menu exporter service published by target application windows,
 * deserializes top-level menus and submenus, manages background deep caching queues to pre-fetch menu
 * hierarchies for fast search indexing, and emits notifications when menus are updated or ready.
 */
class AppMenuModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an AppMenuModel instance.
     * @param parent Optional parent object.
     */
    explicit AppMenuModel(QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~AppMenuModel() override;

public:
    /**
     * @brief Sets target DBus service name and menu object path to import application menus from.
     * @param serviceName DBus service name exporting the menu (e.g., ":1.123").
     * @param menuObjectPath DBus object path (e.g., "/MenuBar").
     */
    void updateApplicationMenu(const QString &serviceName, const QString &menuObjectPath);

    /**
     * @brief Returns the root QMenu populated from DBus.
     * @return Pointer to root QMenu, or nullptr if unavailable.
     */
    QMenu *menu() const;

private:
    void update();

signals:
    /**
     * @brief Emitted when menu availability status changes.
     */
    void menuAvailableChanged();

    /**
     * @brief Emitted when the menu structure needs to be refreshed.
     */
    void modelNeedsUpdate();

    /**
     * @brief Emitted when the model is reset due to service disconnect or menu reload.
     */
    void modelReset();

    /**
     * @brief Emitted when menu structure loading/caching completes and search can process candidates.
     */
    void menuReadyForSearch();

    /**
     * @brief Emitted when a specific submenu finished loading over DBus.
     * @param menu Pointer to loaded QMenu.
     */
    void subMenuReady(QMenu *menu);

public:
    /**
     * @brief Requests loading of a specific submenu over DBus if not yet populated.
     * @param menu Target submenu to load.
     */
    void loadSubMenu(QMenu *menu);

    /**
     * @brief Halts active deep caching queue processing and resets background timers.
     */
    void stopCaching();

    /**
     * @brief Initiates background deep caching of all unpopulated submenus across the menu tree.
     */
    void startDeepCaching();

private Q_SLOTS:
    void onMenuUpdated(QMenu *menu);
    void onActionChanged();
    void processNext();

private:
    void registerSubMenus(QMenu *menu);
    void resumeDeepCacheIfIdle(QMenu *menu);
    bool menuAvailable() const;
    void setMenuAvailable(bool set);

    QTimer *m_staggerTimer;
    QList<QPointer<QMenu>> m_menusToDeepCache;
    qsizetype m_nextMenuToProcess = 0;
    QSet<QMenu *> m_seenMenus;
    bool m_menuAvailable;
    bool m_deepCacheRequested = false;
    bool m_deepCacheStarted = false;
    QSet<QMenu *> m_pendingDeepCacheUpdates;
    bool m_updatePending = false;

    QPointer<QMenu> m_menu;

    QDBusServiceWatcher *m_serviceWatcher;
    QString m_serviceName;
    QString m_menuObjectPath;

    QPointer<KDBusMenuImporter> m_importer;
};

} // namespace Material
