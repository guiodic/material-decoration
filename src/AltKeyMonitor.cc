// SPDX-License-Identifier: GPL-2.0-or-later
#include "AltKeyMonitor.h"
#include "BuildConfig.h"

#include <KModifierKeyInfo>
#include <QGuiApplication>
#include <QPointer>

#if HAVE_WAYLAND
#include <QSocketNotifier>
#include <cerrno>
#include <cstring>
#include <wayland-client.h>
#include "wayland-material-keystate-client-protocol.h"
#endif

namespace Material
{

class AltKeyMonitor::Private
{
public:
    explicit Private(AltKeyMonitor *owner)
        : q(owner)
    {
        const QString platform = QGuiApplication::platformName();
        if (platform == QLatin1String("xcb")) {
            auto *keys = new KModifierKeyInfo(q);
            QObject::connect(keys, &KModifierKeyInfo::keyPressed, q, [owner](Qt::Key key, bool pressed) {
                if (key == Qt::Key_Alt) owner->setAltPressed(pressed);
            });
            q->setAltPressed(keys->isKeyPressed(Qt::Key_Alt));
            return;
        }
#if HAVE_WAYLAND
        // KWin's QPA backend does not deliver the application's modifier events
        // to Qt. Observe the compositor's keystate protocol on a separate connection.
        if (platform != QLatin1String("kwin") && !platform.startsWith(QLatin1String("wayland"))) return;
        display = wl_display_connect(nullptr);
        if (!display) return;
        registry = wl_display_get_registry(display);
        static const wl_registry_listener registryListener = {globalAdded, globalRemoved};
        wl_registry_add_listener(registry, &registryListener, this);
        readNotifier = std::make_unique<QSocketNotifier>(wl_display_get_fd(display), QSocketNotifier::Read, q);
        writeNotifier = std::make_unique<QSocketNotifier>(wl_display_get_fd(display), QSocketNotifier::Write, q);
        writeNotifier->setEnabled(false);
        QObject::connect(readNotifier.get(), &QSocketNotifier::activated, q, [this] {
            if (wl_display_dispatch(display) < 0) {
                disconnectDisplay();
                q->setAltPressed(false);
            } else {
                flush();
            }
        });
        QObject::connect(writeNotifier.get(), &QSocketNotifier::activated, q, [this] { flush(); });
        flush();
#endif
    }

    ~Private()
    {
#if HAVE_WAYLAND
        disconnectDisplay();
#endif
    }

    AltKeyMonitor *q;
#if HAVE_WAYLAND
    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    org_kde_kwin_keystate *keys = nullptr;
    uint32_t globalName = 0;
    std::unique_ptr<QSocketNotifier> readNotifier;
    std::unique_ptr<QSocketNotifier> writeNotifier;

    static void globalAdded(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
    {
        auto *self = static_cast<Private *>(data);
        if (self->keys || std::strcmp(interface, "org_kde_kwin_keystate") != 0 || version < 5) return;
        self->globalName = name;
        self->keys = static_cast<org_kde_kwin_keystate *>(wl_registry_bind(registry, name, &org_kde_kwin_keystate_interface, 5));
        static const org_kde_kwin_keystate_listener listener = {stateChanged};
        org_kde_kwin_keystate_add_listener(self->keys, &listener, self);
        org_kde_kwin_keystate_fetchStates(self->keys);
    }

    static void globalRemoved(void *data, wl_registry *, uint32_t name)
    {
        auto *self = static_cast<Private *>(data);
        if (!self->keys || self->globalName != name) return;
        org_kde_kwin_keystate_destroy(self->keys);
        self->keys = nullptr;
        self->globalName = 0;
        self->q->setAltPressed(false);
    }

    static void stateChanged(void *data, org_kde_kwin_keystate *, uint32_t key, uint32_t state)
    {
        if (key == ORG_KDE_KWIN_KEYSTATE_KEY_ALT) {
            static_cast<Private *>(data)->q->setAltPressed(state != ORG_KDE_KWIN_KEYSTATE_STATE_UNLOCKED);
        }
    }

    void flush()
    {
        if (wl_display_flush(display) >= 0) {
            writeNotifier->setEnabled(false);
        } else if (errno == EAGAIN) {
            writeNotifier->setEnabled(true);
        } else {
            disconnectDisplay();
            q->setAltPressed(false);
        }
    }

    void disconnectDisplay()
    {
        if (readNotifier) {
            readNotifier->setEnabled(false);
            readNotifier.release()->deleteLater();
        }
        if (writeNotifier) {
            writeNotifier->setEnabled(false);
            writeNotifier.release()->deleteLater();
        }
        if (keys) org_kde_kwin_keystate_destroy(keys);
        if (registry) wl_registry_destroy(registry);
        if (display) wl_display_disconnect(display);
        keys = nullptr;
        registry = nullptr;
        display = nullptr;
    }
#endif
};

AltKeyMonitor *AltKeyMonitor::instance()
{
    static QPointer<AltKeyMonitor> monitor;
    if (!monitor) monitor = new AltKeyMonitor(qGuiApp);
    return monitor;
}

AltKeyMonitor::AltKeyMonitor(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>(this))
{
}

AltKeyMonitor::~AltKeyMonitor() = default;

bool AltKeyMonitor::isAltPressed() const
{
    return m_altPressed;
}

void AltKeyMonitor::setAltPressed(bool pressed)
{
    if (m_altPressed == pressed) return;
    m_altPressed = pressed;
    Q_EMIT altPressedChanged(pressed);
}

}
