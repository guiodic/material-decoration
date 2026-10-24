// SPDX-License-Identifier: GPL-2.0-or-later
#include "TilingController.h"

#include <QObject>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDebug>
#include <QPointer>
#include <QQueue>
#include <QTimer>
#include <QUuid>
#include <QVariantMap>

namespace Material
{
namespace
{
QUuid windowId(QObject *decoration)
{
    // KWin supplies the owning window as the QObject parent. Inspect its Qt
    // property instead of depending on KWin's private C++ classes or ABI.
    return decoration && decoration->parent()
        ? decoration->parent()->property("internalId").toUuid() : QUuid();
}
}

class TilingBridge : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.github.guiodic.MaterialTiling")
public:
    explicit TilingBridge(QObject *parent) : QObject(parent)
    {
        auto bus = QDBusConnection::sessionBus();
        m_registered = bus.registerService(QStringLiteral("com.github.guiodic.MaterialTiling"));
        if (m_registered) {
            m_registered = bus.registerObject(QStringLiteral("/Tiling"), this, QDBusConnection::ExportAllSlots);
            if (!m_registered) bus.unregisterService(QStringLiteral("com.github.guiodic.MaterialTiling"));
        }
        m_timeout.setSingleShot(true);
        m_timeout.setInterval(2000);
        connect(&m_timeout, &QTimer::timeout, this, [this] { finish(false, QStringLiteral("Script did not respond")); });
    }

    bool enqueue(QObject *decoration, Qt::Edges edges, std::function<void(bool)> callback = {})
    {
        if (!m_registered || m_pending.size() >= 8 || !canTileWindow(decoration)) {
            if (callback) QTimer::singleShot(0, decoration, [callback] { callback(false); });
            return false;
        }
        Request request;
        request.context = decoration;
        request.callback = std::move(callback);
        request.data = {{QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
                        {QStringLiteral("uuid"), windowId(decoration).toString(QUuid::WithoutBraces)},
                        {QStringLiteral("edges"), int(edges)},
                        {QStringLiteral("operation"), edges ? QStringLiteral("tile") : QStringLiteral("ping")}};
        m_pending.enqueue(std::move(request));
        if (m_pending.size() == 1) dispatch();
        return true;
    }

public slots:
    QVariantMap takeRequest()
    {
        if (!trustedCaller() || m_pending.isEmpty() || m_taken) return {};
        m_taken = true;
        return m_pending.head().data;
    }

    void complete(const QString &id, bool success, const QString &reason)
    {
        if (!trustedCaller() || m_pending.isEmpty() || m_pending.head().data.value(QStringLiteral("id")).toString() != id) return;
        finish(success, reason);
    }

private:
    struct Request {
        QPointer<QObject> context;
        QVariantMap data;
        std::function<void(bool)> callback;
    };
    bool trustedCaller() const
    {
        // Verified on KWin 6.x: script callDBus() and this bridge both use
        // QDBusConnection::sessionBus(), so their unique bus names match.
        // Sharing a process alone would not guarantee the same connection.
        return calledFromDBus() && message().service() == QDBusConnection::sessionBus().baseService();
    }
    void dispatch()
    {
        if (m_inFlight) return;
        while (!m_pending.isEmpty() && !m_pending.head().context) m_pending.dequeue();
        if (m_pending.isEmpty()) return;
        m_inFlight = true;
        m_taken = false;
        m_timeout.start();
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"), QStringLiteral("/component/kwin"),
                                                      QStringLiteral("org.kde.kglobalaccel.Component"), QStringLiteral("invokeShortcut"));
        message << QStringLiteral("MaterialTilingPopupDispatch");
        const QString id = m_pending.head().data.value(QStringLiteral("id")).toString();
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 1500), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, id](QDBusPendingCallWatcher *call) {
            const auto reply = call->reply();
            call->deleteLater();
            if (m_pending.isEmpty() || m_pending.head().data.value(QStringLiteral("id")).toString() != id) return;
            if (reply.type() == QDBusMessage::ErrorMessage) {
                finish(false, QStringLiteral("Script action is unavailable"));
            }
        });
    }
    void finish(bool success, const QString &reason)
    {
        if (m_pending.isEmpty()) return;
        m_timeout.stop();
        m_inFlight = false;
        auto request = m_pending.dequeue();
        if (!success) qWarning() << "Material tiling:" << reason;
        if (request.context && request.callback) {
            QTimer::singleShot(0, request.context, [callback = std::move(request.callback), success] { callback(success); });
        }
        // Let the script finish its acknowledgement before delivering another request.
        QTimer::singleShot(0, this, [this] { dispatch(); });
    }
    bool m_inFlight = false;
    bool m_registered = false;
    bool m_taken = false;
    QQueue<Request> m_pending;
    QTimer m_timeout;
};

static TilingBridge *bridge()
{
    static auto *instance = new TilingBridge(QCoreApplication::instance());
    return instance;
}

bool canTileWindow(QObject *decoration)
{
    return !windowId(decoration).isNull() && decoration->parent()->property("resizeable").toBool()
        && !decoration->parent()->property("specialWindow").toBool();
}

void checkTilingScript(QObject *decoration, std::function<void(bool)> callback)
{
    bridge()->enqueue(decoration, {}, std::move(callback));
}

bool tileWindow(QObject *decoration, Qt::Edges edges)
{
    const int mask = int(edges);
    if (mask != 1 && mask != 2 && mask != 4 && mask != 8 && mask != 3 && mask != 5 && mask != 10 && mask != 12) return false;
    return bridge()->enqueue(decoration, edges);
}
}
#include "TilingController.moc"
