// SPDX-License-Identifier: GPL-2.0-or-later
#include "LongPressAction.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QStringList>

namespace Material
{
namespace
{
void checkShortcut(QObject *context, const QString &shortcut, std::function<void(bool)> callback)
{
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"), QStringLiteral("/component/kwin"),
                                                QStringLiteral("org.kde.kglobalaccel.Component"), QStringLiteral("shortcutNames"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 1500), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [callback = std::move(callback), shortcut](QDBusPendingCallWatcher *call) {
        QDBusPendingReply<QStringList> reply = *call;
        call->deleteLater();
        callback(!reply.isError() && reply.value().contains(shortcut));
    });
}
}

void checkLongPressAction(QObject *context, const QString &shortcut, const QString &script, std::function<void(bool)> callback)
{
    if (script.isEmpty()) {
        checkShortcut(context, shortcut, std::move(callback));
        return;
    }
    // Saved shortcut names can outlive an unloaded script. Check the script too.
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
                                                QStringLiteral("org.kde.kwin.Scripting"), QStringLiteral("isScriptLoaded"));
    message << script;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 1500), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context,
                     [context, callback = std::move(callback), shortcut](QDBusPendingCallWatcher *call) {
        QDBusPendingReply<bool> reply = *call;
        call->deleteLater();
        if (reply.isError() || !reply.value()) callback(false);
        else checkShortcut(context, shortcut, std::move(callback));
    });
}
}
