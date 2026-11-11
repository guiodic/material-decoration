// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "CustomTilingLayout.h"
#include <Qt>
#include <QVariantMap>
#include <functional>

namespace KDecoration3
{
class Decoration;
}
namespace Material
{
bool canTileWindow(KDecoration3::Decoration *decoration);
bool restoreWindowStateImmediately(KDecoration3::Decoration *decoration, const QVariantMap &state);
bool prepareWindowStateImmediately(KDecoration3::Decoration *decoration, const QVariantMap &state);
void restoreWindowState(KDecoration3::Decoration *decoration, const QVariantMap &state, std::function<void(bool)> callback);
void fetchTilingLayout(KDecoration3::Decoration *decoration, std::function<void(bool, const CustomTilingLayout &)> callback);
bool tileWindow(KDecoration3::Decoration *decoration, Qt::Edges edges);
bool tileCustomWindow(KDecoration3::Decoration *decoration, const QString &tileId, std::function<void(bool)> callback);
}
