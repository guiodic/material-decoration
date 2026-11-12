// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QSet>
#include <QVariantMap>

namespace Material
{
QString windowConfigurationScope(const QObject *workspace);

// Keeps a stable topology identity while KWin updates outputs and evacuates
// windows. Snapshotting happens only on configuration changes, not on moves.
class WindowStateScope : public QObject
{
    Q_OBJECT
public:
    static WindowStateScope *self();
    QString current();

signals:
    void snapshotRequested(const QString &scope, const QVariantMap &outputOrigins);

private slots:
    void changing();
    void changed();

private:
    explicit WindowStateScope(QObject *parent);
    void attach();
    void watchOutputs();
    QPointer<QObject> m_wrapper;
    QPointer<QObject> m_workspace;
    QSet<QObject *> m_watched;
    QString m_scope;
    QVariantMap m_origins;
    bool m_changing = false;
};
}
