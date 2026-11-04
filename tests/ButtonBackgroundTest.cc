// SPDX-License-Identifier: GPL-2.0-or-later
#include "ButtonBackground.h"
#include "DrawHelper.h"

#include <QImage>
#include <QPainter>
#include <QTest>

class ButtonBackgroundTest : public QObject
{
    Q_OBJECT

private:
    static void scene(QImage &image, qreal border, const QColor &base,
                      QPainterPath &borderPath, QPainterPath &titlePath)
    {
        const auto frame = Material::DrawHelper::getRoundedPath(QRectF(0, 0, 120, 80), 6, true, true, true, true);
        titlePath = Material::DrawHelper::getRoundedPath(QRectF(border, border, 120 - 2 * border, 34), 6);
        borderPath = border > 0 ? frame.subtracted(titlePath) : QPainterPath();
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        p.drawPath(borderPath);
        p.setBrush(base);
        p.drawPath(titlePath);
    }

private slots:
    void idle_data()
    {
        QTest::addColumn<qreal>("scale");
        for (qreal scale : {1.0, 1.25, 1.5, 2.0}) {
            QTest::newRow(qPrintable(QString::number(scale))) << scale;
        }
    }

    void idle()
    {
        QFETCH(qreal, scale);
        QImage image(QSize(qRound(120 * scale), qRound(80 * scale)), QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(scale);
        image.fill(Qt::transparent);
        QPainterPath border, title;
        scene(image, 0, QColor(39, 43, 46), border, title);
        const QImage before = image;
        {
            QPainter p(&image);
            Material::paintRoundedButtonBackground(&p, QRectF(83.3, 0, 36.7, 33), border, title,
                Qt::black, QColor(39, 43, 46), Qt::transparent, 1.0);
            Material::paintRoundedButtonBackground(&p, QRectF(0, 0, 36.7, 33), border, title,
                Qt::black, QColor(39, 43, 46), Qt::red, 0.0);
        }
        QCOMPARE(image, before);
    }

    void preservesBorder()
    {
        QImage image(120, 80, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainterPath border, title;
        scene(image, 6, QColor(39, 43, 46), border, title);
        const QRgb before = image.pixel(113, 6);
        const QRgb beforeLeft = image.pixel(6, 6);
        QCOMPARE(qAlpha(before), 255);
        QCOMPARE(qAlpha(beforeLeft), 255);
        {
            QPainter p(&image);
            Material::paintRoundedButtonBackground(&p, QRectF(78, 6, 36, 33), border, title,
                Qt::black, QColor(39, 43, 46), QColor(218, 68, 83), 1.0);
            Material::paintRoundedButtonBackground(&p, QRectF(6, 6, 36, 33), border, title,
                Qt::black, QColor(39, 43, 46), QColor(218, 68, 83), 1.0);
        }
        QCOMPARE(image.pixel(113, 6), before);
        QCOMPARE(image.pixel(6, 6), beforeLeft);
    }

    void fractionalSeam_data() { idle_data(); }

    void fractionalSeam()
    {
        QFETCH(qreal, scale);
        QImage image(QSize(qRound(120 * scale), qRound(80 * scale)), QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(scale);
        image.fill(Qt::transparent);
        QPainterPath border, title;
        scene(image, 0, QColor(39, 43, 46), border, title);
        {
            QPainter p(&image);
            Material::paintRoundedButtonBackground(&p, QRectF(83.3, 0, 36.7, 33), border, title,
                Qt::black, QColor(39, 43, 46), QColor(218, 68, 83), 1.0);
        }
        for (int x = qRound(80 * scale); x < qRound(88 * scale); ++x) {
            QCOMPARE(qAlpha(image.pixel(x, qRound(20 * scale))), 255);
        }
        // The outer antialiased red edge must not contain dark titlebar color.
        for (int y = 0; y < qRound(6 * scale); ++y) {
            for (int x = qRound(114 * scale); x < image.width(); ++x) {
                const auto c = image.pixelColor(x, y);
                if (c.alpha() >= 32) {
                    QVERIFY(c.red() > 205);
                }
            }
        }
    }

    void matchesNativePixelCoverage_data()
    {
        QTest::addColumn<qreal>("scale");
        QTest::addColumn<qreal>("edge");
        QTest::addColumn<bool>("right");
        for (qreal scale : {1.0, 1.25, 1.5, 2.0}) {
            for (qreal edge : {83.1, 83.3, 83.5, 83.7, 83.9}) {
                for (bool right : {false, true}) {
                    const QByteArray name = QStringLiteral("%1-%2-%3")
                        .arg(scale).arg(edge).arg(right ? "right" : "left").toLatin1();
                    QTest::newRow(name.constData()) << scale << edge << right;
                }
            }
        }
    }

    void matchesNativePixelCoverage()
    {
        QFETCH(qreal, scale);
        QFETCH(qreal, edge);
        QFETCH(bool, right);
        const QColor base(39, 43, 46);
        const QColor overlay(218, 68, 83);
        QImage image(QSize(qRound(120 * scale), qRound(80 * scale)), QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(scale);
        image.fill(Qt::transparent);
        QPainterPath border, title;
        scene(image, 0, base, border, title);
        QImage expected = image;
        const QRectF geometry = right ? QRectF(edge, 0, 120 - edge, 33)
                                      : QRectF(0, 0, 120 - edge, 33);
        {
            QPainter p(&image);
            Material::paintRoundedButtonBackground(&p, geometry, border, title,
                Qt::black, base, overlay, 1.0);
        }
        {
            QPainter p(&expected);
            p.setRenderHint(QPainter::Antialiasing, false);
            p.fillRect(geometry, overlay);
        }
        // Compare the entire scanline below the rounded corners: snapping must
        // color exactly the pixels selected by Qt's native rectangle rasterizer.
        const int y = qRound(20 * scale);
        for (int x = 0; x < image.width(); ++x) {
            QCOMPARE(image.pixel(x, y), expected.pixel(x, y));
        }
    }

    void translucentColors()
    {
        for (int baseAlpha : {0, 128, 255}) {
            for (int overlayAlpha : {128, 255}) {
                for (qreal opacity : {0.6, 1.0}) {
                    const QColor base(39, 43, 46, baseAlpha);
                    const QColor overlay(218, 68, 83, overlayAlpha);
                    QImage expected(1, 1, QImage::Format_ARGB32_Premultiplied);
                    expected.fill(Qt::transparent);
                    {
                        QPainter p(&expected);
                        p.fillRect(expected.rect(), base);
                        p.setOpacity(opacity);
                        p.fillRect(expected.rect(), overlay);
                    }
                    QImage image(120, 80, expected.format());
                    image.fill(Qt::transparent);
                    QPainterPath border, title;
                    scene(image, 0, base, border, title);
                    {
                        QPainter p(&image);
                        Material::paintRoundedButtonBackground(&p, QRectF(83.3, 0, 36.7, 33), border, title,
                            Qt::black, base, overlay, opacity);
                    }
                    const QRgb actual = image.pixel(100, 20);
                    const QRgb reference = expected.pixel(0, 0);
                    // Allow the rounding difference between one composed fill
                    // and Qt's two separate premultiplied-alpha fills.
                    QVERIFY(qAbs(qAlpha(actual) - qAlpha(reference)) <= 2);
                    QVERIFY(qAbs(qRed(actual) - qRed(reference)) <= 2);
                    QVERIFY(qAbs(qGreen(actual) - qGreen(reference)) <= 2);
                    QVERIFY(qAbs(qBlue(actual) - qBlue(reference)) <= 2);
                }
            }
        }
    }
};

QTEST_GUILESS_MAIN(ButtonBackgroundTest)
#include "ButtonBackgroundTest.moc"
