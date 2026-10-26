// SPDX-License-Identifier: GPL-2.0-or-later
#include "TilingController.h"

#include <QObject>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <cmath>
#include <limits>
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

    bool enqueue(QObject *decoration, QVariantMap data, std::function<void(bool, const QString &)> callback = {})
    {
        if (!m_registered || m_pending.size() >= 8 || !canTileWindow(decoration)) {
            if (callback) QTimer::singleShot(0, decoration, [callback] { callback(false, {}); });
            return false;
        }
        Request request;
        request.context = decoration;
        request.callback = std::move(callback);
        request.data = std::move(data);
        request.data.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
        request.data.insert(QStringLiteral("uuid"), windowId(decoration).toString(QUuid::WithoutBraces));
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

    void completeLayout(const QString &id, const QString &layout)
    {
        if (!trustedCaller() || m_pending.isEmpty() || m_pending.head().data.value(QStringLiteral("id")).toString() != id) return;
        finish(true, {}, layout);
    }

private:
    struct Request {
        QPointer<QObject> context;
        QVariantMap data;
        std::function<void(bool, const QString &)> callback;
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
    void finish(bool success, const QString &reason, const QString &layout = {})
    {
        if (m_pending.isEmpty()) return;
        m_timeout.stop();
        m_inFlight = false;
        auto request = m_pending.dequeue();
        if (!success) qWarning() << "Material tiling:" << reason;
        if (request.context && request.callback) {
            QTimer::singleShot(0, request.context, [callback = std::move(request.callback), success, layout] { callback(success, layout); });
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

void fetchTilingLayout(QObject *decoration, std::function<void(bool, const CustomTilingLayout &)> callback)
{
    bridge()->enqueue(decoration, {{QStringLiteral("operation"), QStringLiteral("ping")}, {QStringLiteral("layout"), true}},
                      [callback = std::move(callback)](bool available, const QString &json) {
        CustomTilingLayout layout;
        QSet<QString> ids;
        const auto zones = QJsonDocument::fromJson(json.toUtf8()).array();
        for (const auto &value : zones) {
            const auto zone = value.toObject();
            const auto coordinate = [&zone](const char *name) {
                return zone.value(QLatin1String(name)).toDouble(std::numeric_limits<double>::quiet_NaN());
            };
            const QRectF rect(coordinate("x"), coordinate("y"), coordinate("width"), coordinate("height"));
            const auto id = zone.value(QStringLiteral("id")).toString();
            if (id.isEmpty() || ids.contains(id) || !std::isfinite(rect.x()) || !std::isfinite(rect.y()) ||
                !std::isfinite(rect.width()) || !std::isfinite(rect.height()) || rect.isEmpty() ||
                !QRectF(-0.000001, -0.000001, 1.000002, 1.000002).contains(rect)) {
                layout.clear();
                break;
            }
            ids.insert(id);
            layout.append({rect.intersected(QRectF(0, 0, 1, 1)), id});
        }
        callback(available, layout);
    });
}

bool tileWindow(QObject *decoration, Qt::Edges edges)
{
    const int mask = int(edges);
    if (mask != 1 && mask != 2 && mask != 4 && mask != 8 && mask != 3 && mask != 5 && mask != 10 && mask != 12) return false;
    return bridge()->enqueue(decoration, {{QStringLiteral("operation"), QStringLiteral("tile")}, {QStringLiteral("edges"), mask}});
}

bool tileCustomWindow(QObject *decoration, const QString &tileId, std::function<void(bool)> callback)
{
    if (tileId.isEmpty()) return false;
    return bridge()->enqueue(decoration, {{QStringLiteral("operation"), QStringLiteral("tileCustom")}, {QStringLiteral("tileId"), tileId}},
                             [callback = std::move(callback)](bool success, const QString &) { callback(success); });
}
}
#include "TilingController.moc"
