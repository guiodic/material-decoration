// SPDX-License-Identifier: GPL-2.0-or-later
#include "TilingPopup.h"
#include "TilingZoneButton.h"
#include <QEnterEvent>
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

    void openingReleaseDoesNotSelect_data()
    {
        QTest::addColumn<bool>("onZone");
        QTest::newRow("zone") << true;
        QTest::newRow("popup") << false;
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
        QFETCH(bool, onZone);
        QTest::mouseRelease(onZone ? buttons.first() : static_cast<QWidget *>(&popup), Qt::LeftButton);
        QVERIFY(selected.isEmpty());
        QVERIFY(popup.isVisible());
        QTest::mouseClick(buttons.first(), Qt::LeftButton);
        QCOMPARE(selected.size(), 1);
    }

    void animatedHighlightReversesWithoutJump()
    {
        TilingZoneButton button;
        button.resize(60, 40);
        QPalette palette = button.palette();
        palette.setColor(QPalette::Midlight, Qt::black);
        palette.setColor(QPalette::Highlight, Qt::white);
        button.setPalette(palette);
        button.setAnimationSettings(true, 400);
        button.show();
        QCoreApplication::processEvents();
        button.resetHighlight();
        const auto color = [&] { return button.grab().toImage().pixelColor(button.rect().center()); };
        QCOMPARE(color(), QColor(Qt::black));
        button.setKeyboardSelected(true);
        QCOMPARE(color(), QColor(Qt::black));
        QTest::qWait(150);
        const QColor halfway = color();
        QVERIFY(halfway.red() > 0 && halfway.red() < 255);
        QCOMPARE(halfway.alpha(), 255);
        button.setKeyboardSelected(false);
        QVERIFY(qAbs(color().red() - halfway.red()) <= 3);
        QTRY_COMPARE(color(), QColor(Qt::black));
        button.setKeyboardSelected(true);
        QTRY_COMPARE(color(), QColor(Qt::white));
        button.setKeyboardSelected(true);
        QCOMPARE(color(), QColor(Qt::white));

        button.setAnimationSettings(false, 400);
        button.setKeyboardSelected(false);
        QCOMPARE(color(), QColor(Qt::black));
        QEnterEvent enter(QPointF(5, 5), QPointF(5, 5), QPointF(5, 5));
        QCoreApplication::sendEvent(&button, &enter);
        QCOMPARE(color(), QColor(Qt::white));
        button.setKeyboardSelected(true);
        button.setKeyboardSelected(false);
        QCOMPARE(color(), QColor(Qt::white)); // Physical hover still holds it.
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(&button, &leave);
        QCOMPARE(color(), QColor(Qt::black));
        button.setAnimationSettings(true, 0);
        button.setKeyboardSelected(true);
        QCOMPARE(color(), QColor(Qt::white));
        button.hide();
        QVERIFY(!button.keyboardSelected());
    }

    void keyboardAndCancel()
    {
        TilingPopup popup;
        QSignalSpy selected(&popup, &TilingPopup::zoneSelected);
        popup.popup(QPoint(10, 10));
        QCoreApplication::processEvents();
        auto *first = popup.findChildren<QToolButton *>().first();
        for (auto *button : popup.findChildren<QToolButton *>()) {
            QVERIFY(!button->property("keyboardSelected").toBool());
        }
        QTest::keyClick(&popup, Qt::Key_Return);
        QTest::keyClick(&popup, Qt::Key_Space);
        QVERIFY(selected.isEmpty());
        QVERIFY(popup.isVisible());
        QTest::keyClick(&popup, Qt::Key_Right);
        QVERIFY(first->hasFocus());
        QVERIFY(first->property("keyboardSelected").toBool());
        QTest::keyClick(first, Qt::Key_Right);
        QVERIFY(QApplication::focusWidget() != first);
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
        QCOMPARE(selected.size(), 1);
        QVERIFY(!popup.isVisible());
        selected.clear();
        popup.popup(QPoint(10, 10));
        QCoreApplication::processEvents();
        for (auto *button : popup.findChildren<QToolButton *>()) {
            QVERIFY(!button->property("keyboardSelected").toBool());
        }
        QTest::keyClick(&popup, Qt::Key_Tab);
        QVERIFY(first->hasFocus());
        QTest::keyClick(first, Qt::Key_Backtab);
        QVERIFY(popup.findChildren<QToolButton *>().last()->hasFocus());
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
        QVERIFY(!popup.isVisible());
        QVERIFY(selected.isEmpty());
    }
};

QTEST_MAIN(TilingPopupTest)
#include "TilingPopupTest.moc"
