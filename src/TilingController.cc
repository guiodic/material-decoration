// SPDX-License-Identifier: GPL-2.0-or-later
#include "TilingController.h"

#include <KDecoration3/Decoration>
#include <KDecoration3/DecoratedWindow>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>
#include <QJSEngine>
#include <QSet>
#include <cmath>
#include <limits>
#include <QUuid>
#include <QVariantMap>

namespace Material
{
namespace
{
QUuid windowId(KDecoration3::Decoration *decoration)
{
    // KWin supplies the owning window as the QObject parent. Inspect its Qt
    // property instead of depending on KWin's private C++ classes or ABI.
    return decoration && decoration->parent()
        ? decoration->parent()->property("internalId").toUuid() : QUuid();
}
}

class WindowStateScriptBridge : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE QVariantList quickTiles(QObject *manager) const
    {
        QVariantList result;
        if (!manager || !manager->inherits("KWin::TileManager")) return result;
        // The script restricts window membership while selecting a root;
        // Tile::manage() then rejects other desktops before changing them.
        for (auto *root : manager->children()) {
            if (!root->inherits("KWin::QuickRootTile")) continue;
            for (const auto &tile : root->property("tiles").toList()) {
                if (auto *object = tile.value<QObject *>()) {
                    QJSEngine::setObjectOwnership(object, QJSEngine::CppOwnership);
                    result.append(QVariant::fromValue(object));
                }
            }
        }
        return result;
    }
};

bool canTileWindow(KDecoration3::Decoration *decoration)
{
    return !windowId(decoration).isNull() && decoration->window()->isResizeable()
        && !decoration->parent()->property("specialWindow").toBool();
}

static QVariantMap windowStateRequest(KDecoration3::Decoration *decoration, const QVariantMap &state)
{
    auto request = state;
    // Runtime placement hint only; never persisted in the window's state.
    request.insert(QStringLiteral("titleBarHeight"), decoration ? decoration->titleBar().height() : 0);
    return request;
}

static QJSEngine *scriptEngine(const QString &name)
{
    // KWin owns these engines below its registered Scripting object. This
    // QObject hierarchy is an implementation detail, not a stable KWin API.
    auto *scripting = QDBusConnection::sessionBus().objectRegisteredAt(QStringLiteral("/Scripting"));
    if (scripting) {
        for (auto *engine : scripting->findChildren<QJSEngine *>()) {
            auto function = engine->globalObject().property(name);
            if (function.isCallable()) return engine;
        }
    }
    return {};
}

static QJSValue callScript(KDecoration3::Decoration *decoration, const QString &name, const QVariantMap &request, bool *available = nullptr)
{
    if (available) *available = true;
    if (!decoration || !decoration->parent()) return {};
    auto *engine = scriptEngine(name);
    if (!engine) {
        if (available) *available = false;
        return {};
    }
    auto function = engine->globalObject().property(name);
    static auto *stateBridge = new WindowStateScriptBridge(QCoreApplication::instance());
    QJSEngine::setObjectOwnership(stateBridge, QJSEngine::CppOwnership);
    engine->globalObject().setProperty(QStringLiteral("materialWindowStateBridge"), engine->newQObject(stateBridge));
    auto *owner = decoration->parent();
    QJSEngine::setObjectOwnership(owner, QJSEngine::CppOwnership);
    // Activation/tiling can destroy the decoration. Do not access it after
    // entering JavaScript; the KWin window is the actual operation target.
    auto result = function.call({engine->newQObject(owner), engine->toScriptValue(request)});
    if (result.isError()) {
        qWarning() << "Material script:" << result.toString();
        return {};
    }
    return result;
}

bool tilingScriptAvailable()
{
    return scriptEngine(QStringLiteral("materialTiling")) != nullptr;
}

WindowRestoreResult restoreWindowStateImmediately(KDecoration3::Decoration *decoration, const QVariantMap &state)
{
    bool available = false;
    const auto result = callScript(decoration, QStringLiteral("materialRestoreWindowState"), windowStateRequest(decoration, state), &available);
    if (!available) return WindowRestoreResult::Unavailable;
    return result.isBool() && result.toBool() ? WindowRestoreResult::Succeeded : WindowRestoreResult::Failed;
}

WindowRestoreResult prepareWindowStateImmediately(KDecoration3::Decoration *decoration, const QVariantMap &state)
{
    bool available = false;
    const auto result = callScript(decoration, QStringLiteral("materialPrepareWindowState"), windowStateRequest(decoration, state), &available);
    if (!available) return WindowRestoreResult::Unavailable;
    return result.isBool() && result.toBool() ? WindowRestoreResult::Succeeded : WindowRestoreResult::Failed;
}

QObject *windowStateWorkspace()
{
    auto *scripting = QDBusConnection::sessionBus().objectRegisteredAt(QStringLiteral("/Scripting"));
    if (!scripting) return nullptr;
    for (auto *object : scripting->findChildren<QObject *>()) {
        if (object->inherits("KWin::WorkspaceWrapper")) return object;
    }
    return nullptr;
}

bool fetchTilingLayout(KDecoration3::Decoration *decoration, CustomTilingLayout &layout)
{
    layout.clear();
    if (!canTileWindow(decoration)) return false;
    const auto result = callScript(decoration, QStringLiteral("materialTiling"), {{"operation", "layout"}});
    if (!result.isArray()) return false;
    CustomTilingLayout parsed;
    QSet<QString> ids;
    const auto length = result.property("length").toUInt();
    for (quint32 i = 0; i < length; ++i) {
        const auto zone = result.property(i);
        const auto coordinate = [&zone](const char *name) {
            const auto value = zone.property(QLatin1String(name));
            return value.isNumber() ? value.toNumber() : std::numeric_limits<double>::quiet_NaN();
        };
        const QRectF rect(coordinate("x"), coordinate("y"), coordinate("width"), coordinate("height"));
        const auto identifier = zone.property("id");
        const auto id = identifier.isString() ? identifier.toString() : QString();
        if (id.isEmpty() || ids.contains(id) || !std::isfinite(rect.x()) || !std::isfinite(rect.y()) ||
            !std::isfinite(rect.width()) || !std::isfinite(rect.height()) || rect.isEmpty() ||
            !QRectF(-0.000001, -0.000001, 1.000002, 1.000002).contains(rect)) return false;
        ids.insert(id);
        parsed.append({rect.intersected(QRectF(0, 0, 1, 1)), id});
    }
    layout = parsed;
    return true;
}

bool tileWindow(KDecoration3::Decoration *decoration, Qt::Edges edges)
{
    const int mask = int(edges);
    if (!canTileWindow(decoration) || (mask != 1 && mask != 2 && mask != 4 && mask != 8 && mask != 3 && mask != 5 && mask != 10 && mask != 12)) return false;
    const auto result = callScript(decoration, QStringLiteral("materialTiling"), {{"operation", "tile"}, {"edges", mask}});
    return result.isBool() && result.toBool();
}

bool tileCustomWindow(KDecoration3::Decoration *decoration, const QString &tileId)
{
    if (!canTileWindow(decoration) || tileId.isEmpty()) return false;
    const auto result = callScript(decoration, QStringLiteral("materialTiling"), {{"operation", "tileCustom"}, {"tileId", tileId}});
    return result.isBool() && result.toBool();
}
}
#include "TilingController.moc"
