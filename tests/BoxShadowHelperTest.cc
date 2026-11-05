// SPDX-License-Identifier: GPL-2.0-or-later
#include "BoxShadowHelper.h"

#include <QImage>
#include <QPainter>
#include <QTest>

class BoxShadowHelperTest : public QObject
{
    Q_OBJECT

private slots:
    void centeredShadow_data()
    {
        QTest::addColumn<int>("radius");
        QTest::addColumn<bool>("evenSize");
        for (int radius : {16, 32, 48, 64}) {
            for (bool evenSize : {false, true}) {
                const QByteArray name = QByteArray::number(radius) + (evenSize ? "-even" : "-odd");
                QTest::newRow(name.constData()) << radius << evenSize;
            }
        }
    }

    void centeredShadow()
    {
        QFETCH(int, radius);
        QFETCH(bool, evenSize);
        Material::BoxShadowRenderer renderer;
        QSize boxSize = Material::BoxShadowRenderer::calculateMinimumBoxSize(radius);
        if (evenSize) {
            boxSize += QSize(1, 1);
        }
        renderer.setBoxSize(boxSize);
        renderer.setBorderRadius(5.5);
        renderer.addShadow(QPointF(), radius, QColor(33, 33, 33));
        const QImage image = renderer.render();
        QVERIFY(!image.isNull());
        QVERIFY(qAlpha(image.pixel(image.width() / 2, image.height() / 2)) > 0);
        // A shadow without an offset must share the texture's geometric center.
        // A half-pixel displacement makes opposing edge samples differ.
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                QCOMPARE(image.pixel(x, y), image.pixel(image.width() - 1 - x, y));
                QCOMPARE(image.pixel(x, y), image.pixel(x, image.height() - 1 - y));
            }
        }
    }

    void translatedShadow_data()
    {
        QTest::addColumn<QPointF>("offset");
        QTest::newRow("small-right") << QPointF(1, 0);
        QTest::newRow("small-up") << QPointF(0, -3);
        QTest::newRow("right") << QPointF(96, 0);
        QTest::newRow("left") << QPointF(-96, 0);
        QTest::newRow("down") << QPointF(0, 80);
        QTest::newRow("up") << QPointF(0, -80);
        QTest::newRow("bottom-right") << QPointF(96, 80);
        QTest::newRow("top-left") << QPointF(-96, -80);
        QTest::newRow("top-right") << QPointF(96, -80);
        QTest::newRow("bottom-left") << QPointF(-96, 80);
        QTest::newRow("fractional-right") << QPointF(96.2, 0);
        QTest::newRow("fractional-left") << QPointF(-96.2, 0);
        QTest::newRow("fractional-down") << QPointF(0, 80.2);
        QTest::newRow("fractional-up") << QPointF(0, -80.2);
        QTest::newRow("fractional-top-right") << QPointF(96.2, -80.2);
        QTest::newRow("fractional-bottom-left") << QPointF(-96.2, 80.2);
    }

    void translatedShadow()
    {
        QFETCH(QPointF, offset);
        const int radius = 16;
        Material::BoxShadowRenderer centered;
        centered.setBoxSize(Material::BoxShadowRenderer::calculateMinimumBoxSize(radius));
        centered.setBorderRadius(5.5);
        centered.addShadow(QPointF(), radius, QColor(33, 33, 33));
        const QImage reference = centered.render();
        QVERIFY(!reference.isNull());
        QVERIFY(qAlpha(reference.pixel(reference.width() / 2, reference.height() / 2)) > 0);

        Material::BoxShadowRenderer shifted;
        shifted.setBoxSize(Material::BoxShadowRenderer::calculateMinimumBoxSize(radius));
        shifted.setBorderRadius(5.5);
        shifted.addShadow(offset, radius, QColor(33, 33, 33));
        const QImage actual = shifted.render();

        QVERIFY(!actual.isNull());
        // The whole unshifted texture, including its blur, must fit after
        // translation. Check the bounds independently of raster alignment.
        QRectF translatedBounds(reference.rect());
        translatedBounds.moveCenter(QRectF(actual.rect()).center() + offset);
        QVERIFY(QRectF(actual.rect()).contains(translatedBounds));
        const QPoint shadowCenter = (QRectF(actual.rect()).center() + offset).toPoint();
        QVERIFY(actual.rect().contains(shadowCenter));
        QVERIFY(qAlpha(actual.pixel(shadowCenter)) > 0);

        // Integer offsets map reference pixels exactly. Fractional offsets also
        // account for the actual canvas center and Qt's image sampling.
        QImage expected(actual.size(), reference.format());
        expected.fill(Qt::transparent);
        {
            QPainter painter(&expected);
            painter.drawImage(translatedBounds.topLeft(), reference);
        }
        QCOMPARE(actual, expected);
    }
};

QTEST_GUILESS_MAIN(BoxShadowHelperTest)
#include "BoxShadowHelperTest.moc"
