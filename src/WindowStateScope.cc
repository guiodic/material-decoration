// SPDX-License-Identifier: GPL-2.0-or-later
#include "WindowStateScope.h"
#include "TilingController.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDBusConnection>
#include <QJsonDocument>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QRectF>
#include <QTimer>
#include <algorithm>

namespace Material
{
namespace
{
QVariantMap rectangle(const QVariant &value)
{
    QVariantMap result;
    const auto *meta = value.metaType().metaObject();
    const QRectF rect = value.toRectF();
    const QVariantMap fallback{{"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()}};
    for (const char *name : {"x", "y", "width", "height"}) {
        const int index = meta ? meta->indexOfProperty(name) : -1;
        result.insert(QLatin1String(name), index >= 0 ? meta->property(index).readOnGadget(value.constData()) : fallback.value(QLatin1String(name)));
    }
    return result;
}

QVariantMap configuration(const QObject *workspace)
{
    if (!workspace) return {};
    QVariantList screens;
    for (const auto &value : workspace->property("screens").toList()) {
        if (auto *screen = value.value<QObject *>()) {
            auto row = rectangle(screen->property("geometry"));
            for (const char *name : {"name", "manufacturer", "model", "serialNumber"}) row.insert(QLatin1String(name), screen->property(name).toString());
            const auto scale = screen->property("devicePixelRatio");
            row.insert("scale", scale.isValid() ? scale.toDouble() : 1.0);
            screens.append(row);
        }
    }
    if (screens.isEmpty()) return {};
    std::sort(screens.begin(), screens.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("name").toString() < b.toMap().value("name").toString();
    });
    QStringList desktops;
    for (const auto &value : workspace->property("desktops").toList()) {
        if (auto *desktop = value.value<QObject *>()) desktops.append(desktop->property("id").toString());
    }
    return {{"screens", screens}, {"desktops", desktops},
            {"columns", workspace->property("desktopGridWidth").toInt()}, {"rows", workspace->property("desktopGridHeight").toInt()}};
}

QString signature(const QVariantMap &value)
{
    if (value.isEmpty()) return {};
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}

void connectNamed(QObject *sender, const char *name, QObject *receiver, const char *slot)
{
    if (!sender) return;
    const auto destination = receiver->metaObject()->method(receiver->metaObject()->indexOfSlot(slot));
    for (int i = 0; i < sender->metaObject()->methodCount(); ++i) {
        const auto signal = sender->metaObject()->method(i);
        if (signal.methodType() == QMetaMethod::Signal && signal.name() == name) {
            QObject::connect(sender, signal, receiver, destination, Qt::DirectConnection);
            return;
        }
    }
}
}

QString windowConfigurationScope(const QObject *workspace)
{
    return signature(configuration(workspace));
}

WindowStateScope::WindowStateScope(QObject *parent) : QObject(parent) {}

WindowStateScope *WindowStateScope::self()
{
    static auto *instance = new WindowStateScope(QCoreApplication::instance());
    return instance;
}

QString WindowStateScope::current()
{
    attach();
    return m_scope;
}

void WindowStateScope::attach()
{
    auto *wrapper = windowStateWorkspace();
    if (!wrapper || wrapper == m_wrapper) return;
    m_wrapper = wrapper;
    auto *interface = QDBusConnection::sessionBus().objectRegisteredAt(QStringLiteral("/KWin"));
    m_workspace = interface ? interface->parent() : nullptr;
    // outputRemoved is emitted before KWin evacuates tiles/windows. Geometry
    // notifications precede rearrangement as well. Keep the OLD scope until
    // outputsChanged completes the transaction.
    connectNamed(m_workspace, "outputAdded", this, "changing()");
    connectNamed(m_workspace, "outputRemoved", this, "changing()");
    connectNamed(m_workspace, "outputsChanged", this, "changed()");
    connectNamed(m_wrapper, "desktopLayoutChanged", this, "changing()");
    connectNamed(m_wrapper, "desktopsChanged", this, "changed()");
    changed();
}

void WindowStateScope::watchOutputs()
{
    if (!m_wrapper) return;
    for (const auto &value : m_wrapper->property("screens").toList()) {
        auto *output = value.value<QObject *>();
        if (!output || m_watched.contains(output)) continue;
        m_watched.insert(output);
        connect(output, &QObject::destroyed, this, [this, output] { m_watched.remove(output); });
        connectNamed(output, "geometryChanged", this, "changing()");
        connectNamed(output, "scaleChanged", this, "changing()");
    }
}

void WindowStateScope::changing()
{
    if (m_changing || m_scope.isEmpty()) return;
    m_changing = true;
    Q_EMIT snapshotRequested(m_scope, m_origins);
    // Also handles grid/layout changes without an outputsChanged notification.
    QTimer::singleShot(0, this, &WindowStateScope::changed);
}

void WindowStateScope::changed()
{
    if (!m_wrapper) return;
    const auto context = configuration(m_wrapper);
    m_scope = signature(context);
    m_origins.clear();
    for (const auto &value : context.value("screens").toList()) {
        const auto row = value.toMap();
        m_origins.insert(row.value("name").toString(), QVariantMap{{"x", row.value("x")}, {"y", row.value("y")}});
    }
    m_changing = false;
    watchOutputs();
}
}
