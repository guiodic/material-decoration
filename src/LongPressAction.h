// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>
#include <functional>

class QObject;

namespace Material
{
// Checks availability only; the shortcut API does not acknowledge execution.
void checkLongPressAction(QObject *context, const QString &shortcut, const QString &script,
                          std::function<void(bool)> callback);
}
