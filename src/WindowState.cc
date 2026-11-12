// SPDX-License-Identifier: GPL-2.0-or-later
#include "WindowState.h"
#include "WindowStateScope.h"
#include "SettingsProvider.h"
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
#include <QRectF>
#include <QScopedValueRollback>
#include <QSet>
#include <QTimer>
#include <cmath>

namespace KWin { class Tile; }
Q_DECLARE_OPAQUE_POINTER(KWin::Tile *)

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

QVariantMap normalSize(const QVariant &geometry)
{
    const auto rectangle = tileRegion(geometry);
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
    if (parent && parent->inherits("KWin::QuickRootTile")) {
        int edges = quickTileEdges(region);
        // KWin 6.8 creates these children in mode order. Geometry alone can
        // misidentify a half as a quarter after resizing its outer edge.
        constexpr int modes[]{2, 4, 1, 8, 3, 5, 10, 12};
        const auto siblings = parent->property("tiles").toList();
        if (siblings.size() == 8) {
            for (int i = 0; i < 8; ++i) if (siblings[i].value<QObject *>() == tile) edges = modes[i];
        }
        return {{"kind", "quick"}, {"edges", edges}, {"region", region}};
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
                      {"outputName", state.value("outputName")}, {"outputUuid", state.value("outputUuid")}};
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

QString stateIdentity(const QString &windowKey, const QString &scope)
{
    return windowKey + QLatin1Char(':') + scope;
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
    explicit WindowStateController(KDecoration3::Decoration *decoration, bool enabled, bool rememberScreen, bool rememberDesktop)
        : QObject(decoration->parent()), m_window(decoration->parent()), m_decoration(decoration), m_enabled(enabled)
        , m_rememberScreen(rememberScreen), m_rememberDesktop(rememberDesktop)
    {
        // Read synchronously on closed(), before KWin releases the window.
        connect(m_window, SIGNAL(closed()), this, SLOT(save()), Qt::DirectConnection);
        connect(m_window, SIGNAL(readyForPaintingChanged()), this, SLOT(scheduleRestore()));
        connect(m_window, SIGNAL(interactiveMoveResizeStarted()), this, SLOT(cancelRestore()), Qt::DirectConnection);
        connect(m_window, SIGNAL(interactiveMoveResizeFinished()), this, SLOT(updateTile()), Qt::DirectConnection);
        m_retryTimer.setSingleShot(true);
        connect(&m_retryTimer, &QTimer::timeout, this, &WindowStateController::restore);
        m_prepareRetryTimer.setSingleShot(true);
        connect(&m_prepareRetryTimer, &QTimer::timeout, this, &WindowStateController::prepare);
        auto *scope = WindowStateScope::self();
        connect(scope, &WindowStateScope::scopeChanged, this, [this](const QString &newScope) {
            // Capture this for every open window before any sibling can close
            // and create a state in the newly entered configuration.
            if (!m_enabled || m_window->property("deleted").toBool()) return;
            rememberStatePresence(newScope);
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
                m_userAdjustedWindowKey.clear();
            }
        });
        connect(scope, &WindowStateScope::snapshotRequested, this, [this](const QString &oldScope, const QVariantMap &origins) {
            if (!m_enabled || !m_restoreStarted || m_window->property("deleted").toBool()) return;
            auto state = stateToSave();
            saveUserPlacementInPreviousScope(state);
            const auto origin = origins.value(state.value("outputName").toString()).toMap();
            if (!origin.isEmpty()) state.insert("outputPosition", origin);
            saveState(oldScope, state);
            m_previousScope = oldScope;
            m_previousOutputs = origins.keys();
            m_userAdjustedWindowKey.clear();
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
        connectSignal("windowClassChanged", "rememberCurrentIdentity()");
        connectSignal("desktopFileNameChanged", "rememberCurrentIdentity()");
        connectSignal("windowRoleChanged", "rememberCurrentIdentity()");
        connectSignal("tagChanged", "rememberCurrentIdentity()");
        connectSignal("interactiveMoveResizeStepped", "userPlacementStepped()");
        connectSignal("tileChanged", "committedTileChanged(KWin::Tile*)");
        connectSignal("requestedTileChanged", "requestedTileChanged()");
        connectSignal("frameGeometryAboutToChange", "rememberNormalSize()");
        connectSignal("frameGeometryChanged", "queueNormalSizeUpdate()");
        connectSignal("maximizedAboutToChange", "maximizeStarting()");
        connectSignal("maximizedChanged", "maximizeFinished()");
        connectSignal("fullScreenChanged", "queueNormalSizeUpdate()");
        m_originalDesktops = m_window->property("desktops");
        prepare();
        // Initialization may still apply the app's initial desktop/minimized
        // state after creating the decoration. Reassert before its first buffer.
        QTimer::singleShot(0, this, &WindowStateController::prepare);
    }
    void configure(KDecoration3::Decoration *decoration, bool enabled, bool rememberScreen, bool rememberDesktop)
    {
        m_decoration = decoration;
        m_enabled = enabled;
        m_rememberScreen = rememberScreen;
        m_rememberDesktop = rememberDesktop;
        if (!enabled || !rememberDesktop) m_prepareRetryTimer.stop();
        if ((!enabled || !rememberDesktop) && !m_restoreStarted) {
            if (m_prepared && m_originalDesktops.isValid()) m_window->setProperty("desktops", m_originalDesktops);
            releaseVisibilityGuard();
            m_prepared = false;
        }
    }

private slots:
    void prepare()
    {
        if (!m_enabled || m_restoreCancelled || m_restoreStarted || m_prepareFailed || !m_decoration
            || m_prepareAttempts >= 100 || m_window->property("deleted").toBool()) return;
        m_prepareRetryTimer.stop();
        refreshState();
        if (!m_rememberDesktop || m_state.isEmpty()) return;
        const bool wasMinimized = m_window->property("minimized").toBool();
        ++m_prepareAttempts;
        const auto result = prepareWindowStateImmediately(m_decoration, restorationRequest(m_state));
        m_prepared = result == WindowRestoreResult::Succeeded || m_prepared;
        m_prepareFailed = result == WindowRestoreResult::Failed;
        m_visibilityGuard = m_visibilityGuard || (!wasMinimized && m_window->property("minimized").toBool());
        // A short pre-map interval covers script startup without delaying the
        // first buffer. Stop on success/error; the initial zero-timer reassertion
        // above is deliberately retained even after an immediate success.
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
    void rememberCurrentIdentity()
    {
        if (m_window->property("deleted").toBool()) return;
        // Match KWin's DecoratedWindow::windowClass() even if the decoration
        // has been removed. Eligibility must follow the current class/title.
        const QString windowClass = m_window->property("resourceName").toString() + QLatin1Char(' ')
            + m_window->property("resourceClass").toString();
        const auto settings = SettingsProvider::self()->internalSettings(windowClass, m_window->property("caption").toString());
        configure(m_decoration, settings->rememberWindowState(), settings->rememberWindowScreen(), settings->rememberWindowDesktop());
        // Observe entry into an identity before a sibling can create its state.
        // An identity change does not trigger another restore of a mapped window.
        if (m_enabled) rememberStatePresence(WindowStateScope::self()->current());
    }
    void userPlacementStepped()
    {
        if (!m_enabled || !m_restoreStarted || m_restoring) return;
        const auto scope = WindowStateScope::self()->current();
        const auto key = windowStateKey(m_window);
        m_userModifiedStates.insert(stateIdentity(key, scope));
        if (!m_previousScope.isEmpty() && scope != m_previousScope) m_userAdjustedWindowKey = key;
    }
    void updateTile()
    {
        if (!m_window->property("deleted").toBool()) m_lastTile = windowTile(m_window);
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
    void cancelRestore()
    {
        // An interactive move/resize is an explicit user decision. Never let
        // a late script startup override it, even after the interaction ends.
        m_restoreCancelled = true;
        if (m_enabled) m_userModifiedStates.insert(stateIdentity(windowStateKey(m_window), WindowStateScope::self()->current()));
        m_retryTimer.stop();
        m_prepareRetryTimer.stop();
        releaseVisibilityGuard();
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
            m_restoredStates.insert(stateIdentity(m_stateKey, m_stateScope));
            updateTile();
        } else if (result == WindowRestoreResult::Unavailable) retry();
    }

private:
    QVariantMap restorationRequest(QVariantMap state) const
    {
        // Policy belongs to the current settings, never to the persisted state.
        state.insert("rememberScreen", m_rememberScreen);
        state.insert("rememberDesktop", m_rememberDesktop);
        return state;
    }
    void refreshState()
    {
        const auto scope = WindowStateScope::self()->current();
        const auto key = windowStateKey(m_window);
        if (m_stateScope != scope || m_stateKey != key) {
            // Preparation may already have moved the window to a desktop from
            // the previous topology. Undo it even if the new scope has no state.
            if (m_prepared && m_originalDesktops.isValid()) {
                auto original = m_originalDesktops;
                if (auto *workspace = windowStateWorkspace()) {
                    QList<QObject *> live;
                    for (const auto &value : workspace->property("desktops").toList()) live.append(value.value<QObject *>());
                    QList<QObject *> desktops;
                    for (const auto &value : original.toList()) {
                        auto *desktop = value.value<QObject *>();
                        if (desktop && live.contains(desktop)) desktops.append(desktop);
                    }
                    // An empty original list means all desktops; a deleted
                    // original desktop instead falls back to the current one.
                    if (desktops.isEmpty() && !original.toList().isEmpty()) {
                        if (auto *desktop = workspace->property("currentDesktop").value<QObject *>()) desktops.append(desktop);
                    }
                    original = QVariant::fromValue(desktops);
                }
                m_window->setProperty("desktops", original);
            }
            releaseVisibilityGuard();
            m_prepared = false;
            m_prepareFailed = false;
            m_state.clear();
            m_stateScope = scope;
            m_stateKey = key;
        }
        if (m_state.isEmpty()) m_state = readWindowState(key, scope);
        const auto identity = stateIdentity(key, scope);
        if (!m_statePresentOnEntry.contains(identity)) m_statePresentOnEntry.insert(identity, !m_state.isEmpty());
    }
    QString rememberStatePresence(const QString &scope)
    {
        const auto key = windowStateKey(m_window);
        const auto identity = stateIdentity(key, scope);
        if (!m_statePresentOnEntry.contains(identity)) {
            m_statePresentOnEntry.insert(identity, !readWindowState(key, scope).isEmpty());
        }
        return identity;
    }
    bool canSave(const QString &scope)
    {
        // Preserve an existing placement if startup restoration failed or is
        // still waiting for the script. New states and explicit user moves or
        // resizes remain writable; use the same policy for topology snapshots.
        const auto identity = rememberStatePresence(scope);
        // Other windows sharing this identity may have saved since we opened.
        // Protect only the state we encountered, not their subsequent writes.
        return m_restoredStates.contains(identity) || m_userModifiedStates.contains(identity) || m_savedStates.contains(identity)
            || !m_statePresentOnEntry.value(identity);
    }
    void saveState(const QString &scope, const QVariantMap &state)
    {
        const auto key = windowStateKey(m_window);
        if (canSave(scope) && writeWindowState(key, state, scope)) {
            // A first snapshot made by this window is not an unrestored state
            // from a previous opening; its later saves remain legitimate.
            m_savedStates.insert(stateIdentity(key, scope));
        }
    }
    void saveUserPlacementInPreviousScope(const QVariantMap &state)
    {
        const auto key = windowStateKey(m_window);
        if (m_userAdjustedWindowKey.isEmpty() || m_userAdjustedWindowKey != key || m_previousScope.isEmpty()
            || !m_previousOutputs.contains(state.value("outputName").toString())) return;
        // This deliberate placement also authorizes the preceding configuration;
        // unrelated configurations remain protected.
        m_userModifiedStates.insert(stateIdentity(key, m_previousScope));
        // Keep the current output origin: restoration translates this position
        // if the surviving screen returns to a different place in the old layout.
        saveState(m_previousScope, state);
    }
    QVariantMap siblingNormalSize() const
    {
        // Only correct a floating startup frame that looks inherited from a
        // live native tile of the same application/role/tag. The normal size comes
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
        if (state.contains("tile") && validSize(m_normalSize)) {
            auto tile = state.value("tile").toMap();
            tile.insert("normalSize", m_normalSize);
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
    bool m_restoreStarted = false;
    QSet<QString> m_restoredStates;
    QSet<QString> m_userModifiedStates;
    QSet<QString> m_savedStates;
    QHash<QString, bool> m_statePresentOnEntry;
    QString m_previousScope;
    QStringList m_previousOutputs;
    QString m_userAdjustedWindowKey;
    bool m_restoreCancelled = false;
    QTimer m_retryTimer;
    bool m_prepared = false;
    bool m_prepareFailed = false;
    int m_prepareAttempts = 0;
    QTimer m_prepareRetryTimer;
    bool m_visibilityGuard = false;
    QVariant m_originalDesktops;
    QString m_stateScope;
    QString m_stateKey;
    QVariantMap m_state;
    QVariantMap m_lastTile;
    QVariantMap m_normalSize;
    QTimer m_normalSizeTimer;
    bool m_requestedTiled = false;
    bool m_committedTiled = false;
    bool m_maximizeChanging = false;
    bool m_restoring = false;
    int m_attempts = 0;
};

void configureWindowState(KDecoration3::Decoration *decoration, bool enabled, bool rememberScreen, bool rememberDesktop)
{
    auto *window = decoration->parent();
    // Do not affect XWayland, dialogs, popups or the KCM's preview.
    if (!window || !window->inherits("KWin::WaylandWindow") || !window->property("normalWindow").toBool()
        || window->property("transient").toBool()) return;
    if (auto *controller = window->findChild<WindowStateController *>(QString(), Qt::FindDirectChildrenOnly)) {
        controller->configure(decoration, enabled, rememberScreen, rememberDesktop);
    } else {
        // Owned by the KWin window, so fullscreen/decoration removal cannot lose
        // the close handler. Normal sizes are tracked in memory, never polled.
        new WindowStateController(decoration, enabled, rememberScreen, rememberDesktop);
    }
}
}

#include "WindowState.moc"
