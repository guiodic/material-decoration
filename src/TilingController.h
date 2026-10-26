// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "CustomTilingLayout.h"
#include <Qt>
#include <functional>

class QObject;
namespace Material
{
bool canTileWindow(QObject *decoration);
void fetchTilingLayout(QObject *decoration, std::function<void(bool, const CustomTilingLayout &)> callback);
bool tileWindow(QObject *decoration, Qt::Edges edges);
bool tileCustomWindow(QObject *decoration, const QString &tileId, std::function<void(bool)> callback);
}
