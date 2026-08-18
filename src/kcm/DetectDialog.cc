/*
 * Copyright (C) 2026 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

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
