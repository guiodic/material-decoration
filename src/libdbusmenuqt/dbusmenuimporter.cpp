/* This file is part of the dbusmenu-qt library
    SPDX-FileCopyrightText: 2009 Canonical
    SPDX-FileContributor: Aurelien Gateau <aurelien.gateau@canonical.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "dbusmenuimporter.h"

#include "debug.h"

// STL
#include <utility>
#include <functional>

// Qt
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDebug>
#include <QFont>
#include <QMenu>
#include <QKeyEvent>
#include <QPointer>
#include <QSet>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QWidgetAction>

// Local
#include "dbusmenushortcut_p.h"
#include "dbusmenutypes_p.h"
#include "utils_p.h"

// Generated
#include "dbusmenu_interface.h"

// #define BENCHMARK
#ifdef BENCHMARK
static QTime sChrono;
#endif

#define DMRETURN_IF_FAIL(cond)                                                                                                                                 \
    if (!(cond)) {                                                                                                                                             \
        qCWarning(DBUSMENUQT) << "Condition failed: " #cond;                                                                                                   \
        return;                                                                                                                                                \
    }

static constexpr auto DBUSMENU_PROPERTY_ID = "_dbusmenu_id";
static constexpr auto DBUSMENU_PROPERTY_LABEL = "_dbusmenu_label";
static constexpr auto DBUSMENU_PROPERTY_ICON_NAME = "_dbusmenu_icon_name";
static constexpr auto DBUSMENU_PROPERTY_ICON_DATA_HASH = "_dbusmenu_icon_data_hash";

static constexpr int MAX_ACTIONS_PER_MENU = 1000;
static constexpr int MAX_TOTAL_ACTIONS = 5000;

static QAction *createKdeTitle(const QAction *action, QWidget *parent)
{
    QToolButton *titleWidget = new QToolButton(nullptr);
    QFont font = titleWidget->font();
    font.setBold(true);
    titleWidget->setFont(font);
    titleWidget->setIcon(action->icon());
    titleWidget->setText(action->text());
    titleWidget->setDown(true);
    titleWidget->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QWidgetAction *titleAction = new QWidgetAction(parent);
    titleAction->setDefaultWidget(titleWidget);
    return titleAction;
}

class DBusMenuImporterPrivate
{
public:
    DBusMenuImporter *q;

    DBusMenuInterface *m_interface = nullptr;
    QMenu *m_menu = nullptr;
    using ActionForId = QHash<int, QAction *>;
    ActionForId m_actionForId;
    QTimer m_pendingLayoutUpdateTimer;

    QSet<int> m_idsRefreshedByAboutToShow;
    QSet<int> m_pendingLayoutUpdates;
    QPointer<QObject> m_activationRequest;

    QDBusPendingCallWatcher *refresh(int id)
    {
        const auto call = m_interface->GetLayout(id, 1, QStringList());
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, q);
        watcher->setProperty(DBUSMENU_PROPERTY_ID, id);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, q, &DBusMenuImporter::slotGetLayoutFinished);

        return watcher;
    }

    QMenu *createMenu(QWidget *parent)
    {
        QMenu *menu = q->createMenu(parent);
        QObject::connect(menu, &QMenu::hovered, q, &DBusMenuImporter::slotActionHovered);
        return menu;
    }

    /**
     * Init all the immutable action properties here
     * TODO: Document immutable properties?
     *
     * Note: we remove properties we handle from the map (using QMap::take()
     * instead of QMap::value()) to avoid warnings about these properties in
     * updateAction()
     */
    QAction *createAction(int id, const QVariantMap &_map, QWidget *parent)
    {
        QVariantMap map = _map;
        QAction *action = new QAction(parent);
        action->setProperty(DBUSMENU_PROPERTY_ID, id);

        const QString type = map.take(QStringLiteral("type")).toString();
        if (type == QLatin1StringView("separator")) {
            action->setSeparator(true);
        }

        if (map.take(QStringLiteral("children-display")).toString() == QLatin1StringView("submenu")) {
            QMenu *menu = createMenu(parent);
            action->setMenu(menu);
        }

        const QString toggleType = map.take(QStringLiteral("toggle-type")).toString();
        if (!toggleType.isEmpty()) {
            action->setCheckable(true);
            if (toggleType == QLatin1StringView("radio")) {
                QActionGroup *group = new QActionGroup(action);
                group->addAction(action);
            }
        }

        const bool isKdeTitle = map.take(QStringLiteral("x-kde-title")).toBool();
        updateAction(action, map);

        if (isKdeTitle) {
            QAction *oldAction = action;
            action = createKdeTitle(oldAction, parent);
            oldAction->deleteLater();
        }

        return action;
    }

    /**
     * Update mutable properties of an action.
     *
     * @param action the action to update
     * @param map holds the property values
     */
    void updateAction(QAction *action, const QVariantMap &map)
    {
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            const QString &key = it.key();
            if (key == QLatin1StringView("type") || key == QLatin1StringView("toggle-type") || key == QLatin1StringView("children-display")) {
                continue;
            }
            updateActionProperty(action, key, it.value());
        }
    }

    void updateActionProperty(QAction *action, const QString &key, const QVariant &value)
    {
        if (key == QLatin1StringView("label")) {
            updateActionLabel(action, value);
        } else if (key == QLatin1StringView("enabled")) {
            updateActionEnabled(action, value);
        } else if (key == QLatin1StringView("toggle-state")) {
            updateActionChecked(action, value);
        } else if (key == QLatin1StringView("icon-name")) {
            updateActionIconByName(action, value);
        } else if (key == QLatin1StringView("icon-data")) {
            updateActionIconByData(action, value);
        } else if (key == QLatin1StringView("visible")) {
            updateActionVisible(action, value);
        } else if (key == QLatin1StringView("shortcut")) {
            updateActionShortcut(action, value);
        } else {
            // qDebug(DBUSMENUQT) << "Unhandled property update" << key;
        }
    }

    void updateActionLabel(QAction *action, const QVariant &value)
    {
        action->setProperty(DBUSMENU_PROPERTY_LABEL, value.toString());
        QString text = menuLabelToQt(value.toString());
        if (action->menu()) {
            text += QLatin1StringView("  ");
        }
        action->setText(text);
    }

    void updateActionEnabled(QAction *action, const QVariant &value)
    {
        action->setEnabled(value.isValid() ? value.toBool() : true);
    }

    void updateActionChecked(QAction *action, const QVariant &value)
    {
        if (action->isCheckable() && value.isValid()) {
            action->setChecked(value.toInt() == 1);
        }
    }

    void updateActionIconByName(QAction *action, const QVariant &value)
    {
        const QString iconName = value.toString();
        const QString previous = action->property(DBUSMENU_PROPERTY_ICON_NAME).toString();
        if (previous == iconName) {
            return;
        }
        action->setProperty(DBUSMENU_PROPERTY_ICON_NAME, iconName);
        if (iconName.isEmpty()) {
            action->setIcon(QIcon());
            return;
        }
        action->setIcon(q->iconForName(iconName));
    }

    void updateActionIconByData(QAction *action, const QVariant &value)
    {
        const QByteArray data = value.toByteArray();
        uint dataHash = qHash(data);
        uint previousDataHash = action->property(DBUSMENU_PROPERTY_ICON_DATA_HASH).toUInt();
        if (previousDataHash == dataHash) {
            return;
        }
        action->setProperty(DBUSMENU_PROPERTY_ICON_DATA_HASH, dataHash);
        QPixmap pix;
        if (!pix.loadFromData(data)) {
            // qDebug(DBUSMENUQT) << "Failed to decode icon-data property for action" << action->text();
            action->setIcon(QIcon());
            return;
        }
        action->setIcon(QIcon(pix));
    }

    void updateActionVisible(QAction *action, const QVariant &value)
    {
        action->setVisible(value.isValid() ? value.toBool() : true);
    }

    void updateActionShortcut(QAction *action, const QVariant &value)
    {
        QDBusArgument arg = value.value<QDBusArgument>();
        DBusMenuShortcut dmShortcut;
        arg >> dmShortcut;
        QKeySequence keySequence = dmShortcut.toKeySequence();
        action->setShortcut(keySequence);
    }

    QMenu *menuForId(int id) const
    {
        if (id == 0) {
            return q->menu();
        }
        QAction *action = m_actionForId.value(id);
        if (!action) {
            return nullptr;
        }
        return action->menu();
    }

    void slotItemsPropertiesUpdated(const DBusMenuItemList &updatedList, const DBusMenuItemKeysList &removedList);

    void sendEvent(int id, const QString &eventId)
    {
        m_interface->Event(id, eventId, QDBusVariant(QString()), 0u);
    }
};

DBusMenuImporter::DBusMenuImporter(const QString &service, const QString &path, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<DBusMenuImporterPrivate>())
{
    DBusMenuTypes_register();

    d->q = this;
    d->m_interface = new DBusMenuInterface(service, path, QDBusConnection::sessionBus(), this);

    d->m_pendingLayoutUpdateTimer.setSingleShot(true);
    connect(&d->m_pendingLayoutUpdateTimer, &QTimer::timeout, this, &DBusMenuImporter::processPendingLayoutUpdates);

    connect(d->m_interface, &DBusMenuInterface::LayoutUpdated, this, &DBusMenuImporter::slotLayoutUpdated);
    connect(d->m_interface, &DBusMenuInterface::ItemActivationRequested, this, &DBusMenuImporter::slotItemActivationRequested);
    connect(d->m_interface, &DBusMenuInterface::ItemsPropertiesUpdated, this, [this](const DBusMenuItemList &updatedList, const DBusMenuItemKeysList &removedList) {
        d->slotItemsPropertiesUpdated(updatedList, removedList);
    });

    d->refresh(0);
}

DBusMenuImporter::~DBusMenuImporter()
{
    // Do not use "delete d->m_menu": even if we are being deleted we should
    // leave enough time for the menu to finish what it was doing, for example
    // if it was being displayed.
    if (d->m_menu) {
        // If StatusNotifierItemSource is gone before PlasmaDBusMenuImporter::menuUpdated, there is no menu
        d->m_menu->deleteLater();
    }
}

void DBusMenuImporter::slotLayoutUpdated(uint revision, int parentId)
{
    Q_UNUSED(revision)
    d->m_idsRefreshedByAboutToShow.remove(parentId);
    d->m_pendingLayoutUpdates << parentId;
    if (!d->m_pendingLayoutUpdateTimer.isActive()) {
        d->m_pendingLayoutUpdateTimer.start();
    }
}

void DBusMenuImporter::processPendingLayoutUpdates()
{
    QSet<int> ids;
    ids.swap(d->m_pendingLayoutUpdates);
    for (int id : std::as_const(ids)) {
        d->refresh(id);
    }
}

QMenu *DBusMenuImporter::menu() const
{
    if (!d->m_menu) {
        d->m_menu = d->createMenu(nullptr);
    }
    return d->m_menu;
}

void DBusMenuImporterPrivate::slotItemsPropertiesUpdated(const DBusMenuItemList &updatedList, const DBusMenuItemKeysList &removedList)
{
    bool needLayoutUpdate = false;
    for (const DBusMenuItem &item : updatedList) {
        QAction *action = m_actionForId.value(item.id);
        if (!action) {
            // We don't know this action. It probably is in a menu we haven't fetched yet.
            // This can happen if a new item is added but LayoutUpdated hasn't been received yet.
            needLayoutUpdate = true;
            continue;
        }

        auto it = item.properties.constBegin();
        const auto end = item.properties.constEnd();
        for (; it != end; ++it) {
            updateActionProperty(action, it.key(), it.value());
        }
    }

    for (const DBusMenuItemKeys &item : removedList) {
        QAction *action = m_actionForId.value(item.id);
        if (!action) {
            // We don't know this action. It probably is in a menu we haven't fetched yet.
            needLayoutUpdate = true;
            continue;
        }

        for (const QString &key : item.properties) {
            updateActionProperty(action, key, QVariant());
        }
    }

    if (needLayoutUpdate) {
        q->slotLayoutUpdated(0, 0);
    }
}

QAction *DBusMenuImporter::actionForId(int id) const
{
    return d->m_actionForId.value(id);
}

void DBusMenuImporter::slotItemActivationRequested(int id, uint /*timestamp*/)
{
    cancelActivationRequest();
    QAction *action = d->m_actionForId.value(id);
    if (action) {
        Q_EMIT actionActivationRequested(action);
        return;
    }

    // The protocol has no parent lookup. Fetch only the ID tree on an explicit
    // request for an unknown item, then import the menus along its path.
    auto *request = new QObject(this);
    d->m_activationRequest = request;
    QTimer::singleShot(5000, request, &QObject::deleteLater);
    auto *watcher = new QDBusPendingCallWatcher(d->m_interface->GetLayout(0, -1, {QStringLiteral("children-display")}), request);
    connect(watcher, &QDBusPendingCallWatcher::finished, request, [this, request, id](QDBusPendingCallWatcher *finished) {
        const QDBusPendingReply<uint, DBusMenuLayoutItem> reply = *finished;
        finished->deleteLater();
        if (reply.isError()) { request->deleteLater(); return; }
        QList<int> path;
        int remaining = MAX_TOTAL_ACTIONS;
        std::function<bool(const DBusMenuLayoutItem &, int)> find = [&](const DBusMenuLayoutItem &item, int depth) {
            if (--remaining < 0 || depth > 64) return false;
            path.append(item.id);
            if (item.id == id) return true;
            for (const auto &child : item.children) {
                if (remaining <= 0) break;
                if (find(child, depth + 1)) return true;
            }
            path.removeLast();
            return false;
        };
        if (!find(reply.argumentAt<1>(), 0)) { request->deleteLater(); return; }

        // Each update imports one level, keeping unrelated branches unloaded.
        auto advance = [this, request, path, id, waiting = QPointer<QMenu>{}, requested = QSet<int>{}](QMenu *updated) mutable {
            if (waiting && waiting != updated) return;
            waiting.clear();
            for (qsizetype i = 0; i < path.size(); ++i) {
                if (path.at(i) == 0) continue;
                QAction *item = d->m_actionForId.value(path.at(i));
                if (item) continue;
                QMenu *parent = i > 0 ? d->menuForId(path.at(i - 1)) : nullptr;
                const int parentId = i > 0 ? path.at(i - 1) : 0;
                if (!parent || requested.contains(parentId)) { request->deleteLater(); return; }
                requested.insert(parentId);
                waiting = parent;
                updateMenu(parent);
                return;
            }
            QAction *target = d->m_actionForId.value(id);
            disconnect(this, nullptr, request, nullptr);
            request->deleteLater();
            if (target) Q_EMIT actionActivationRequested(target);
        };
        const QPointer<QObject> lifetime(request);
        advance(nullptr);
        if (lifetime) connect(this, &DBusMenuImporter::menuUpdated, lifetime, advance);
    });
}

void DBusMenuImporter::cancelActivationRequest()
{
    delete d->m_activationRequest.data();
}

void DBusMenuImporter::slotGetLayoutFinished(QDBusPendingCallWatcher *watcher)
{
    const int parentId = watcher->property(DBUSMENU_PROPERTY_ID).toInt();
    watcher->deleteLater();

    d->m_idsRefreshedByAboutToShow.remove(parentId);

    QMenu *menu = d->menuForId(parentId);

    const QDBusPendingReply<uint, DBusMenuLayoutItem> reply = *watcher;
    if (!reply.isValid()) {
        qCWarning(DBUSMENUQT) << reply.error().message();
        if (menu) {
            Q_EMIT menuUpdated(menu);
        }
        return;
    }

#ifdef BENCHMARK
    qCDebug(DBUSMENUQT) << "- items received:" << sChrono.elapsed() << "ms";
#endif
    const DBusMenuLayoutItem rootItem = reply.argumentAt<1>();

    if (!menu) {
        qCDebug(DBUSMENUQT) << "No menu for id" << parentId;
        return;
    }

    menu->setUpdatesEnabled(false);

    const auto actionIdentity = [](const QAction *action) {
        // Compare exported labels: KDE may have added mnemonic markers locally.
        return QVariantList{action->property(DBUSMENU_PROPERTY_LABEL),
                            action->property(DBUSMENU_PROPERTY_ICON_NAME),
                            action->property(DBUSMENU_PROPERTY_ICON_DATA_HASH),
                            action->shortcut().toString(QKeySequence::PortableText),
                            action->isCheckable(), bool(action->menu())};
    };
    const QPointer<QAction> selectedAction(menu->activeAction());
    const QVariantList selectedIdentity = selectedAction ? actionIdentity(selectedAction) : QVariantList();

    int childCount = rootItem.children.count();
    if (childCount > MAX_ACTIONS_PER_MENU) {
        qCWarning(DBUSMENUQT) << "Menu children count" << childCount << "exceeds limit" << MAX_ACTIONS_PER_MENU << ". Truncating.";
        childCount = MAX_ACTIONS_PER_MENU;
    }

    const auto actions = menu->actions();
    QSet<int> newIds;
    newIds.reserve(childCount);
    for (int i = 0; i < childCount; ++i) {
        newIds.insert(rootItem.children.at(i).id);
    }

    // 1. Remove actions no longer present and keep valid ones in currentActions
    QList<QAction *> currentActions;
    currentActions.reserve(actions.count());
    for (QAction *action : std::as_const(actions)) {
        const int id = action->property(DBUSMENU_PROPERTY_ID).toInt();
        if (!newIds.contains(id)) {
            menu->removeAction(action);
            if (QMenu *subMenu = action->menu()) {
                subMenu->deleteLater();
            }
            action->deleteLater();
            d->m_actionForId.remove(id);
        } else {
            currentActions.append(action);
        }
    }

    // 2. Synchronize existing actions and add new ones
    QList<QAction *> finalActions;
    finalActions.reserve(childCount);
    QSet<QAction *> usedActions;
    usedActions.reserve(currentActions.count());

    int nextUnusedIndex = 0;
    int ignoredCount = 0;
    for (int i = 0; i < childCount; ++i) {
        const DBusMenuLayoutItem &dbusMenuItem = rootItem.children.at(i);
        QAction *action = d->m_actionForId.value(dbusMenuItem.id);

        if (action) {
            // Update properties
            d->updateAction(action, dbusMenuItem.properties);
            if (action->parent() != menu) {
                action->setParent(menu);
            }
        } else {
            // Create
            if (d->m_actionForId.count() >= MAX_TOTAL_ACTIONS) {
                ignoredCount++;
                continue;
            }
            const int id = dbusMenuItem.id;
            action = d->createAction(id, dbusMenuItem.properties, menu);
            d->m_actionForId.insert(id, action);

            connect(action, &QObject::destroyed, this, [this, id]() {
                d->m_actionForId.remove(id);
            });

            connect(action, &QAction::triggered, this, [id, this]() {
                sendClickedEvent(id);
            });

            if (QMenu *menuAction = action->menu()) {
                connect(menuAction, &QMenu::aboutToShow, this, &DBusMenuImporter::slotMenuAboutToShow, Qt::UniqueConnection);
            }
        }

        // Find the first unused action in currentActions to be the "before" action.
        while (nextUnusedIndex < currentActions.count() && usedActions.contains(currentActions.at(nextUnusedIndex))) {
            nextUnusedIndex++;
        }
        QAction *before = (nextUnusedIndex < currentActions.count()) ? currentActions.at(nextUnusedIndex) : nullptr;

        if (before != action) {
            menu->insertAction(before, action);
        }

        finalActions.append(action);
        usedActions.insert(action);
    }
    Q_ASSERT(menu->actions() == finalActions);

    // Some applications rebuild a submenu with fresh IDs while it is opening.
    // Retain its keyboard selection only if the same command has one clear match.
    if (!selectedIdentity.isEmpty() && menu->isVisible() && !menu->activeAction()
        && !finalActions.contains(selectedAction)) {
        QAction *replacement = nullptr;
        for (QAction *action : std::as_const(finalActions)) {
            if (!action->isVisible() || !action->isEnabled() || action->isSeparator()
                || actionIdentity(action) != selectedIdentity) {
                continue;
            }
            if (replacement) {
                replacement = nullptr;
                break;
            }
            replacement = action;
        }
        if (replacement) menu->setActiveAction(replacement);
    }

    if (ignoredCount > 0) {
        qCWarning(DBUSMENUQT) << "Maximum total actions limit reached (" << MAX_TOTAL_ACTIONS << ")." << ignoredCount << "new actions were ignored.";
    }

    connect(menu, &QMenu::aboutToHide, this, &DBusMenuImporter::slotMenuAboutToHide, Qt::UniqueConnection);
    menu->setUpdatesEnabled(true);
    Q_EMIT menuUpdated(menu);
}

void DBusMenuImporter::sendClickedEvent(int id)
{
    d->sendEvent(id, QStringLiteral("clicked"));
}

void DBusMenuImporter::updateMenu()
{
    updateMenu(DBusMenuImporter::menu());
}

void DBusMenuImporter::updateMenu(QMenu *menu)
{
    Q_ASSERT(menu);

    QAction *action = menu->menuAction();
    if (!action) {
        return;
    }

    const int id = action->property(DBUSMENU_PROPERTY_ID).toInt();

    if (d->m_idsRefreshedByAboutToShow.contains(id)) {
        return; // Update already in progress, ignore re-entrant call.
    }
    d->m_idsRefreshedByAboutToShow << id;

    const auto call = d->m_interface->AboutToShow(id);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    watcher->setProperty(DBUSMENU_PROPERTY_ID, id);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &DBusMenuImporter::slotAboutToShowDBusCallFinished);

    // Firefox deliberately ignores "aboutToShow" whereas Qt ignores" opened", so we'll just send both all the time...
    d->sendEvent(id, QStringLiteral("opened"));
}

void DBusMenuImporter::slotAboutToShowDBusCallFinished(QDBusPendingCallWatcher *watcher)
{
    const int id = watcher->property(DBUSMENU_PROPERTY_ID).toInt();
    watcher->deleteLater();

    QMenu *menu = d->menuForId(id);
    if (!menu) {
        d->m_idsRefreshedByAboutToShow.remove(id);
        return;
    }

    const QDBusPendingReply<bool> reply = *watcher;
    if (reply.isError()) {
        d->m_idsRefreshedByAboutToShow.remove(id);
        qCWarning(DBUSMENUQT) << "Call to AboutToShow() failed:" << reply.error().message();
        Q_EMIT menuUpdated(menu);
        return;
    }
    // We used to only refresh if needRefresh was true.
    // However, some servers are buggy and don't signal correctly, or signals are lost.
    // Since this is called JIT before showing a menu, always refreshing is safer
    d->refresh(id);
}

void DBusMenuImporter::slotMenuAboutToHide()
{
    QMenu *menu = qobject_cast<QMenu *>(sender());
    Q_ASSERT(menu);

    QAction *action = menu->menuAction();
    Q_ASSERT(action);

    const int id = action->property(DBUSMENU_PROPERTY_ID).toInt();
    d->sendEvent(id, QStringLiteral("closed"));
}

void DBusMenuImporter::slotMenuAboutToShow()
{
    QMenu *menu = qobject_cast<QMenu *>(sender());
    Q_ASSERT(menu);

    retryEmptySubMenu(menu);
    updateMenu(menu);
}

void DBusMenuImporter::retryEmptySubMenu(QMenu *menu)
{
    auto *parent = qobject_cast<QMenu *>(QApplication::activePopupWidget());
    QAction *action = parent ? parent->activeAction() : nullptr;
    if (!menu->actions().isEmpty() || !action || action->menu() != menu || parent == menu) {
        return;
    }

    // QMenu cannot open an empty submenu while its D-Bus contents are in flight.
    auto *request = new QObject(this);
    const QPointer<QMenu> parentMenu(parent);
    const QPointer<QAction> requestedAction(action);
    connect(parent, &QMenu::aboutToHide, request, [request] { request->setProperty("cancelled", true); });
    connect(parent, &QMenu::hovered, request, [request, requestedAction](QAction *hovered) {
        if (hovered != requestedAction) request->setProperty("cancelled", true);
    });
    connect(this, &DBusMenuImporter::menuUpdated, request, [request, parentMenu, requestedAction, menu](QMenu *updated) {
        if (updated != menu) return;
        QTimer::singleShot(0, request, [request, parentMenu, requestedAction] {
            if (!request->property("cancelled").toBool() && parentMenu && requestedAction
                && parentMenu->isVisible() && parentMenu->activeAction() == requestedAction
                && QApplication::activePopupWidget() == parentMenu && requestedAction->menu()
                && !requestedAction->menu()->isVisible() && !requestedAction->menu()->actions().isEmpty()) {
                const auto key = parentMenu->layoutDirection() == Qt::RightToLeft ? Qt::Key_Left : Qt::Key_Right;
                QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
                QCoreApplication::sendEvent(parentMenu, &event);
            }
            request->deleteLater();
        });
    });
    QTimer::singleShot(5000, request, &QObject::deleteLater);
}

void DBusMenuImporter::slotActionHovered(QAction *action)
{
    if (action && action->menu() && action->menu()->actions().isEmpty()) {
        updateMenu(action->menu());
    }
}

QMenu *DBusMenuImporter::createMenu(QWidget *parent)
{
    return new QMenu(parent);
}

QIcon DBusMenuImporter::iconForName(const QString &name)
{
    return QIcon::fromTheme(name);
}

#include "moc_dbusmenuimporter.cpp"
