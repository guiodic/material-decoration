// SPDX-License-Identifier: GPL-2.0-or-later
#include "WindowState.h"
#include "WindowStateScope.h"
#include "TilingController.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KDecoration3/Decoration>
#include <QCryptographicHash>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
#include <QMetaMethod>
#include <QPointer>
#include <QQmlProperty>
#include <QRectF>
#include <QScopedValueRollback>
#include <QSet>
#include <QTimer>
#include <cmath>

namespace KWin { class Tile; }
Q_DECLARE_OPAQUE_POINTER(KWin::Tile *)

namespace Material
{
QVariantMap windowRectangle(const QVariant &value)
{
    if (!value.isValid()) return {};
    // KWin's rectangle gadgets expose the same coordinates as QRectF.
    const auto *meta = value.metaType().metaObject();
    const QRectF rect = value.toRectF();
    QVariantMap result{{"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()}};
    for (auto it = result.begin(); it != result.end(); ++it) {
        const int index = meta ? meta->indexOfProperty(it.key().toLatin1().constData()) : -1;
        if (index >= 0) it.value() = meta->property(index).readOnGadget(value.constData());
    }
    return result;
}

namespace
{
QVariantMap position(const QVariant &value)
{
    const auto rect = windowRectangle(value);
    return rect.isEmpty() ? QVariantMap() : QVariantMap{{"x", rect.value("x")}, {"y", rect.value("y")}};
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

QVariantMap normalSize(const QVariant &geometry)
{
    const auto rectangle = windowRectangle(geometry);
    return {{"width", rectangle.value("width")}, {"height", rectangle.value("height")}};
}

bool validSize(const QVariantMap &size)
{
    for (const char *name : {"width", "height"}) {
        bool ok = false;
        const double value = size.value(QLatin1String(name)).toDouble(&ok);
        if (!ok || !std::isfinite(value) || value <= 0) return false;
    }
    return true;
}

bool sameSize(const QVariantMap &a, const QVariantMap &b)
{
    // KDE persists integral sizes; frame borders/scaling can add a fraction.
    return validSize(a) && validSize(b)
        && std::abs(a.value("width").toDouble() - b.value("width").toDouble()) <= 2
        && std::abs(a.value("height").toDouble() - b.value("height").toDouble()) <= 2;
}

bool validTileRegion(const QVariantMap &region)
{
    const double x = region.value("x").toDouble(), y = region.value("y").toDouble();
    const double w = region.value("width").toDouble(), h = region.value("height").toDouble();
    return validPosition(region) && std::isfinite(w) && std::isfinite(h) && w > 0 && h > 0
        && x >= 0 && y >= 0 && x + w <= 1.00001 && y + h <= 1.00001;
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
    const auto region = windowRectangle(tile->property("relativeGeometry"));
    if (parent && parent->inherits("KWin::QuickRootTile")) {
        // KWin 6.8 creates these children in mode order. Geometry alone can
        // misidentify a half as a quarter after resizing its outer edge.
        constexpr int modes[]{2, 4, 1, 8, 3, 5, 10, 12};
        const auto siblings = parent->property("tiles").toList();
        if (siblings.size() != 8) return {};
        for (int i = 0; i < 8; ++i) {
            if (siblings[i].value<QObject *>() == tile) return {{"kind", "quick"}, {"edges", modes[i]}, {"region", region}};
        }
        return {};
    }
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

// Dimensions are retained only as tile metadata for subsequent unsnapping.
// Floating window sizes remain under the application's control.
QVariantMap positionState(const QVariantMap &state)
{
    if (state.value("version").toInt() != 3) return {};
    const auto coordinates = state.value("position").toMap();
    if (!validPosition(coordinates)) return {};
    QVariantMap saved{{"version", 3}, {"position", QVariantMap{{"x", coordinates.value("x")}, {"y", coordinates.value("y")}}},
                      {"desktopIds", state.value("desktopIds")}, {"onAllDesktops", state.value("onAllDesktops").toBool()},
                      {"outputName", state.value("outputName")}};
    const auto output = state.value("outputPosition").toMap();
    if (validPosition(output)) saved.insert("outputPosition", QVariantMap{{"x", output.value("x")}, {"y", output.value("y")}});
    const auto tile = state.value("tile").toMap();
    const QString kind = tile.value("kind").toString();
    if (kind == QLatin1String("quick")) {
        const int edges = tile.value("edges").toInt();
        if (QList<int>{1, 2, 4, 8, 3, 5, 10, 12}.contains(edges)) {
            QVariantMap savedTile{{"kind", kind}, {"edges", edges}};
            const auto region = tile.value("region").toMap();
            if (validTileRegion(region)) {
                QVariantMap normalized;
                for (const char *key : {"x", "y", "width", "height"}) normalized.insert(QLatin1String(key), region.value(QLatin1String(key)).toDouble());
                savedTile.insert("region", normalized);
            }
            saved.insert("tile", savedTile);
        }
    } else if (kind == QLatin1String("custom")) {
        const auto path = tile.value("path").toList();
        const auto region = tile.value("region").toMap();
        bool valid = !path.isEmpty() && path.size() <= 32 && validTileRegion(region);
        for (const auto &index : path) if (index.toDouble() != index.toInt() || index.toInt() < 0) valid = false;
        if (valid) saved.insert("tile", QVariantMap{{"kind", kind}, {"path", path}, {"region", region}});
    }
    const auto size = tile.value("normalSize").toMap();
    if (saved.contains("tile") && validSize(size)) {
        auto savedTile = saved.value("tile").toMap();
        savedTile.insert("normalSize", QVariantMap{{"width", size.value("width").toDouble()}, {"height", size.value("height").toDouble()}});
        saved.insert("tile", savedTile);
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
    const QJsonArray identity{app, window->property("tag").toString()};
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
        state.insert("outputPosition", position(output->property("geometry")));
    }
    const auto tile = windowTile(window);
    if (!tile.isEmpty()) state.insert("tile", tile);
    return state;
}

QVariantMap readWindowState(const QString &key, const QString &scope)
{
    if (key.isEmpty() || scope.isEmpty()) return {};
    const auto group = KConfigGroup(stateConfig(), key).group(QStringLiteral("Configurations")).group(scope);
    const auto state = QJsonDocument::fromJson(group.readEntry("State", QByteArray())).object().toVariantMap();
    return positionState(state);
}

bool writeWindowState(const QString &key, const QVariantMap &state, const QString &scope, bool sync)
{
    const auto saved = positionState(state);
    if (key.isEmpty() || scope.isEmpty() || saved.isEmpty()) return false;
    const auto config = stateConfig();
    auto group = KConfigGroup(config, key).group(QStringLiteral("Configurations")).group(scope);
    group.writeEntry("State", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
    return !sync || config->sync();
}

bool snapshotWindowStates(const QString &scope, const QVariantMap &outputOrigins)
{
    if (scope.isEmpty()) return false;
    // Keep the shared config alive until every direct snapshot handler finishes.
    const auto config = stateConfig();
    Q_EMIT WindowStateScope::self()->snapshotRequested(scope, outputOrigins);
    return config->sync();
}

class WindowStateController : public QObject
{
    Q_OBJECT
public:
    explicit WindowStateController(KDecoration3::Decoration *decoration, bool enabled, bool rememberScreen, bool rememberDesktop, bool resizeOtherTiledWindows)
        : QObject(decoration->parent()), m_window(decoration->parent()), m_decoration(decoration), m_enabled(enabled)
        , m_rememberScreen(rememberScreen), m_rememberDesktop(rememberDesktop), m_resizeOtherTiledWindows(resizeOtherTiledWindows)
    {
        // Read synchronously on closed(), before KWin releases the window.
        connect(m_window, SIGNAL(closed()), this, SLOT(save()), Qt::DirectConnection);
        connect(m_window, SIGNAL(readyForPaintingChanged()), this, SLOT(scheduleRestore()));
        connect(m_window, SIGNAL(interactiveMoveResizeStarted()), this, SLOT(startUserInteraction()), Qt::DirectConnection);
        connect(m_window, SIGNAL(interactiveMoveResizeFinished()), this, SLOT(finishUserInteraction()), Qt::DirectConnection);
        m_retryTimer.setSingleShot(true);
        connect(&m_retryTimer, &QTimer::timeout, this, &WindowStateController::restore);
        m_prepareRetryTimer.setSingleShot(true);
        connect(&m_prepareRetryTimer, &QTimer::timeout, this, &WindowStateController::prepare);
        auto *scope = WindowStateScope::self();
        connect(scope, &WindowStateScope::scopeChanged, this, [this](const QString &newScope) {
            // Capture this for every open window before any sibling can close
            // and create a state in the newly entered configuration.
            if (!m_enabled || m_window->property("deleted").toBool()) return;
            rememberSavePermission(newScope);
            // Only carry a deliberate placement back across output removal.
            // Adding a screen or changing desktops keeps independent states.
            auto *workspace = windowStateWorkspace();
            if (!workspace || m_previousScope.isEmpty()) return;
            QSet<QString> outputs;
            for (const auto &value : workspace->property("screens").toList()) {
                if (auto *output = value.value<QObject *>()) outputs.insert(output->property("name").toString());
            }
            bool removed = false;
            for (const auto &name : m_previousOutputs) removed = removed || !outputs.contains(name);
            if (!removed) {
                m_previousScope.clear();
                m_previousOutputs.clear();
                m_userAdjustedAfterRemoval = false;
            }
        });
        connect(scope, &WindowStateScope::snapshotRequested, this, [this](const QString &oldScope, const QVariantMap &origins) {
            if (!m_enabled || !m_restoreStarted || m_window->property("deleted").toBool()) return;
            auto state = stateToSave();
            saveUserPlacementInPreviousScope(state, false);
            const auto origin = origins.value(state.value("outputName").toString()).toMap();
            if (!origin.isEmpty()) state.insert("outputPosition", origin);
            saveState(oldScope, state, false);
            m_previousScope = oldScope;
            m_previousOutputs = origins.keys();
            m_userAdjustedAfterRemoval = false;
        });
        m_normalSizeTimer.setSingleShot(true);
        connect(&m_normalSizeTimer, &QTimer::timeout, this, &WindowStateController::rememberSettledNormalSize);
        m_requestedTiled = m_window->property("tile").value<QObject *>() != nullptr;
        const auto connectSignal = [this](const char *name, const char *slot) {
            const auto destination = metaObject()->method(metaObject()->indexOfSlot(slot));
            for (int i = 0; i < m_window->metaObject()->methodCount(); ++i) {
                const auto signal = m_window->metaObject()->method(i);
                if (signal.methodType() == QMetaMethod::Signal && signal.name() == name) {
                    connect(m_window, signal, this, destination, Qt::DirectConnection);
                    break;
                }
            }
        };
        connectSignal("interactiveMoveResizeStepped", "userPlacementStepped()");
        connectSignal("tileChanged", "committedTileChanged(KWin::Tile*)");
        connectSignal("requestedTileChanged", "requestedTileChanged()");
        connectSignal("frameGeometryAboutToChange", "rememberNormalSize()");
        connectSignal("frameGeometryChanged", "queueNormalSizeUpdate()");
        connectSignal("maximizedAboutToChange", "maximizeStarting()");
        connectSignal("maximizedChanged", "maximizeFinished()");
        connectSignal("fullScreenChanged", "queueNormalSizeUpdate()");
        for (const auto &value : m_window->property("desktops").toList())
            m_originalDesktops.append(value.value<QObject *>());
        // Already mapped windows appear in KWin's stacking order. There is no
        // readyForPainting Qt property, and its signal will not fire again.
        if (auto *workspace = windowStateWorkspace()) {
            for (const auto &value : workspace->property("stackingOrder").toList()) {
                if (value.value<QObject *>() != m_window) continue;
                m_restoreStarted = true;
                m_saveAllowed.insert(scope->current(), true);
                rememberNormalSize();
                updateTile();
                return;
            }
        }
        prepare();
    }
    void configure(KDecoration3::Decoration *decoration, bool enabled, bool rememberScreen, bool rememberDesktop, bool resizeOtherTiledWindows)
    {
        m_decoration = decoration;
        m_enabled = enabled;
        m_rememberScreen = rememberScreen;
        m_rememberDesktop = rememberDesktop;
        m_resizeOtherTiledWindows = resizeOtherTiledWindows;
        if (!enabled || !rememberDesktop) m_prepareRetryTimer.stop();
        if ((!enabled || !rememberDesktop) && !m_restoreStarted) {
            if (m_prepared) {
                QVariantList desktops;
                for (const auto &desktop : m_originalDesktops) if (desktop) desktops.append(QVariant::fromValue(desktop.data()));
                // An empty original list means all desktops. If every original
                // desktop was deleted, leave KWin's current membership intact.
                if (m_originalDesktops.isEmpty() || !desktops.isEmpty())
                    QQmlProperty::write(m_window, QStringLiteral("desktops"), desktops);
            }
            releaseVisibilityGuard();
            m_prepared = false;
        }
    }

private slots:
    void prepare()
    {
        if (!m_enabled || m_restoreCancelled || m_restoreStarted || !m_decoration
            || m_prepareAttempts >= 100 || m_window->property("deleted").toBool()) return;
        m_prepareRetryTimer.stop();
        refreshState();
        if (m_prepareFailed || !m_rememberDesktop || m_state.isEmpty()) return;
        const bool wasMinimized = m_window->property("minimized").toBool();
        ++m_prepareAttempts;
        const auto result = prepareWindowStateImmediately(m_decoration, restorationRequest(m_state));
        m_prepared = result == WindowRestoreResult::Succeeded || m_prepared;
        m_prepareFailed = result == WindowRestoreResult::Failed;
        m_visibilityGuard = m_visibilityGuard || (!wasMinimized && m_window->property("minimized").toBool());
        // A short pre-map interval covers script startup without delaying the
        // first buffer. Stop on success or error.
        if (result == WindowRestoreResult::Unavailable && !m_prepared && m_prepareAttempts < 100) {
            m_prepareRetryTimer.start(50); // At most about five seconds in total.
        }
    }
    void scheduleRestore()
    {
        if (m_restoreStarted) return;
        m_restoreStarted = true;
        m_prepareRetryTimer.stop();
        rememberNormalSize();
        // KWin has placed and added the window while handling this signal.
        // Restore synchronously before returning to the render loop; queuing a
        // timer or D-Bus round trip here exposes the initial centered frame.
        if (m_enabled) restore();
        releaseVisibilityGuard();
    }
    void save()
    {
        m_prepareRetryTimer.stop();
        m_retryTimer.stop();
        if (!m_enabled) return;
        const auto scope = WindowStateScope::self()->current();
        const auto state = stateToSave();
        saveState(scope, state);
        saveUserPlacementInPreviousScope(state);
    }
    void userPlacementStepped()
    {
        if (!m_enabled || !m_restoreStarted || m_restoring) return;
        const auto scope = WindowStateScope::self()->current();
        m_saveAllowed.insert(scope, true);
        if (!m_previousScope.isEmpty() && scope != m_previousScope) m_userAdjustedAfterRemoval = true;
    }
    void updateTile()
    {
        if (m_window->property("deleted").toBool()) return;
        const auto tile = windowTile(m_window);
        if (!m_restoring && tile.value("edges") != m_lastTile.value("edges")) m_customQuickTile.clear();
        m_lastTile = tile;
    }
    void requestedTileChanged()
    {
        m_requestedTiled = m_window->property("tile").value<QObject *>() != nullptr;
        queueNormalSizeUpdate();
    }
    void committedTileChanged(KWin::Tile *tile)
    {
        // Only compare this opaque pointer; no private KWin API is called.
        // The requested property may already refer to a later configure event.
        m_committedTiled = tile != nullptr;
        updateTile();
        queueNormalSizeUpdate();
    }
    void queueNormalSizeUpdate()
    {
        // KWin commits tile/maximize state AFTER frameGeometryChanged. Wait
        // until that same event has finished before classifying the frame.
        if (m_enabled && m_restoreStarted) m_normalSizeTimer.start(0);
    }
    void rememberSettledNormalSize()
    {
        // A maximize request can be superseded before the client commits, so
        // maximizedChanged need not fire. At this point committed properties
        // classify the frame and no transient "about to" flag is needed.
        m_maximizeChanging = false;
        // Resizing a quick tile changes its proportions without rebinding it.
        // Keep the close-cleanup fallback current without changing normal size.
        if (m_requestedTiled || m_committedTiled || !m_lastTile.isEmpty()) updateTile();
        rememberNormalSize();
    }
    void rememberNormalSize()
    {
        if (!m_enabled || !m_restoreStarted || m_restoring || m_requestedTiled || m_committedTiled || m_maximizeChanging
            || m_window->property("deleted").toBool() || m_window->property("fullScreen").toBool()
            || m_window->property("maximizeMode").toInt()) return;
        const auto size = normalSize(m_window->property("frameGeometry"));
        if (validSize(size)) m_normalSize = size;
    }
    void maximizeStarting()
    {
        rememberNormalSize();
        m_maximizeChanging = true;
    }
    void maximizeFinished()
    {
        m_maximizeChanging = false;
        queueNormalSizeUpdate();
    }
    void startUserInteraction()
    {
        m_resizeTile = m_window->property("resize").toBool() ? windowTile(m_window) : QVariantMap();
        // Cancel pending restoration when the user takes control.
        m_restoreCancelled = true;
        if (m_enabled) m_saveAllowed.insert(WindowStateScope::self()->current(), true);
        m_retryTimer.stop();
        m_prepareRetryTimer.stop();
        releaseVisibilityGuard();
    }
    void finishUserInteraction()
    {
        updateTile();
        if (m_enabled && m_resizeTile.contains("edges")
            && m_resizeTile.value("edges") == m_lastTile.value("edges")
            && m_resizeTile.value("region") != m_lastTile.value("region")) m_customQuickTile = m_lastTile;
        m_resizeTile.clear();
    }
    void restore()
    {
        if (!m_enabled || m_restoreCancelled || !m_decoration || m_window->property("deleted").toBool()) return;
        const QString key = windowStateKey(m_window);
        // Initialization and script registration can finish after Decoration::init().
        if (key.isEmpty()) {
            retry();
            return;
        }
        refreshState();
        const auto inheritedSize = siblingNormalSize();
        if (m_state.isEmpty() && inheritedSize.isEmpty()) return;
        auto state = m_state.isEmpty() ? captureWindowState(m_window) : m_state;
        if (!inheritedSize.isEmpty()) state.insert("inheritedTileSize", inheritedSize);
        // Ignore synthetic intermediate sizes while seeding KWin's unsnap
        // geometry. Missing metadata must not turn a tile-sized startup frame
        // into an allegedly known normal size.
        if (m_state.contains("tile")) m_normalSize = m_state.value("tile").toMap().value("normalSize").toMap();
        QScopedValueRollback restoring(m_restoring, true);
        const auto result = restoreWindowStateImmediately(m_decoration, restorationRequest(state));
        if (result == WindowRestoreResult::Succeeded) {
            m_saveAllowed.insert(m_stateScope, true);
            updateTile();
            const auto tile = state.value("tile").toMap();
            m_customQuickTile = tile.value("kind") == QLatin1String("quick") ? tile : QVariantMap();
        } else if (result == WindowRestoreResult::Unavailable) retry();
    }

private:
    QVariantMap restorationRequest(QVariantMap state) const
    {
        // Policy belongs to the current settings, never to the persisted state.
        state.insert("rememberScreen", m_rememberScreen);
        state.insert("rememberDesktop", m_rememberDesktop);
        state.insert("resizeOtherTiledWindows", m_resizeOtherTiledWindows);
        return state;
    }
    void refreshState()
    {
        const auto scope = WindowStateScope::self()->current();
        const auto key = windowStateKey(m_window);
        if (m_stateScope != scope || m_stateKey != key) {
            // After a configuration change, start from the current desktop
            // rather than reconstructing memberships from the old topology.
            if (m_prepared) {
                if (auto *workspace = windowStateWorkspace()) {
                    if (auto *desktop = workspace->property("currentDesktop").value<QObject *>())
                        QQmlProperty::write(m_window, QStringLiteral("desktops"), QVariantList{QVariant::fromValue(desktop)});
                }
            }
            releaseVisibilityGuard();
            m_prepared = false;
            m_prepareFailed = false;
            m_state.clear();
            m_stateScope = scope;
            m_stateKey = key;
        }
        if (m_state.isEmpty()) m_state = readWindowState(key, scope);
        if (!m_saveAllowed.contains(scope)) m_saveAllowed.insert(scope, m_state.isEmpty());
    }
    void rememberSavePermission(const QString &scope)
    {
        if (!m_saveAllowed.contains(scope)) {
            m_saveAllowed.insert(scope, readWindowState(windowStateKey(m_window), scope).isEmpty());
        }
    }
    bool canSave(const QString &scope)
    {
        // Preserve an existing placement if startup restoration failed or is
        // still waiting for the script. New states and explicit user moves or
        // resizes remain writable; use the same policy for topology snapshots.
        rememberSavePermission(scope);
        // Other windows sharing this identity may have saved since we opened.
        // Protect only the state we encountered, not their subsequent writes.
        return m_saveAllowed.value(scope);
    }
    void saveState(const QString &scope, const QVariantMap &state, bool sync = true)
    {
        if (canSave(scope)) writeWindowState(windowStateKey(m_window), state, scope, sync);
    }
    void saveUserPlacementInPreviousScope(const QVariantMap &state, bool sync = true)
    {
        if (!m_userAdjustedAfterRemoval || m_previousScope.isEmpty()
            || !m_previousOutputs.contains(state.value("outputName").toString())) return;
        // This deliberate placement also authorizes the preceding configuration;
        // unrelated configurations remain protected.
        m_saveAllowed.insert(m_previousScope, true);
        // Keep the current output origin: restoration translates this position
        // if the surviving screen returns to a different place in the old layout.
        saveState(m_previousScope, state, sync);
    }
    QVariantMap siblingNormalSize() const
    {
        // Only correct a floating startup frame that looks inherited from a
        // live native tile of the same application/tag. The normal size comes
        // from that window's controller; excluded windows cannot provide it.
        if (m_window->property("tile").value<QObject *>() || m_window->property("fullScreen").toBool()
            || m_window->property("maximizeMode").toInt() || !m_window->property("resizeable").toBool()) return {};
        auto *workspace = windowStateWorkspace();
        if (!workspace) return {};
        const auto initialSize = normalSize(m_window->property("frameGeometry"));
        const auto key = windowStateKey(m_window);
        QVariantMap hint;
        for (const auto &entry : workspace->property("stackingOrder").toList()) {
            auto *window = entry.value<QObject *>();
            if (!window || window == m_window || window->property("deleted").toBool() || window->property("fullScreen").toBool()
                || window->property("maximizeMode").toInt() || windowStateKey(window) != key) continue;
            auto *controller = window->findChild<WindowStateController *>(QString(), Qt::FindDirectChildrenOnly);
            if (!controller || !controller->m_enabled || !controller->m_committedTiled || !controller->m_requestedTiled
                || !validSize(controller->m_normalSize)) continue;
            const auto tileSize = normalSize(window->property("frameGeometry"));
            if (!sameSize(initialSize, tileSize) || sameSize(controller->m_normalSize, tileSize)) continue;
            if (!hint.isEmpty() && !sameSize(hint.value("normalSize").toMap(), controller->m_normalSize)) return {};
            hint = {{"tileSize", tileSize}, {"normalSize", controller->m_normalSize}};
        }
        return hint;
    }
    QVariantMap stateToSave() const
    {
        auto state = captureWindowState(m_window);
        if (!state.contains("tile") && !m_lastTile.isEmpty()) state.insert("tile", m_lastTile);
        if (state.contains("tile")) {
            auto tile = state.value("tile").toMap();
            if (tile.value("kind") == QLatin1String("quick")) {
                tile.remove("region");
                if (tile.value("edges") == m_customQuickTile.value("edges") && m_customQuickTile.contains("region"))
                    tile.insert("region", m_customQuickTile.value("region"));
            }
            if (validSize(m_normalSize)) tile.insert("normalSize", m_normalSize);
            state.insert("tile", tile);
        }
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
        // 20 retries at 250 ms: stop after about 5 seconds.
        if (!m_restoreCancelled && ++m_attempts <= 20) m_retryTimer.start(250);
    }
    QObject *m_window;
    QPointer<KDecoration3::Decoration> m_decoration;
    bool m_enabled;
    bool m_rememberScreen;
    bool m_rememberDesktop;
    bool m_resizeOtherTiledWindows;
    bool m_restoreStarted = false;
    // New states are writable; existing ones need a successful restore or a user move.
    QHash<QString, bool> m_saveAllowed;
    QString m_previousScope;
    QStringList m_previousOutputs;
    bool m_userAdjustedAfterRemoval = false;
    bool m_restoreCancelled = false;
    QTimer m_retryTimer;
    bool m_prepared = false;
    bool m_prepareFailed = false;
    int m_prepareAttempts = 0;
    QTimer m_prepareRetryTimer;
    bool m_visibilityGuard = false;
    QList<QPointer<QObject>> m_originalDesktops;
    QString m_stateScope;
    QString m_stateKey;
    QVariantMap m_state;
    QVariantMap m_lastTile;
    QVariantMap m_customQuickTile;
    QVariantMap m_resizeTile;
    QVariantMap m_normalSize;
    QTimer m_normalSizeTimer;
    bool m_requestedTiled = false;
    bool m_committedTiled = false;
    bool m_maximizeChanging = false;
    bool m_restoring = false;
    int m_attempts = 0;
};

void configureWindowState(KDecoration3::Decoration *decoration, bool enabled, bool rememberScreen, bool rememberDesktop, bool resizeOtherTiledWindows)
{
    auto *window = decoration->parent();
    // Do not affect XWayland, dialogs, popups or the KCM's preview.
    if (!window || !window->inherits("KWin::WaylandWindow") || !window->property("normalWindow").toBool()
        || window->property("transient").toBool()) return;
    if (auto *controller = window->findChild<WindowStateController *>(QString(), Qt::FindDirectChildrenOnly)) {
        controller->configure(decoration, enabled, rememberScreen, rememberDesktop, resizeOtherTiledWindows);
    } else {
        // Owned by the KWin window, so fullscreen/decoration removal cannot lose
        // the close handler. Normal sizes are tracked in memory, never polled.
        new WindowStateController(decoration, enabled, rememberScreen, rememberDesktop, resizeOtherTiledWindows);
    }
}
}

#include "WindowState.moc"
