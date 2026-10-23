// SPDX-License-Identifier: GPL-2.0-or-later
#include "TilingPopup.h"
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>

using namespace Material;

class TilingPopupTest : public QObject
{
    Q_OBJECT
private slots:
    void layoutsCoverTheirPreview()
    {
        const auto layouts = tilingLayouts();
        QCOMPARE(layouts.size(), 4);
        for (const auto &layout : layouts) {
            qreal area = 0;
            for (qsizetype i = 0; i < layout.size(); ++i) {
                const auto &zone = layout[i];
                QVERIFY(QRectF(0, 0, 1, 1).contains(zone.rect));
                QVERIFY(!zone.label.isEmpty());
                QVERIFY(zone.edges != Qt::Edges());
                QVERIFY(!(zone.edges.testFlag(Qt::LeftEdge) && zone.edges.testFlag(Qt::RightEdge)));
                QVERIFY(!(zone.edges.testFlag(Qt::TopEdge) && zone.edges.testFlag(Qt::BottomEdge)));
                area += zone.rect.width() * zone.rect.height();
                for (qsizetype j = i + 1; j < layout.size(); ++j) {
                    QVERIFY(!zone.rect.intersects(layout[j].rect));
                }
            }
            QCOMPARE(area, 1.0);
        }
    }

    void selectsEveryZone()
    {
        TilingPopup popup;
        QSignalSpy selected(&popup, &TilingPopup::zoneSelected);
        const auto buttons = popup.findChildren<QToolButton *>();
        qsizetype index = 0;
        for (const auto &layout : tilingLayouts()) {
            for (const auto &zone : layout) {
                popup.popup(QPoint(10, 10));
                QCoreApplication::processEvents();
                QVERIFY(popup.isVisible());
                QTest::mouseClick(buttons[index++], Qt::LeftButton);
                QCOMPARE(selected.size(), 1);
                QCOMPARE(qvariant_cast<Qt::Edges>(selected.takeFirst().at(0)), zone.edges);
                QVERIFY(!popup.isVisible());
            }
        }
        QCOMPARE(index, buttons.size());
    }

    void openingReleaseDoesNotSelect()
    {
        QWidget source;
        source.show();
        QTest::mousePress(&source, Qt::LeftButton);
        TilingPopup popup;
        QSignalSpy selected(&popup, &TilingPopup::zoneSelected);
        popup.popup(QPoint(10, 10));
        const auto buttons = popup.findChildren<QToolButton *>();
        QTest::mouseRelease(buttons.first(), Qt::LeftButton);
        QVERIFY(selected.isEmpty());
        QVERIFY(popup.isVisible());
        QTest::mouseClick(buttons.first(), Qt::LeftButton);
        QCOMPARE(selected.size(), 1);
    }

    void keyboardAndCancel()
    {
        TilingPopup popup;
        QSignalSpy selected(&popup, &TilingPopup::zoneSelected);
        popup.popup(QPoint(10, 10));
        QCoreApplication::processEvents();
        auto *first = popup.findChildren<QToolButton *>().first();
        QVERIFY(first->hasFocus());
        QTest::keyClick(first, Qt::Key_Right);
        QVERIFY(QApplication::focusWidget() != first);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
        QCOMPARE(selected.size(), 1);
        QVERIFY(!popup.isVisible());
        selected.clear();
        popup.popup(QPoint(10, 10));
        QTest::keyClick(first, Qt::Key_Escape);
        QVERIFY(!popup.isVisible());
        QVERIFY(selected.isEmpty());
    }
};

QTEST_MAIN(TilingPopupTest)
#include "TilingPopupTest.moc"
