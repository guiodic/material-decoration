// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <Qt>
#include <functional>

class QObject;
namespace Material
{
bool canTileWindow(QObject *decoration);
void checkTilingScript(QObject *decoration, std::function<void(bool)> callback);
bool tileWindow(QObject *decoration, Qt::Edges edges);
}
