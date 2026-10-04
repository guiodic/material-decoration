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

//// DBusMenuItem
/**
 * @brief Internal struct used to serialize DBus menu items over DBus.
 */
struct DBusMenuItem {
    int id = 0;             ///< Menu item identifier
    QVariantMap properties;  ///< Menu item property map
};

/**
 * @brief Serializes DBusMenuItem to DBus argument stream.
 */
QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuItem &item);

/**
 * @brief Deserializes DBusMenuItem from DBus argument stream.
 */
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuItem &item);

typedef QList<DBusMenuItem> DBusMenuItemList;

//// DBusMenuItemKeys
/**
 * @brief Represents a list of requested property keys for a DBus menu item.
 */
struct DBusMenuItemKeys {
    int id = 0;              ///< Menu item identifier
    QStringList properties;  ///< Requested property key list
};

/**
 * @brief Serializes DBusMenuItemKeys to DBus argument stream.
 */
QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuItemKeys &);

/**
 * @brief Deserializes DBusMenuItemKeys from DBus argument stream.
 */
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuItemKeys &);

typedef QList<DBusMenuItemKeys> DBusMenuItemKeysList;

//// DBusMenuLayoutItem
/**
 * @brief Represents a hierarchical DBus menu item node containing property map and child nodes.
 */
struct DBusMenuLayoutItem;
struct DBusMenuLayoutItem {
    int id = 0;                         ///< Menu item identifier
    QVariantMap properties;             ///< Menu item properties
    QList<DBusMenuLayoutItem> children; ///< List of child menu item nodes
};

/**
 * @brief Serializes DBusMenuLayoutItem to DBus argument stream.
 */
QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuLayoutItem &);

/**
 * @brief Deserializes DBusMenuLayoutItem from DBus argument stream.
 */
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuLayoutItem &);

typedef QList<DBusMenuLayoutItem> DBusMenuLayoutItemList;

//// DBusMenuShortcut

class DBusMenuShortcut;

/**
 * @brief Serializes DBusMenuShortcut to DBus argument stream.
 */
QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuShortcut &);

/**
 * @brief Deserializes DBusMenuShortcut from DBus argument stream.
 */
const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuShortcut &);

/**
 * @brief Registers custom DBusMenu data types with the Qt DBus type system.
 */
void DBusMenuTypes_register();