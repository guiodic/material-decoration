#include "DetectDialog.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace Material
{

DetectDialog::DetectDialog(QObject *parent)
    : QObject(parent)
{
}

void DetectDialog::detect()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("/KWin"),
                                                          QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("queryWindowInfo"));

    QDBusPendingReply<QVariantMap> asyncReply = QDBusConnection::sessionBus().asyncCall(message);
    auto *watcher = new QDBusPendingCallWatcher(asyncReply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        QDBusPendingReply<QVariantMap> reply = *self;
        self->deleteLater();
        if (!reply.isValid()) {
            Q_EMIT detectionDone(false);
            return;
        }
        m_properties = reply.value();
        Q_EMIT detectionDone(true);
    });
}

QVariantMap DetectDialog::properties() const
{
    return m_properties;
}

} // namespace Material
