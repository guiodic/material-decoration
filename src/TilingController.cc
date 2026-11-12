// SPDX-License-Identifier: GPL-2.0-or-later
#include "TilingController.h"

#include <KDecoration3/Decoration>
#include <KDecoration3/DecoratedWindow>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
        // Tile::manage() rejects roots on other desktops before changing them.
        // This preserves focus and the current desktop during a quick snap.
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

static QString windowStateJson(KDecoration3::Decoration *decoration, const QVariantMap &state)
{
    auto request = state;
    // Runtime placement hint only; never persisted in the window's state.
    request.insert(QStringLiteral("titleBarHeight"), decoration ? decoration->titleBar().height() : 0);
    return QString::fromUtf8(QJsonDocument::fromVariant(request).toJson(QJsonDocument::Compact));
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

static QJSValue callScript(KDecoration3::Decoration *decoration, const QString &name, const QString &json)
{
    if (!decoration || !decoration->parent()) return {};
    auto *engine = scriptEngine(name);
    if (!engine) return {};
    auto function = engine->globalObject().property(name);
    static auto *stateBridge = new WindowStateScriptBridge(QCoreApplication::instance());
    QJSEngine::setObjectOwnership(stateBridge, QJSEngine::CppOwnership);
    engine->globalObject().setProperty(QStringLiteral("materialWindowStateBridge"), engine->newQObject(stateBridge));
    auto *owner = decoration->parent();
    QJSEngine::setObjectOwnership(owner, QJSEngine::CppOwnership);
    // Activation/tiling can destroy the decoration. Do not access it after
    // entering JavaScript; the KWin window is the actual operation target.
    auto result = function.call({engine->newQObject(owner), QJSValue(json)});
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

bool restoreWindowStateImmediately(KDecoration3::Decoration *decoration, const QVariantMap &state)
{
    const auto result = callScript(decoration, QStringLiteral("materialRestoreWindowState"), windowStateJson(decoration, state));
    return result.isBool() && result.toBool();
}

bool prepareWindowStateImmediately(KDecoration3::Decoration *decoration, const QVariantMap &state)
{
    const auto result = callScript(decoration, QStringLiteral("materialPrepareWindowState"), windowStateJson(decoration, state));
    return result.isBool() && result.toBool();
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
    const auto result = callScript(decoration, QStringLiteral("materialTiling"), QStringLiteral("{\"operation\":\"layout\"}"));
    if (!result.isString()) return false;
    const auto document = QJsonDocument::fromJson(result.toString().toUtf8());
    if (!document.isArray()) return false;
    CustomTilingLayout parsed;
    QSet<QString> ids;
    for (const auto &value : document.array()) {
        const auto zone = value.toObject();
        const auto coordinate = [&zone](const char *name) {
            return zone.value(QLatin1String(name)).toDouble(std::numeric_limits<double>::quiet_NaN());
        };
        const QRectF rect(coordinate("x"), coordinate("y"), coordinate("width"), coordinate("height"));
        const auto id = zone.value(QStringLiteral("id")).toString();
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
    const auto json = QJsonDocument(QJsonObject{{"operation", "tile"}, {"edges", mask}}).toJson(QJsonDocument::Compact);
    const auto result = callScript(decoration, QStringLiteral("materialTiling"), QString::fromUtf8(json));
    return result.isBool() && result.toBool();
}

bool tileCustomWindow(KDecoration3::Decoration *decoration, const QString &tileId)
{
    if (!canTileWindow(decoration) || tileId.isEmpty()) return false;
    const auto json = QJsonDocument(QJsonObject{{"operation", "tileCustom"}, {"tileId", tileId}}).toJson(QJsonDocument::Compact);
    const auto result = callScript(decoration, QStringLiteral("materialTiling"), QString::fromUtf8(json));
    return result.isBool() && result.toBool();
}
}
#include "TilingController.moc"
