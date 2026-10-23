// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <KConfig>
#include <KConfigGroup>
#include <QStringList>

namespace Material
{
// Preserve the appearance of configurations written before the opacity control.
inline void migrateTilingPopupOpacity(KConfig &config)
{
    bool changed = false;
    for (const auto &name : config.groupList()) {
        if (name != QLatin1String("Windeco") && !name.startsWith(QLatin1String("Windeco Exception "))) continue;
        KConfigGroup group(&config, name);
        if (!group.hasKey("TilingPopupTransparency")) continue;
        if (!group.hasKey("TilingPopupOpacity")) {
            group.writeEntry("TilingPopupOpacity", 100 - qBound(0, group.readEntry("TilingPopupTransparency", 15), 100));
        }
        group.deleteEntry("TilingPopupTransparency");
        changed = true;
    }
    if (changed) config.sync();
}
}
