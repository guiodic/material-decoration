/* This file is part of the dbusmenu-qt library
    SPDX-FileCopyrightText: 2009 Canonical
    SPDX-FileContributor: Aurelien Gateau <aurelien.gateau@canonical.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "dbusmenutypes_p.h"

// Local
#include "dbusmenushortcut_p.h"

// Qt
#include <QDBusArgument>
#include <QDBusMetaType>

//// DBusMenuItem
QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuItem &obj)
{
    argument.beginStructure();
    argument << obj.id << obj.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuItem &obj)
{
    argument.beginStructure();
    argument >> obj.id >> obj.properties;
    argument.endStructure();
    return argument;
}

//// DBusMenuItemKeys
QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuItemKeys &obj)
{
    argument.beginStructure();
    argument << obj.id << obj.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuItemKeys &obj)
{
    argument.beginStructure();
    argument >> obj.id >> obj.properties;
    argument.endStructure();
    return argument;
}

//// DBusMenuLayoutItem
QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuLayoutItem &obj)
{
    argument.beginStructure();
    argument << obj.id << obj.properties;
    argument.beginArray(qMetaTypeId<QDBusVariant>());
    for (const DBusMenuLayoutItem &child : obj.children) {
        argument << QDBusVariant(QVariant::fromValue(child));
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

namespace
{
void readLayout(const QDBusArgument &argument, DBusMenuLayoutItem &obj, int &remaining)
{
    --remaining;
    obj.truncated = false;
    argument.beginStructure();
    argument >> obj.id >> obj.properties;
    argument.beginArray();
    obj.children.clear();
    while (!argument.atEnd() && remaining > 0) {
        QDBusVariant dbusVariant;
        argument >> dbusVariant;
        const auto childArgument = dbusVariant.variant().value<QDBusArgument>();

        auto &child = obj.children.emplace_back();
        readLayout(childArgument, child, remaining);
        obj.truncated = obj.truncated || child.truncated;
    }
    obj.truncated = obj.truncated || !argument.atEnd();
    // Closing the array skips unread entries without decoding their subtrees.
    argument.endArray();
    argument.endStructure();
}
}

const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuLayoutItem &obj)
{
    // Include the root in addition to the maximum number of imported actions.
    int remaining = DBusMenuLimits::MaxActions + 1;
    readLayout(argument, obj, remaining);
    return argument;
}

//// DBusMenuShortcut
QDBusArgument &operator<<(QDBusArgument &argument, const DBusMenuShortcut &obj)
{
    argument.beginArray(qMetaTypeId<QStringList>());
    for (const QStringList &shortcut : obj) {
        argument << shortcut;
    }
    argument.endArray();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, DBusMenuShortcut &obj)
{
    argument.beginArray();
    obj.clear();
    while (!argument.atEnd()) {
        // Deserialize directly into the newly created element to avoid stack temporaries
        argument >> obj.emplace_back();
    }
    argument.endArray();
    return argument;
}

void DBusMenuTypes_register()
{
    static bool registered = false;
    if (registered) {
        return;
    }
    qDBusRegisterMetaType<DBusMenuItem>();
    qDBusRegisterMetaType<DBusMenuItemList>();
    qDBusRegisterMetaType<DBusMenuItemKeys>();
    qDBusRegisterMetaType<DBusMenuItemKeysList>();
    qDBusRegisterMetaType<DBusMenuLayoutItem>();
    qDBusRegisterMetaType<DBusMenuLayoutItemList>();
    qDBusRegisterMetaType<DBusMenuShortcut>();
    registered = true;
}
