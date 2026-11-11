/* This file is part of the dbusmenu-qt library
    SPDX-FileCopyrightText: 2009 Canonical
    SPDX-FileContributor: Aurelien Gateau <aurelien.gateau@canonical.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#pragma once

// Qt
#include <QObject>

// STL
#include <memory>

class QAction;
class QDBusPendingCallWatcher;
class QIcon;
class QMenu;

class DBusMenuImporterPrivate;

/**
 * Imports com.canonical.dbusmenu items as QMenu and QAction objects on the
 * session bus. Menu contents are loaded asynchronously.
 */
class DBusMenuImporter : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates an importer for service and path, and queues the initial root
     * layout request. Construction does not wait for the remote menu.
     */
    DBusMenuImporter(const QString &service, const QString &path, QObject *parent = nullptr);

    ~DBusMenuImporter() override;

    /** Returns the currently imported action for id, or nullptr if not loaded. */
    QAction *actionForId(int id) const;

    /**
     * Returns the root menu, creating it if necessary without waiting for data.
     * The importer schedules this menu for deletion when it is destroyed.
     */
    QMenu *menu() const;
    /** Cancels an unresolved exporter activation request; menu loads may finish. */
    void cancelActivationRequest();

public Q_SLOTS:
    /**
     * Asynchronously refreshes the root menu before it is shown.
     * @see updateMenu(QMenu *)
     */
    void updateMenu();

    /**
     * Asynchronously refreshes an imported menu, notifying the exporter before
     * fetching its layout. Re-entrant requests for the same menu are coalesced;
     * later requests or layout signals can supersede an outstanding reply.
     * @see menuUpdated()
     */
    void updateMenu(QMenu *menu);

Q_SIGNALS:
    /**
     * Emitted when a current layout request completes, including errors and
     * rejected layouts that leave the menu unchanged. Also emitted for updates
     * initiated by exporter layout signals. Superseded replies are discarded
     * without emitting this signal, so it is not an acknowledgement per call.
     * @see updateMenu()
     */
    void menuUpdated(QMenu *);

    /**
     * Emitted when the exporter requests an item and its action is available.
     * Unknown items may require loading ancestor menus first. This signal does
     * not trigger the action; cancelled or unsuccessful searches do not emit it.
     */
    void actionActivationRequested(QAction *);

protected:
    /**
     * Creates a menu for the given parent, optionally using a host-specific type.
     * Used for the root and submenus; the default returns new QMenu(parent).
     */
    virtual QMenu *createMenu(QWidget *parent);

    /**
     * Resolves a named icon. The default uses QIcon::fromTheme(name), returning
     * a null icon when the theme cannot resolve the name.
     */
    virtual QIcon iconForName(const QString &);

private Q_SLOTS:
    void sendClickedEvent(int);
    void slotActionHovered(QAction *);
    void slotMenuAboutToShow();
    void slotMenuAboutToHide();
    void slotAboutToShowDBusCallFinished(QDBusPendingCallWatcher *);
    void slotItemActivationRequested(int id, uint timestamp);
    void processPendingLayoutUpdates();
    void slotLayoutUpdated(uint revision, int parentId);
    void slotGetLayoutFinished(QDBusPendingCallWatcher *);

private:
    void retryEmptySubMenu(QMenu *menu);
    Q_DISABLE_COPY(DBusMenuImporter)
    std::unique_ptr<DBusMenuImporterPrivate> const d;
    friend class DBusMenuImporterPrivate;
};
