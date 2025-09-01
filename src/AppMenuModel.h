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
#include <QAbstractListModel>
#include <QAbstractNativeEventFilter>
#include <QAction>
#include <QDBusServiceWatcher>
#include <QMenu>
#include <QModelIndex>
#include <QPointer>
#include <QRect>
#include <QStringList>
#include <QTimer>


namespace Material
{

class KDBusMenuImporter;

class AppMenuModel : public QAbstractListModel, public QAbstractNativeEventFilter
{
    Q_OBJECT

    Q_PROPERTY(bool menuAvailable READ menuAvailable WRITE setMenuAvailable NOTIFY menuAvailableChanged)
    Q_PROPERTY(QVariant winId READ winId WRITE setWinId NOTIFY winIdChanged)

public:
    explicit AppMenuModel(QObject *parent = nullptr);
    ~AppMenuModel() override;

private:
    void x11Init();
    void waylandInit();

public:
    enum AppMenuRole
    {
        MenuRole = Qt::UserRole + 1, // TODO this should be Qt::DisplayRole
        ActionRole
    };

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    void updateApplicationMenu(const QString &serviceName, const QString &menuObjectPath);

    bool menuAvailable() const;
    void setMenuAvailable(bool set);

    QVariant winId() const;
    void setWinId(const QVariant &id);

    QMenu *menu() const;

signals:
    void requestActivateIndex(int index);

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private Q_SLOTS:
    void onWinIdChanged();
    // void onX11WindowChanged(WId id); // UNUSED, remove 
    void onX11WindowRemoved(WId id);

    void update();

signals:
    void menuAvailableChanged();
    void modelNeedsUpdate();
    void winIdChanged();
    void menuReadyForSearch();

private Q_SLOTS:
    void onMenuUpdated(QMenu *menu);
    void performMenuUpdate();
    void doDeepCaching();

private:
    QTimer *m_updateTimer;
    QTimer *m_deepCacheTimer;
    bool m_menuAvailable;
    bool m_deepCachingAllowed = false;
    int m_pendingMenuUpdates = 0;
    bool m_updatePending = false;

    QVariant m_winId{-1};

    //! window that its menu initialization may be delayed
    WId m_delayedMenuWindowId = 0;

    QPointer<QMenu> m_menu;

    QDBusServiceWatcher *m_serviceWatcher;
    QString m_serviceName;
    QString m_menuObjectPath;

    QPointer<KDBusMenuImporter> m_importer;

    void fetchImmediateSubmenus(QMenu *menu);
    void cacheSubMenus(QMenu *menu);
    QByteArray x11GetWindowProperty(WId id, const QByteArray &name);
};

} // namespace Material
