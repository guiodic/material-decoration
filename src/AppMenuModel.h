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

namespace Material
{

class KDBusMenuImporter;

class AppMenuModel : public QObject
{
    Q_OBJECT

public:
    explicit AppMenuModel(QObject *parent = nullptr);
    ~AppMenuModel() override;

public:
    void updateApplicationMenu(const QString &serviceName, const QString &menuObjectPath);

    bool menuAvailable() const;
    void setMenuAvailable(bool set);

    QMenu *menu() const;

private Q_SLOTS:
    void update();

signals:
    void menuAvailableChanged();
    void modelNeedsUpdate();
    void modelReset();
    void menuReadyForSearch();
    void subMenuReady(QMenu *menu);

public Q_SLOTS:
    void loadSubMenu(QMenu *menu);
    void cacheSubtree(QMenu *menu);
    void stopCaching();

public Q_SLOTS:
    void startDeepCaching();

private Q_SLOTS:
    void onMenuUpdated(QMenu *menu);
    void processNext();

private:
    QTimer *m_staggerTimer;
    QList<QPointer<QMenu>> m_menusToDeepCache;
    QSet<QMenu *> m_menusInQueue;
    bool m_menuAvailable;
    bool m_isCachingSubtree = false;
    bool m_isCachingEverything = false;
    bool m_deepCacheStarted = false;
    int m_pendingMenuUpdates = 0;
    bool m_updatePending = false;

    QPointer<QMenu> m_menu;

    QDBusServiceWatcher *m_serviceWatcher;
    QString m_serviceName;
    QString m_menuObjectPath;

    QPointer<KDBusMenuImporter> m_importer;
};

} // namespace Material
