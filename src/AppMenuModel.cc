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

// KF
#include <KWindowSystem>
// In KF5 5.101, KWindowSystem moved several signals to KX11Extras
// Eg: https://invent.kde.org/frameworks/kwindowsystem/-/commit/7cfd7c36eb017242d7a0202db82895be6b8fb81c
#if HAVE_KF5_101 // KX11Extras
#include <KX11Extras>
#endif

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

#if HAVE_X11
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <private/qtx11extras_p.h>
#else
#include <QX11Info>
#endif
#include <xcb/xcb.h>
#endif


namespace Material
{

static const QByteArray s_x11AppMenuServiceNamePropertyName = QByteArrayLiteral("_KDE_NET_WM_APPMENU_SERVICE_NAME");
static const QByteArray s_x11AppMenuObjectPathPropertyName = QByteArrayLiteral("_KDE_NET_WM_APPMENU_OBJECT_PATH");

#if HAVE_X11
static QHash<QByteArray, xcb_atom_t> s_atoms;
#endif

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
    if (KWindowSystem::isPlatformX11()) {
#if HAVE_X11
        x11Init();
#else
        // Not compiled with X11
        return;
#endif

    } else if (KWindowSystem::isPlatformWayland()) {
#if HAVE_Wayland
        // TODO
        // waylandInit();
        return;
#else
        // Not compiled with KWayland
        return;
#endif

    } else {
        // Not X11 or Wayland
        return;
    }

    m_serviceWatcher->setConnection(QDBusConnection::sessionBus());
    // If our current DBus connection gets lost, close the menu
    // we'll select the new menu when the focus changes
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString & serviceName) {
        if (serviceName == m_serviceName) {
            setMenuAvailable(false);
            emit modelNeedsUpdate();
        }
    });

    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(200); // 200ms debounce interval
    connect(m_updateTimer, &QTimer::timeout, this, &AppMenuModel::performMenuUpdate);

    m_deepCacheTimer = new QTimer(this);
    m_deepCacheTimer->setSingleShot(true);
    m_deepCacheTimer->setInterval(600);
    connect(m_deepCacheTimer, &QTimer::timeout, this, &AppMenuModel::doDeepCaching);
}

AppMenuModel::~AppMenuModel() = default;

void AppMenuModel::x11Init()
{
#if HAVE_X11
    connect(this, &AppMenuModel::winIdChanged,
            this, &AppMenuModel::onWinIdChanged);

// In KF5 5.101, KWindowSystem moved several signals to KX11Extras
// Eg: https://invent.kde.org/frameworks/kwindowsystem/-/commit/7cfd7c36eb017242d7a0202db82895be6b8fb81c


#if HAVE_KF5_101 // KX11Extras
    // Select non-deprecated overloaded method. Uses coding pattern from:
    // https://invent.kde.org/plasma/plasma-workspace/blame/master/libtaskmanager/xwindowsystemeventbatcher.cpp#L30
    /* UNUSED
    void (KX11Extras::*myWindowChangeSignal)(WId window, NET::Properties properties, NET::Properties2 properties2) = &KX11Extras::windowChanged;
    connect(KX11Extras::self(), myWindowChangeSignal,
            this, &AppMenuModel::onX11WindowChanged);
    */        

    // There are apps that are not releasing their menu properly after closing
    // and as such their menu is still shown even though the app does not exist
    // any more. Such apps are Java based e.g. smartgit
    connect(KX11Extras::self(), &KX11Extras::windowRemoved,
            this, &AppMenuModel::onX11WindowRemoved);
#else // KF5 5.100 KWindowSystem
    /* UNUSED
    void (KWindowSystem::*myWindowChangeSignal)(WId window, NET::Properties properties, NET::Properties2 properties2) = &KWindowSystem::windowChanged;
    connect(KWindowSystem::self(), myWindowChangeSignal,
            this, &AppMenuModel::onX11WindowChanged);
    */
    connect(KWindowSystem::self(), &KWindowSystem::windowRemoved,
            this, &AppMenuModel::onX11WindowRemoved);
#endif
    


    connect(this, &AppMenuModel::modelNeedsUpdate, this, [this] {
        if (!m_updatePending) {
            m_updatePending = true;
            QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
        }
    });
#endif
}

void AppMenuModel::waylandInit()
{
#if HAVE_Wayland
    // TODO
#endif
}

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

QVariant AppMenuModel::winId() const
{
    return m_winId;
}

void AppMenuModel::setWinId(const QVariant &id)
{
    if (m_winId == id) {
        return;
    }
    // qCDebug(category) << "AppMenuModel::setWinId" << m_winId << " => " << id;
    m_winId = id;
    emit winIdChanged();
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
    // qCDebug(category) << "AppMenuModel::update (" << m_winId << ")";
    beginResetModel();
    endResetModel();
    m_updatePending = false;
}


void AppMenuModel::onWinIdChanged()
{
    if (KWindowSystem::isPlatformX11()) {
#if HAVE_X11
        qApp->removeNativeEventFilter(this);

        const WId id = m_winId.toUInt();
        if (!id) {
            setMenuAvailable(false);
            emit modelNeedsUpdate();
            return;
        }

        auto updateMenuFromWindowIfHasMenu = [this](WId id) {
            const QString serviceName = QString::fromUtf8(x11GetWindowProperty(id, s_x11AppMenuServiceNamePropertyName));
            const QString menuObjectPath = QString::fromUtf8(x11GetWindowProperty(id, s_x11AppMenuObjectPathPropertyName));

            if (!serviceName.isEmpty() && !menuObjectPath.isEmpty()) {
                updateApplicationMenu(serviceName, menuObjectPath);

                // ALWAYS monitor LibreOffice for menu changes, as the object path can change
                // when a new document is opened from the startcenter.
                const QString winClass = QString::fromUtf8(x11GetWindowProperty(id, "WM_CLASS"));
                if ((winClass.contains ("soffice.bin")) || (winClass.contains ("libreoffice")))   {
                       qApp->installNativeEventFilter(this);
                       m_delayedMenuWindowId = id;
                }
                return true;
            }

            return false;
        };

        if (updateMenuFromWindowIfHasMenu(id)) {
            return;
        }

        // monitor whether an app menu becomes available later
        // this can happen when an app starts, shows its window, and only later announces global menu (e.g. Firefox)
        qApp->installNativeEventFilter(this);
        m_delayedMenuWindowId = id;

        //no menu found, set it to unavailable
        setMenuAvailable(false);
        emit modelNeedsUpdate();
#endif

    } else if (KWindowSystem::isPlatformWayland()) {
#if HAVE_Wayland
        // TODO
#endif
    }
}

//TODO This could be removed using new KDecoration3 APIs:
// 
// applicationMenuActive
// applicationMenuObjectPath
// applicationMenuServiceName
// hasApplicationMenu
//
// Also the position could be get from Positioner and the related popup() method

QByteArray AppMenuModel::x11GetWindowProperty(WId id, const QByteArray &name)
{
    QByteArray value;

#if HAVE_X11
    auto *c = QX11Info::connection();

    if (!s_atoms.contains(name))
    {
        const xcb_intern_atom_cookie_t atomCookie = xcb_intern_atom(c, false, name.length(), name.constData());
        QScopedPointer<xcb_intern_atom_reply_t, QScopedPointerPodDeleter> atomReply(xcb_intern_atom_reply(c, atomCookie, nullptr));

        if (atomReply.isNull()) {
            return value;
        }

        s_atoms[name] = atomReply->atom;

        if (s_atoms[name] == XCB_ATOM_NONE) {
            return value;
        }
    }

    static const long MAX_PROP_SIZE = 10000;
    auto propertyCookie = xcb_get_property(c, false, id, s_atoms[name], XCB_ATOM_STRING, 0, MAX_PROP_SIZE);
    QScopedPointer<xcb_get_property_reply_t, QScopedPointerPodDeleter> propertyReply(xcb_get_property_reply(c, propertyCookie, nullptr));

    if (propertyReply.isNull())
    {
        return value;
    }

    if (propertyReply->type == XCB_ATOM_STRING && propertyReply->format == 8 && propertyReply->value_len > 0)
    {
        const char *data = (const char *) xcb_get_property_value(propertyReply.data());
        int len = propertyReply->value_len;

        if (data) {
            value = QByteArray(data, data[len - 1] ? len : len - 1);
        }
    }
#endif

    return value;
}

/* UNUSED, remove
void AppMenuModel::onX11WindowChanged(WId id)
{
    if (m_winId.toUInt() == id) {
        
    }
}
*/

void AppMenuModel::onX11WindowRemoved(WId id)
{
    if (m_winId.toUInt() == id) {
        setMenuAvailable(false);
    }
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
    if (m_menu.isNull()) { // First time update, this is the main menu
        m_menu = menu;
        if (m_menu.isNull()) {
            return;
        }

        // Connect signals for top-level actions
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
        // 1. Fetch the first level of submenus immediately for UI responsiveness.
        fetchImmediateSubmenus(m_menu);
        // 2. Start a timer to fetch all deeper submenus later, to avoid startup jank.
        m_deepCacheTimer->start();

    } else { // This is a submenu update.
        if (m_deepCachingAllowed) {
            // If the delay has passed, continue caching recursively.
            cacheSubMenus(menu);
        }
        // Still need to track pending updates for the `menuReadyForSearch` signal.
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
    if (!menu) {
        return;
    }

    const auto actions = menu->actions();
    for (QAction *a : actions) {
        if (auto subMenu = a->menu()) {
            // This just fetches the immediate children, it does not recurse.
            m_importer->updateMenu(subMenu);
        }
    }
}

void AppMenuModel::doDeepCaching()
{
    // The timer has fired, now we can do the expensive recursive caching.
    m_deepCachingAllowed = true;
    // Kick off the deep scan from the root. This will find L1 menus that have already
    // arrived and process their children.
    cacheSubMenus(m_menu);
}

void AppMenuModel::cacheSubMenus(QMenu *menu)
{
    // This function is now the recursive part of the cache engine.
    // It is only called on a menu when its contents are available.
    // It finds all children of `menu` and triggers an update for them.
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

bool AppMenuModel::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result);

    if (!KWindowSystem::isPlatformX11() || eventType != "xcb_generic_event_t") {
        return false;
    }

#if HAVE_X11
    auto e = static_cast<xcb_generic_event_t *>(message);
    const uint8_t type = e->response_type & ~0x80;

    if (type == XCB_PROPERTY_NOTIFY) {
        auto *event = reinterpret_cast<xcb_property_notify_event_t *>(e);

        if (event->window == m_delayedMenuWindowId) {

            auto serviceNameAtom = s_atoms.value(s_x11AppMenuServiceNamePropertyName);
            auto objectPathAtom = s_atoms.value(s_x11AppMenuObjectPathPropertyName);

            if (serviceNameAtom != XCB_ATOM_NONE && objectPathAtom != XCB_ATOM_NONE) { // shouldn't happen
                if (event->atom == serviceNameAtom || event->atom == objectPathAtom) {
                    // A property has changed, which could be part of a storm of events.
                    // Start (or restart) a timer to handle the update once the storm subsides.
                    m_updateTimer->start();
                }
            }
        }
    }

#else
    Q_UNUSED(message);
#endif

    return false;
}

void AppMenuModel::performMenuUpdate()
{
    // This check is important to avoid doing work if the window is no longer valid
    if (m_delayedMenuWindowId == 0) {
        return;
    }
    const QString serviceName = QString::fromUtf8(x11GetWindowProperty(m_delayedMenuWindowId, s_x11AppMenuServiceNamePropertyName));
    const QString menuObjectPath = QString::fromUtf8(x11GetWindowProperty(m_delayedMenuWindowId, s_x11AppMenuObjectPathPropertyName));
    updateApplicationMenu(serviceName, menuObjectPath);
}

} // namespace Material
