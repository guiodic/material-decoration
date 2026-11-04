// SPDX-License-Identifier: GPL-2.0-or-later
#include "BoxShadowHelper.h"

#include <QImage>
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
        // A shadow without an offset must share the texture's geometric center.
        // A half-pixel displacement makes opposing edge samples differ.
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                QCOMPARE(image.pixel(x, y), image.pixel(image.width() - 1 - x, y));
                QCOMPARE(image.pixel(x, y), image.pixel(x, image.height() - 1 - y));
            }
        }
    }
};

QTEST_GUILESS_MAIN(BoxShadowHelperTest)
#include "BoxShadowHelperTest.moc"
