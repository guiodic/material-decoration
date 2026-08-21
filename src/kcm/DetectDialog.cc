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

#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariantMap>

namespace Material
{

DetectDialog::DetectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Detect Window Properties"));

    auto layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel(i18n("Click 'Detect' and then click on the target window to capture its properties."), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_detectButton = new QPushButton(i18n("Detect"), this);
    layout->addWidget(m_detectButton);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);

    connect(m_detectButton, &QPushButton::clicked, this, &DetectDialog::detectWindow);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void DetectDialog::detectWindow()
{
    m_detectButton->setEnabled(false);
    m_statusLabel->setText(i18n("Querying KWin for window information..."));

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("queryWindowInfo"));

    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        m_detectButton->setEnabled(true);
        QDBusPendingReply<QVariantMap> reply = *watcher;
        if (!reply.isError()) {
            QVariantMap info = reply.value();
            m_windowClass = info.value(QStringLiteral("resourceClass")).toString();
            if (m_windowClass.isEmpty()) {
                m_windowClass = info.value(QStringLiteral("resourceName")).toString();
            }
            m_caption = info.value(QStringLiteral("caption")).toString();

            m_statusLabel->setText(i18n("Captured Window Class: %1\nCaptured Window Title: %2",
                                        m_windowClass, m_caption));
        } else {
            m_statusLabel->setText(i18n("Failed to detect window info: %1", reply.error().message()));
        }
        watcher->deleteLater();
    });
}

} // namespace Material
