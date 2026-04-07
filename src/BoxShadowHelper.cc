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
#include <cstdint>


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

struct BoxLobes {
    int left;
    int right;
};

void mirrorTopLeftQuadrantAlpha(QImage &image)
{
    const int width = image.width();
    const int height = image.height();

    const int centerX = (width + 1) / 2;
    const int centerY = (height + 1) / 2;

    const int alphaOffset = QSysInfo::ByteOrder == QSysInfo::BigEndian ? 0 : 3;
    const int stride = image.depth() >> 3;

    for (int y = 0; y < centerY; ++y) {
        uchar *in = image.scanLine(y) + alphaOffset;
        uchar *out = in + (width - 1) * stride;

        for (int x = 0; x < centerX; ++x, in += stride, out -= stride) {
            *out = *in;
        }
    }

    for (int y = 0; y < centerY; ++y) {
        const uchar *in = image.scanLine(y) + alphaOffset;
        uchar *out = image.scanLine(height - y - 1) + alphaOffset;

        for (int x = 0; x < width; ++x, in += stride, out += stride) {
            *out = *in;
        }
    }
}
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
    const qreal sigmaSquared = sigma * sigma;

    // Box sizes are computed according to the "Fast Almost-Gaussian Filtering"
    // paper by Peter Kovesi.
    int lower = std::floor(std::sqrt(12 * sigmaSquared / numIterations + 1));
    if (lower % 2 == 0) {
        lower--;
    }

    const int upper = lower + 2;
    const int lowerSquared = lower * lower;
    const int threshold = std::round((12 * sigmaSquared - numIterations * lowerSquared
        - 4 * numIterations * lower - 3 * numIterations) / (-4 * lower - 4));

    QVector<int> boxSizes;
    boxSizes.reserve(numIterations);
    for (int i = 0; i < numIterations; ++i) {
        boxSizes.append(i < threshold ? lower : upper);
    }

    return boxSizes;
}

QVector<BoxLobes> computeLobes(int radius)
{
    const QVector<int> boxSizes = computeBoxSizes(radius, 3);
    QVector<BoxLobes> lobes;
    lobes.reserve(boxSizes.size());
    for (const int boxSize : boxSizes) {
        const int boxRadius = boxSizeToRadius(boxSize);
        lobes.append({boxRadius, boxRadius});
    }
    return lobes;
}

void boxBlurRowAlpha(const uchar *src,
                     uchar *dst,
                     int width,
                     int horizontalStride,
                     int verticalStride,
                     const BoxLobes &lobes,
                     bool transposeInput,
                     bool transposeOutput)
{
    const int inputStep = transposeInput ? verticalStride : horizontalStride;
    const int outputStep = transposeOutput ? verticalStride : horizontalStride;

    const int boxSize = lobes.left + 1 + lobes.right;
    const int reciprocal = (1 << 24) / boxSize;

    uint32_t alphaSum = (boxSize + 1) / 2;

    const uchar *left = src;
    const uchar *right = src;
    uchar *out = dst;

    const uchar firstValue = src[0];
    const uchar lastValue = src[(width - 1) * inputStep];

    alphaSum += firstValue * lobes.left;

    const uchar *initEnd = src + (boxSize - lobes.left) * inputStep;
    while (right < initEnd) {
        alphaSum += *right;
        right += inputStep;
    }

    const uchar *leftEnd = src + boxSize * inputStep;
    while (right < leftEnd) {
        *out = (alphaSum * reciprocal) >> 24;
        alphaSum += *right - firstValue;
        right += inputStep;
        out += outputStep;
    }

    const uchar *centerEnd = src + width * inputStep;
    while (right < centerEnd) {
        *out = (alphaSum * reciprocal) >> 24;
        alphaSum += *right - *left;
        left += inputStep;
        right += inputStep;
        out += outputStep;
    }

    const uchar *rightEnd = dst + width * outputStep;
    while (out < rightEnd) {
        *out = (alphaSum * reciprocal) >> 24;
        alphaSum += lastValue - *left;
        left += inputStep;
        out += outputStep;
    }
}

void boxBlurAlpha(QImage &image, int radius, const QRect &rect = {})
{
    if (radius < 1) {
        return;
    }

    const QRect blurRect = (rect.isNull() ? image.rect() : rect).intersected(image.rect());
    if (blurRect.isEmpty()) {
        return;
    }

    const QVector<BoxLobes> lobes = computeLobes(radius);

    const int alphaOffset = QSysInfo::ByteOrder == QSysInfo::BigEndian ? 0 : 3;
    const int width = blurRect.width();
    const int height = blurRect.height();
    const int rowStride = image.bytesPerLine();
    const int pixelStride = image.depth() >> 3;

    const int bufferStride = qMax(width, height) * pixelStride;
    QVector<uchar> buffer(bufferStride * 2);
    uchar *buf1 = buffer.data();
    uchar *buf2 = buf1 + bufferStride;

    for (int y = 0; y < height; ++y) {
        uchar *row = image.scanLine(blurRect.y() + y) + blurRect.x() * pixelStride + alphaOffset;
        boxBlurRowAlpha(row, buf1, width, pixelStride, rowStride, lobes[0], false, false);
        boxBlurRowAlpha(buf1, buf2, width, pixelStride, rowStride, lobes[1], false, false);
        boxBlurRowAlpha(buf2, row, width, pixelStride, rowStride, lobes[2], false, false);
    }

    for (int x = 0; x < width; ++x) {
        uchar *column = image.scanLine(blurRect.y()) + (blurRect.x() + x) * pixelStride + alphaOffset;
        boxBlurRowAlpha(column, buf1, height, pixelStride, rowStride, lobes[0], true, false);
        boxBlurRowAlpha(buf1, buf2, height, pixelStride, rowStride, lobes[1], false, false);
        boxBlurRowAlpha(buf2, column, height, pixelStride, rowStride, lobes[2], false, true);
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

    // There is no need to blur RGB channels. Blur only the top-left
    // quadrant alpha and then mirror it. The shadow texture is symmetrical
    // before applying an offset at draw time.
    const QRect blurRect(
        0,
        0,
        static_cast<int>(std::ceil(shadow.width() * 0.5)),
        static_cast<int>(std::ceil(shadow.height() * 0.5)));
    boxBlurAlpha(shadow, radius, blurRect);
    mirrorTopLeftQuadrantAlpha(shadow);

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
