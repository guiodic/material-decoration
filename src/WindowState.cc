// SPDX-License-Identifier: GPL-2.0-or-later
#include "WindowState.h"
#include "WindowStateScope.h"
#include "TilingController.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KDecoration3/Decoration>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
#include <QMetaMethod>
#include <QPointer>
#include <QRectF>
#include <QTimer>
#include <cmath>

namespace Material
{
namespace
{
QVariantMap position(const QVariant &value)
{
    // KWin 6.8 uses a Q_GADGET KWin::RectF rather than QRectF.
    const QMetaObject *meta = value.metaType().metaObject();
    QVariantMap result;
    for (const char *name : {"x", "y"}) {
        const int index = meta ? meta->indexOfProperty(name) : -1;
        if (index < 0) {
            const QRectF r = value.toRectF();
            return value.isValid() ? QVariantMap{{"x", r.x()}, {"y", r.y()}} : QVariantMap();
        }
        result.insert(QLatin1String(name), meta->property(index).readOnGadget(value.constData()));
    }
    return result;
}

bool validPosition(const QVariantMap &g)
{
    for (const char *name : {"x", "y"}) {
        bool ok = false;
        const double n = g.value(QLatin1String(name)).toDouble(&ok);
        if (!ok || !std::isfinite(n)) return false;
    }
    return true;
}

QVariantMap tileRegion(const QVariant &value)
{
    auto result = position(value);
    const auto *meta = value.metaType().metaObject();
    for (const char *name : {"width", "height"}) {
        const int index = meta ? meta->indexOfProperty(name) : -1;
        if (index >= 0) result.insert(QLatin1String(name), meta->property(index).readOnGadget(value.constData()));
        else result.insert(QLatin1String(name), QByteArray(name) == "width" ? value.toRectF().width() : value.toRectF().height());
    }
    return result;
}

int quickTileEdges(const QVariantMap &region)
{
    constexpr double epsilon = 0.00001;
    const bool left = std::abs(region.value("x").toDouble()) < epsilon;
    const bool right = std::abs(region.value("x").toDouble() + region.value("width").toDouble() - 1) < epsilon;
    const bool top = std::abs(region.value("y").toDouble()) < epsilon;
    const bool bottom = std::abs(region.value("y").toDouble() + region.value("height").toDouble() - 1) < epsilon;
    return (left && !right ? 2 : 0) | (right && !left ? 4 : 0) | (top && !bottom ? 1 : 0) | (bottom && !top ? 8 : 0);
}

QVariantMap windowTile(const QObject *window)
{
    auto *tile = window->property("tile").value<QObject *>();
    if (!tile) {
        // On an inactive desktop requestedTile is null, but the native tile
        // still owns the window. Look up that association without activating it.
        if (auto *output = window->property("output").value<QObject *>()) {
            for (auto *candidate : output->findChildren<QObject *>()) {
                if (!candidate->inherits("KWin::Tile")) continue;
                for (const auto &owner : candidate->property("windows").toList()) {
                    if (owner.value<QObject *>() == window) tile = candidate;
                }
                if (tile) break;
            }
        }
    }
    if (!tile) return {};
    auto *parent = tile->property("parent").value<QObject *>();
    const auto region = tileRegion(tile->property("relativeGeometry"));
    if (parent && parent->inherits("KWin::QuickRootTile")) return {{"kind", "quick"}, {"edges", quickTileEdges(region)}};
    QVariantList path;
    auto *node = tile;
    while (parent && path.size() < 32) {
        const auto children = parent->property("tiles").toList();
        int index = -1;
        for (int i = 0; i < children.size(); ++i) if (children[i].value<QObject *>() == node) index = i;
        if (index < 0) return {};
        path.prepend(index);
        node = parent;
        parent = node->property("parent").value<QObject *>();
    }
    if (parent || path.isEmpty()) return {};
    return {{"kind", "custom"}, {"path", path}, {"region", region}};
}

// Tile region metadata belongs to the compositor's layout and does not impose
// a size on floating windows.
QVariantMap positionState(const QVariantMap &state)
{
    if (state.value("version").toInt() != 3) return {};
    const auto coordinates = state.value("position").toMap();
    if (!validPosition(coordinates)) return {};
    QVariantMap saved{{"version", 3}, {"position", QVariantMap{{"x", coordinates.value("x")}, {"y", coordinates.value("y")}}},
                      {"desktopIds", state.value("desktopIds")}, {"onAllDesktops", state.value("onAllDesktops").toBool()},
                      {"outputName", state.value("outputName")}, {"outputUuid", state.value("outputUuid")}};
    const auto output = state.value("outputPosition").toMap();
    if (validPosition(output)) saved.insert("outputPosition", QVariantMap{{"x", output.value("x")}, {"y", output.value("y")}});
    const auto tile = state.value("tile").toMap();
    const QString kind = tile.value("kind").toString();
    if (kind == QLatin1String("quick")) {
        const int edges = tile.value("edges").toInt();
        if (QList<int>{1, 2, 4, 8, 3, 5, 10, 12}.contains(edges)) saved.insert("tile", QVariantMap{{"kind", kind}, {"edges", edges}});
    } else if (kind == QLatin1String("custom")) {
        const auto path = tile.value("path").toList();
        const auto region = tile.value("region").toMap();
        const double w = region.value("width").toDouble(), h = region.value("height").toDouble();
        bool valid = !path.isEmpty() && path.size() <= 32 && validPosition(region) && std::isfinite(w) && std::isfinite(h)
            && w > 0 && h > 0 && region.value("x").toDouble() >= 0 && region.value("y").toDouble() >= 0
            && region.value("x").toDouble() + w <= 1.00001 && region.value("y").toDouble() + h <= 1.00001;
        for (const auto &index : path) if (index.toDouble() != index.toInt() || index.toInt() < 0) valid = false;
        if (valid) saved.insert("tile", QVariantMap{{"kind", kind}, {"path", path}, {"region", region}});
    }
    return saved;
}

KSharedConfig::Ptr stateConfig()
{
    return KSharedConfig::openStateConfig(QStringLiteral("material-window-staterc"));
}
}

QString windowStateKey(const QObject *window)
{
    if (!window) return {};
    QString app = window->property("desktopFileName").toString();
    if (app.isEmpty()) app = window->property("resourceClass").toString();
    if (app.isEmpty()) return {};
    // A Wayland tag, when supplied, distinguishes windows of the same app.
    // Otherwise all indistinguishable main windows share the last closed state.
    const QJsonArray identity{app, window->property("windowRole").toString(), window->property("tag").toString()};
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact),
                                                       QCryptographicHash::Sha256).toHex());
}

QVariantMap captureWindowState(const QObject *window)
{
    if (!window || windowStateKey(window).isEmpty()) return {};
    const auto coordinates = position(window->property("frameGeometry"));
    if (!validPosition(coordinates)) return {};
    QStringList desktops;
    const QVariant values = window->property("desktops");
    for (const QVariant &value : values.toList()) {
        if (auto *desktop = value.value<QObject *>()) {
            const QString id = desktop->property("id").toString();
            if (!id.isEmpty()) desktops.append(id);
        }
    }
    QVariantMap state{{"version", 3}, {"position", coordinates}, {"desktopIds", desktops},
                      {"onAllDesktops", window->property("onAllDesktops").toBool()}};
    if (auto *output = window->property("output").value<QObject *>()) {
        state.insert("outputName", output->property("name").toString());
        state.insert("outputUuid", output->property("uuid").toString());
        state.insert("outputPosition", position(output->property("geometry")));
    }
    const auto tile = windowTile(window);
    if (!tile.isEmpty()) state.insert("tile", tile);
    return state;
}

QVariantMap readWindowState(const QString &key, const QString &scope)
{
    if (key.isEmpty()) return {};
    KConfigGroup group(stateConfig(), key);
    if (!scope.isEmpty()) group = group.group(QStringLiteral("Configurations")).group(scope);
    const auto state = QJsonDocument::fromJson(group.readEntry("State", QByteArray())).object().toVariantMap();
    return positionState(state);
}

bool writeWindowState(const QString &key, const QVariantMap &state, const QString &scope)
{
    const auto saved = positionState(state);
    if (key.isEmpty() || saved.isEmpty()) return false;
    const auto config = stateConfig();
    KConfigGroup group(config, key);
    if (!scope.isEmpty()) group = group.group(QStringLiteral("Configurations")).group(scope);
    group.writeEntry("State", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    return config->sync();
}

class WindowStateController : public QObject
{
    Q_OBJECT
public:
    explicit WindowStateController(KDecoration3::Decoration *decoration, bool enabled)
        : QObject(decoration->parent()), m_window(decoration->parent()), m_decoration(decoration), m_enabled(enabled)
    {
        // Read synchronously on closed(), before KWin releases the window.
        connect(m_window, SIGNAL(closed()), this, SLOT(save()), Qt::DirectConnection);
        connect(m_window, SIGNAL(readyForPaintingChanged()), this, SLOT(scheduleRestore()));
        auto *scope = WindowStateScope::self();
        connect(scope, &WindowStateScope::snapshotRequested, this, [this](const QString &oldScope, const QVariantMap &origins) {
            if (!m_enabled || !m_restoreStarted || m_window->property("deleted").toBool()) return;
            auto state = stateToSave();
            const auto origin = origins.value(state.value("outputName").toString()).toMap();
            if (!origin.isEmpty()) state.insert("outputPosition", origin);
            writeWindowState(windowStateKey(m_window), state, oldScope);
        });
        const auto slot = metaObject()->method(metaObject()->indexOfSlot("updateTile()"));
        for (int i = 0; i < m_window->metaObject()->methodCount(); ++i) {
            const auto signal = m_window->metaObject()->method(i);
            if (signal.methodType() == QMetaMethod::Signal && signal.name() == "tileChanged") {
                connect(m_window, signal, this, slot, Qt::DirectConnection);
                break;
            }
        }
        m_originalDesktops = m_window->property("desktops");
        prepare();
        // Initialization may still apply the app's initial desktop/minimized
        // state after creating the decoration. Reassert before its first buffer.
        QTimer::singleShot(0, this, &WindowStateController::prepare);
    }
    void configure(KDecoration3::Decoration *decoration, bool enabled)
    {
        m_decoration = decoration;
        m_enabled = enabled;
        if (!enabled && !m_restoreStarted) {
            if (m_prepared && m_originalDesktops.isValid()) m_window->setProperty("desktops", m_originalDesktops);
            releaseVisibilityGuard();
            m_prepared = false;
        }
    }

private slots:
    void prepare()
    {
        if (!m_enabled || m_restoreStarted || !m_decoration || m_window->property("deleted").toBool()) return;
        if (m_state.isEmpty()) m_state = readWindowState(windowStateKey(m_window), WindowStateScope::self()->current());
        if (m_state.isEmpty()) return;
        const bool wasMinimized = m_window->property("minimized").toBool();
        m_prepared = prepareWindowStateImmediately(m_decoration, m_state) || m_prepared;
        m_visibilityGuard = m_visibilityGuard || (!wasMinimized && m_window->property("minimized").toBool());
    }
    void scheduleRestore()
    {
        if (m_restoreStarted) return;
        m_restoreStarted = true;
        // KWin has placed and added the window while handling this signal.
        // Restore synchronously before returning to the render loop; queuing a
        // timer or D-Bus round trip here exposes the initial centered frame.
        if (m_enabled) restore();
        releaseVisibilityGuard();
    }
    void save()
    {
        if (m_enabled) {
            writeWindowState(windowStateKey(m_window), stateToSave(), WindowStateScope::self()->current());
        }
    }
    void updateTile()
    {
        if (!m_window->property("deleted").toBool()) m_lastTile = windowTile(m_window);
    }
    void restore()
    {
        if (!m_enabled || !m_decoration || m_window->property("deleted").toBool()) return;
        const QString key = windowStateKey(m_window);
        // Initialization and script registration can finish after Decoration::init().
        if (key.isEmpty()) {
            retry();
            return;
        }
        if (m_state.isEmpty()) m_state = readWindowState(key, WindowStateScope::self()->current());
        if (m_state.isEmpty()) return;
        if (restoreWindowStateImmediately(m_decoration, m_state)) {
            updateTile();
            return;
        }
        retry();
    }

private:
    QVariantMap stateToSave() const
    {
        auto state = captureWindowState(m_window);
        if (!state.contains("tile") && !m_lastTile.isEmpty()) state.insert("tile", m_lastTile);
        return state;
    }
    void releaseVisibilityGuard()
    {
        if (!m_visibilityGuard) return;
        m_visibilityGuard = false;
        // Our temporary minimization only prevents KWin's initial activation
        // from bringing an off-desktop window onto the current desktop.
        m_window->setProperty("minimized", false);
    }
    void retry()
    {
        // Four retries at 250 ms: stop after about one second.
        if (++m_attempts <= 4) QTimer::singleShot(250, this, &WindowStateController::restore);
    }
    QObject *m_window;
    QPointer<KDecoration3::Decoration> m_decoration;
    bool m_enabled;
    bool m_restoreStarted = false;
    bool m_prepared = false;
    bool m_visibilityGuard = false;
    QVariant m_originalDesktops;
    QVariantMap m_state;
    QVariantMap m_lastTile;
    int m_attempts = 0;
};

void configureWindowState(KDecoration3::Decoration *decoration, bool enabled)
{
    auto *window = decoration->parent();
    // Do not affect XWayland, dialogs, popups or the KCM's preview.
    if (!window || !window->inherits("KWin::WaylandWindow") || !window->property("normalWindow").toBool()
        || window->property("transient").toBool()) return;
    if (auto *controller = window->findChild<WindowStateController *>(QString(), Qt::FindDirectChildrenOnly)) {
        controller->configure(decoration, enabled);
    } else {
        // Owned by the KWin window, so fullscreen/decoration removal cannot lose
        // the close handler. No geometry or state changes are monitored.
        new WindowStateController(decoration, enabled);
    }
}
}

#include "WindowState.moc"
