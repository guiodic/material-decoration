// SPDX-License-Identifier: GPL-2.0-or-later
#include "ButtonBackground.h"
#include "PixelSnapper.h"

#include <QPainter>

namespace Material
{
void paintRoundedButtonBackground(QPainter *painter, const QRectF &geometry,
                                  const QPainterPath &borderPath, const QPainterPath &titleBarPath,
                                  const QColor &borderColor, const QColor &baseColor,
                                  const QColor &overlayColor, qreal opacity)
{
    const qreal overlayAlpha = overlayColor.alphaF() * opacity;
    if (overlayAlpha <= 0) {
        return;
    }

    // Compose colors before applying the titlebar's corner coverage once.
    const qreal baseAlpha = baseColor.alphaF() * (1.0 - overlayAlpha);
    const qreal alpha = overlayAlpha + baseAlpha;
    const QColor composed = QColor::fromRgbF(
        (overlayColor.redF() * overlayAlpha + baseColor.redF() * baseAlpha) / alpha,
        (overlayColor.greenF() * overlayAlpha + baseColor.greenF() * baseAlpha) / alpha,
        (overlayColor.blueF() * overlayAlpha + baseColor.blueF() * baseAlpha) / alpha,
        alpha);

    // Use the same physical pixel boundary for clearing and repainting, so
    // fractional button coordinates cannot leave a partially painted seam.
    const QRectF patch = PixelSnapper(painter).snap(geometry);
    painter->save();
    painter->setClipRect(patch, Qt::IntersectClip);
    painter->setOpacity(1.0);
    painter->setCompositionMode(QPainter::CompositionMode_Source);
    painter->fillRect(patch, Qt::transparent);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(borderColor);
    painter->drawPath(borderPath);
    painter->setBrush(composed);
    painter->drawPath(titleBarPath);
    painter->restore();
}
}
