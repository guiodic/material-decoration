// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "CustomTilingLayout.h"
#include <Qt>
#include <QVariantMap>
class QObject;

namespace KDecoration3
{
class Decoration;
}
namespace Material
{
QObject *windowStateWorkspace();
bool canTileWindow(KDecoration3::Decoration *decoration);
enum class WindowRestoreResult { Unavailable, Failed, Succeeded };
WindowRestoreResult restoreWindowStateImmediately(KDecoration3::Decoration *decoration, const QVariantMap &state);
WindowRestoreResult prepareWindowStateImmediately(KDecoration3::Decoration *decoration, const QVariantMap &state);
bool matchesSavedWindowState(KDecoration3::Decoration *decoration, const QVariantMap &state);
bool tilingScriptAvailable();
bool fetchTilingLayout(KDecoration3::Decoration *decoration, CustomTilingLayout &layout);
bool tileWindow(KDecoration3::Decoration *decoration, Qt::Edges edges);
bool tileCustomWindow(KDecoration3::Decoration *decoration, const QString &tileId);
}
