// SPDX-License-Identifier: GPL-2.0-or-later
#include "WindowState.h"
#include "TilingController.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <KDecoration3/Decoration>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
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

// Version 1 also stored dimensions and window modes. Import only its
// coordinates; every subsequent write uses the position-only format.
QVariantMap positionState(const QVariantMap &state)
{
    const int version = state.value("version").toInt();
    if (version != 1 && version != 2) return {};
    const auto coordinates = state.value(version == 1 ? "geometry" : "position").toMap();
    if (!validPosition(coordinates)) return {};
    QVariantMap saved{{"version", 2}, {"position", QVariantMap{{"x", coordinates.value("x")}, {"y", coordinates.value("y")}}},
                      {"desktopIds", state.value("desktopIds")}, {"onAllDesktops", state.value("onAllDesktops").toBool()},
                      {"outputName", state.value("outputName")}, {"outputUuid", state.value("outputUuid")}};
    const auto output = state.value(version == 1 ? "outputGeometry" : "outputPosition").toMap();
    if (validPosition(output)) saved.insert("outputPosition", QVariantMap{{"x", output.value("x")}, {"y", output.value("y")}});
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
    QVariantMap state{{"version", 2}, {"position", coordinates}, {"desktopIds", desktops},
                      {"onAllDesktops", window->property("onAllDesktops").toBool()}};
    if (auto *output = window->property("output").value<QObject *>()) {
        state.insert("outputName", output->property("name").toString());
        state.insert("outputUuid", output->property("uuid").toString());
        state.insert("outputPosition", position(output->property("geometry")));
    }
    return state;
}

QVariantMap readWindowState(const QString &key)
{
    if (key.isEmpty()) return {};
    const auto state = QJsonDocument::fromJson(KConfigGroup(stateConfig(), key).readEntry("State", QByteArray())).object().toVariantMap();
    return positionState(state);
}

bool writeWindowState(const QString &key, const QVariantMap &state)
{
    const auto saved = positionState(state);
    if (key.isEmpty() || saved.isEmpty()) return false;
    const auto config = stateConfig();
    KConfigGroup(config, key).writeEntry("State", QJsonDocument::fromVariant(saved).toJson(QJsonDocument::Compact));
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
    }
    void configure(KDecoration3::Decoration *decoration, bool enabled)
    {
        m_decoration = decoration;
        m_enabled = enabled;
    }

private slots:
    void scheduleRestore()
    {
        if (!m_enabled || m_restoreStarted) return;
        m_restoreStarted = true;
        // KWin has placed and added the window while handling this signal.
        // Restore synchronously before returning to the render loop; queuing a
        // timer or D-Bus round trip here exposes the initial centered frame.
        restore();
    }
    void save()
    {
        if (m_enabled) writeWindowState(windowStateKey(m_window), captureWindowState(m_window));
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
        const auto state = readWindowState(key);
        if (state.isEmpty()) return;
        if (restoreWindowStateImmediately(m_decoration, state)) return;
        restoreWindowState(m_decoration, state, [this](bool success) {
            if (!success) retry();
        });
    }

private:
    void retry()
    {
        if (++m_attempts < 20) QTimer::singleShot(250, this, &WindowStateController::restore);
    }
    QObject *m_window;
    QPointer<KDecoration3::Decoration> m_decoration;
    bool m_enabled;
    bool m_restoreStarted = false;
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
