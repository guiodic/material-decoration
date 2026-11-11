/* This file is part of the dbusmenu-qt library
    SPDX-FileCopyrightText: 2009 Canonical
    SPDX-FileContributor: Aurelien Gateau <aurelien.gateau@canonical.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#pragma once

// Qt
#include <QList>
#include <QStringList>
#include <QVariant>

class QDBusArgument;

namespace DBusMenuLimits
{
// Imported actions exclude the root; the decoder budgets one extra root node.
inline constexpr int MaxActions = 5000;
}

//// DBusMenuItem
/**
 * Internal struct used to communicate on DBus
 */
struct DBusMenuItem {
    int id = 0;
    QVariantMap properties;
};

QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuItem &item);
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuItem &item);

typedef QList<DBusMenuItem> DBusMenuItemList;

//// DBusMenuItemKeys
/**
 * Represents a list of keys for a menu item
 */
struct DBusMenuItemKeys {
    int id = 0;
    QStringList properties;
};

QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuItemKeys &);
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuItemKeys &);

typedef QList<DBusMenuItemKeys> DBusMenuItemKeysList;

//// DBusMenuLayoutItem
/**
 * A layout node with its children, encoded as (ia{sv}av). GetLayout() returns
 * a revision and one root node. Decoding shares a node budget across the tree.
 */
struct DBusMenuLayoutItem;
struct DBusMenuLayoutItem {
    int id = 0;
    QVariantMap properties;
    QList<DBusMenuLayoutItem> children;
    // True if this subtree was cut short by the node budget. Propagates to the
    // root so callers can reject partial trees; never serialized on D-Bus.
    bool truncated = false;
};

QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuLayoutItem &);
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuLayoutItem &);

typedef QList<DBusMenuLayoutItem> DBusMenuLayoutItemList;

//// DBusMenuShortcut

class DBusMenuShortcut;

QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuShortcut &);
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuShortcut &);

void DBusMenuTypes_register();
