// SPDX-License-Identifier: GPL-2.0-or-later
#include "ButtonCornerEdges.h"

#include <QTest>

class ButtonCornerEdgesTest : public QObject
{
    Q_OBJECT

private slots:
    void adjacentEdges_data()
    {
        QTest::addColumn<QRectF>("geometry");
        QTest::addColumn<qreal>("left");
        QTest::addColumn<qreal>("right");
        QTest::addColumn<Qt::Edges>("expected");
        QTest::newRow("single-left") << QRectF(0, 0, 36, 33) << 0.0 << 640.0 << Qt::Edges(Qt::LeftEdge);
        QTest::newRow("single-right") << QRectF(604, 0, 36, 33) << 0.0 << 640.0 << Qt::Edges(Qt::RightEdge);
        QTest::newRow("rtl-interior-menu") << QRectF(497.55, 0, 39, 33) << 0.0 << 640.0 << Qt::Edges();
        QTest::newRow("after-left-spacer") << QRectF(36, 0, 36, 33) << 0.0 << 640.0 << Qt::Edges();
        QTest::newRow("before-right-spacer") << QRectF(568, 0, 36, 33) << 0.0 << 640.0 << Qt::Edges();
        QTest::newRow("left-border") << QRectF(6, 0, 36, 33) << 6.0 << 634.0 << Qt::Edges(Qt::LeftEdge);
        QTest::newRow("right-border") << QRectF(598, 0, 36, 33) << 6.0 << 634.0 << Qt::Edges(Qt::RightEdge);
        QTest::newRow("both-edges") << QRectF(6, 0, 628, 33) << 6.0 << 634.0 << (Qt::LeftEdge | Qt::RightEdge);
    }

    void adjacentEdges()
    {
        QFETCH(QRectF, geometry);
        QFETCH(qreal, left);
        QFETCH(qreal, right);
        QFETCH(Qt::Edges, expected);
        QCOMPARE(Material::buttonCornerEdges(geometry, left, right, 1.0), expected);
    }

    void toleranceUsesPhysicalPixels()
    {
        for (qreal scale : {1.0, 1.25, 1.5, 2.0}) {
            const qreal close = 0.49 / scale;
            const qreal far = 0.51 / scale;
            QCOMPARE(Material::buttonCornerEdges(QRectF(close, 0, 36, 33), 0, 640, scale), Qt::Edges(Qt::LeftEdge));
            QCOMPARE(Material::buttonCornerEdges(QRectF(far, 0, 36, 33), 0, 640, scale), Qt::Edges());
            QCOMPARE(Material::buttonCornerEdges(QRectF(604 - close, 0, 36, 33), 0, 640, scale), Qt::Edges(Qt::RightEdge));
            QCOMPARE(Material::buttonCornerEdges(QRectF(604 - far, 0, 36, 33), 0, 640, scale), Qt::Edges());
        }
    }
};

QTEST_GUILESS_MAIN(ButtonCornerEdgesTest)
#include "ButtonCornerEdgesTest.moc"
