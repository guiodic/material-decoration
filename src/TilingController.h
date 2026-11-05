// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "CustomTilingLayout.h"
#include <Qt>
#include <functional>

namespace KDecoration3
{
class Decoration;
}
namespace Material
{
bool canTileWindow(KDecoration3::Decoration *decoration);
void fetchTilingLayout(KDecoration3::Decoration *decoration, std::function<void(bool, const CustomTilingLayout &)> callback);
bool tileWindow(KDecoration3::Decoration *decoration, Qt::Edges edges);
bool tileCustomWindow(KDecoration3::Decoration *decoration, const QString &tileId, std::function<void(bool)> callback);
}
