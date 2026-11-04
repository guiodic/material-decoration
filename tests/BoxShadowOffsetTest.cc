// SPDX-License-Identifier: GPL-2.0-or-later
#include "BoxShadowHelper.h"

#include <QImage>
#include <QTest>

class BoxShadowOffsetTest : public QObject
{
    Q_OBJECT

private slots:
    void translatedShadow_data()
    {
        QTest::addColumn<QPoint>("offset");
        QTest::newRow("small-right") << QPoint(1, 0);
        QTest::newRow("small-up") << QPoint(0, -3);
        QTest::newRow("right") << QPoint(96, 0);
        QTest::newRow("left") << QPoint(-96, 0);
        QTest::newRow("down") << QPoint(0, 80);
        QTest::newRow("up") << QPoint(0, -80);
        QTest::newRow("bottom-right") << QPoint(96, 80);
        QTest::newRow("top-left") << QPoint(-96, -80);
        QTest::newRow("top-right") << QPoint(96, -80);
        QTest::newRow("bottom-left") << QPoint(-96, 80);
    }

    void translatedShadow()
    {
        QFETCH(QPoint, offset);
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
        const QPoint shadowCenter = actual.rect().center() + offset;
        QVERIFY(actual.rect().contains(shadowCenter));
        QVERIFY(qAlpha(actual.pixel(shadowCenter)) > 0);
    }
};

QTEST_GUILESS_MAIN(BoxShadowOffsetTest)
#include "BoxShadowOffsetTest.moc"
