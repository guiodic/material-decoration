/*
 * Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// own
#include "BoxShadowHelper.h"

// Qt
#include <QVector>

// std
#include <cmath>


namespace Material
{
namespace BoxShadowHelper
{

namespace
{
// According to the CSS Level 3 spec, standard deviation must be equal to
// half of the blur radius. https://www.w3.org/TR/css-backgrounds-3/#shadow-blur
// Current window size is too small for sigma equal to half of the blur radius.
// As a workaround, sigma blur scale is lowered. With the lowered sigma
// blur scale, area under the kernel equals to 0.98, which is pretty enough.
// Maybe, it should be changed in the future.
const qreal SIGMA_BLUR_SCALE = 0.4375;
} // anonymous namespace

inline qreal radiusToSigma(qreal radius)
{
    return radius * SIGMA_BLUR_SCALE;
}

inline int boxSizeToRadius(int boxSize)
{
    return (boxSize - 1) / 2;
}

QVector<int> computeBoxSizes(int radius, int numIterations)
{
    const qreal sigma = radiusToSigma(radius);

    // Box sizes are computed according to the "Fast Almost-Gaussian Filtering"
    // paper by Peter Kovesi.
    int lower = std::floor(std::sqrt(12 * std::pow(sigma, 2) / numIterations + 1));
    if (lower % 2 == 0) {
        lower--;
    }

    const int upper = lower + 2;
    const int threshold = std::round((12 * std::pow(sigma, 2) - numIterations * std::pow(lower, 2)
        - 4 * numIterations * lower - 3 * numIterations) / (-4 * lower - 4));

    QVector<int> boxSizes;
    boxSizes.reserve(numIterations);
    for (int i = 0; i < numIterations; ++i) {
        boxSizes.append(i < threshold ? lower : upper);
    }

    return boxSizes;
}

void boxBlurPass(const QImage &src, QImage &dst, int boxSize)
{
    const int alphaStride = src.depth() >> 3;
    const int alphaOffset = QSysInfo::ByteOrder == QSysInfo::BigEndian ? 0 : 3;

    const int radius = boxSizeToRadius(boxSize);
    const qreal invSize = 1.0 / boxSize;

    const int dstStride = dst.width() * alphaStride;

    for (int y = 0; y < src.height(); ++y) {
        const uchar *srcAlpha = src.scanLine(y);
        uchar *dstAlpha = dst.scanLine(0);

        srcAlpha += alphaOffset;
        dstAlpha += alphaOffset + y * alphaStride;

        const uchar *left = srcAlpha;
        const uchar *right = left + alphaStride * radius;

        int window = 0;
        for (int x = 0; x < radius; ++x) {
            window += *srcAlpha;
            srcAlpha += alphaStride;
        }

        for (int x = 0; x <= radius; ++x) {
            window += *right;
            right += alphaStride;
            *dstAlpha = static_cast<uchar>(window * invSize);
            dstAlpha += dstStride;
        }

        for (int x = radius + 1; x < src.width() - radius; ++x) {
            window += *right - *left;
            left += alphaStride;
            right += alphaStride;
            *dstAlpha = static_cast<uchar>(window * invSize);
            dstAlpha += dstStride;
        }

        for (int x = src.width() - radius; x < src.width(); ++x) {
            window -= *left;
            left += alphaStride;
            *dstAlpha = static_cast<uchar>(window * invSize);
            dstAlpha += dstStride;
        }
    }
}

void boxBlurAlpha(QImage &image, int radius, int numIterations)
{
    // Temporary buffer is transposed so we always read memory
    // in linear order.
    QImage tmp(image.height(), image.width(), image.format());

    const QVector<int> boxSizes = computeBoxSizes(radius, numIterations);
    for (const int &boxSize : boxSizes) {
        boxBlurPass(image, tmp, boxSize); // horizontal pass
        boxBlurPass(tmp, image, boxSize); // vertical pass
    }
}

void boxShadow(QPainter *p, const QRect &box, const QPoint &offset, int radius, const QColor &color)
{
    const QSize size = box.size() + 2 * QSize(radius, radius);
    const qreal dpr = p->device()->devicePixelRatioF();

    QPainter painter;

    QImage shadow(size * dpr, QImage::Format_ARGB32_Premultiplied);
    shadow.setDevicePixelRatio(dpr);
    shadow.fill(Qt::transparent);

    painter.begin(&shadow);
    painter.fillRect(QRect(QPoint(radius, radius), box.size()), Qt::black);
    painter.end();

    // There is no need to blur RGB channels. Blur the alpha
    // channel and then give the shadow a tint of the desired color.
    const int numIterations = 3;
    boxBlurAlpha(shadow, radius, numIterations);

    painter.begin(&shadow);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(shadow.rect(), color);
    painter.end();

    QRect shadowRect = shadow.rect();
    shadowRect.setSize(shadowRect.size() / dpr);
    shadowRect.moveCenter(box.center() + offset);
    p->drawImage(shadowRect, shadow);
}

} // namespace BoxShadowHelper
} // namespace Material
