// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <Qt>

namespace KDecoration3 { class Decoration; }
namespace Material
{
bool canTileWindow(KDecoration3::Decoration *decoration);
bool tileWindow(KDecoration3::Decoration *decoration, Qt::Edges edges);
}
