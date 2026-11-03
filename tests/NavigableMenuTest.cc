// SPDX-License-Identifier: GPL-2.0-or-later
#include "NavigableMenu.h"

#include <QApplication>
#include <QSignalSpy>
#include <QTest>

class NavigableMenuTest : public QObject
{
    Q_OBJECT

private slots:
    void cleanup()
    {
        qApp->setLayoutDirection(Qt::LeftToRight);
    }

    void horizontalSignalsFollowPhysicalKeys_data()
    {
        QTest::addColumn<Qt::LayoutDirection>("direction");
        QTest::addColumn<int>("key");
        QTest::newRow("ltr-left") << Qt::LeftToRight << int(Qt::Key_Left);
        QTest::newRow("ltr-right") << Qt::LeftToRight << int(Qt::Key_Right);
        QTest::newRow("rtl-left") << Qt::RightToLeft << int(Qt::Key_Left);
        QTest::newRow("rtl-right") << Qt::RightToLeft << int(Qt::Key_Right);
    }

    void horizontalSignalsFollowPhysicalKeys()
    {
        QFETCH(Qt::LayoutDirection, direction);
        QFETCH(int, key);
        qApp->setLayoutDirection(direction);
        Material::NavigableMenu menu(nullptr);
        auto *action = menu.addAction(QStringLiteral("Open"));
        QSignalSpy left(&menu, &Material::NavigableMenu::hitLeft);
        QSignalSpy right(&menu, &Material::NavigableMenu::hitRight);
        menu.popup(QPoint(300, 100));
        menu.setActiveAction(action);

        QTest::keyClick(&menu, Qt::Key(key));
        QCOMPARE(left.count(), key == Qt::Key_Left ? 1 : 0);
        QCOMPARE(right.count(), key == Qt::Key_Right ? 1 : 0);
        QVERIFY(menu.isVisible());
    }

    void submenusKeepQtNavigation_data()
    {
        QTest::addColumn<Qt::LayoutDirection>("direction");
        QTest::newRow("ltr") << Qt::LeftToRight;
        QTest::newRow("rtl") << Qt::RightToLeft;
    }

    void submenusKeepQtNavigation()
    {
        QFETCH(Qt::LayoutDirection, direction);
        qApp->setLayoutDirection(direction);
        Material::NavigableMenu menu(nullptr);
        auto *child = menu.addMenu(QStringLiteral("Recent files"));
        child->addAction(QStringLiteral("Document"));
        QSignalSpy left(&menu, &Material::NavigableMenu::hitLeft);
        QSignalSpy right(&menu, &Material::NavigableMenu::hitRight);
        menu.popup(QPoint(300, 100));
        menu.setActiveAction(child->menuAction());
        const auto openKey = direction == Qt::RightToLeft ? Qt::Key_Left : Qt::Key_Right;
        const auto closeKey = direction == Qt::RightToLeft ? Qt::Key_Right : Qt::Key_Left;

        QTest::keyClick(&menu, openKey);
        QTRY_VERIFY(child->isVisible());
        QVERIFY(left.isEmpty());
        QVERIFY(right.isEmpty());
        QTest::keyClick(child, closeKey);
        QTRY_VERIFY(!child->isVisible());
        QVERIFY(menu.isVisible());
        QVERIFY(left.isEmpty());
        QVERIFY(right.isEmpty());
    }
};

QTEST_MAIN(NavigableMenuTest)
#include "NavigableMenuTest.moc"
