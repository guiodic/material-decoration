// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>
#include <QVariantMap>

class QObject;
namespace KDecoration3 { class Decoration; }

namespace Material
{
// These helpers use only QObject's metaobject, never KWin's private C++ ABI.
QVariantMap windowRectangle(const QVariant &value);
QString windowStateKey(const QObject *window);
QVariantMap captureWindowState(const QObject *window);
QVariantMap readWindowState(const QString &key, const QString &scope);
bool writeWindowState(const QString &key, const QVariantMap &state, const QString &scope, bool sync = true);
bool snapshotWindowStates(const QString &scope, const QVariantMap &outputOrigins);
void configureWindowState(KDecoration3::Decoration *decoration, bool enabled,
                          bool rememberScreen = true, bool rememberDesktop = false);
}
