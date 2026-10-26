// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QRectF>
#include <QString>
#include <QVector>

namespace Material
{
struct CustomTilingZone {
    QRectF rect;
    QString id;
};
using CustomTilingLayout = QVector<CustomTilingZone>;
}
