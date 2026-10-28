// SPDX-License-Identifier: GPL-2.0-or-later
#include "MenuMnemonics.h"
#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>
#include <QWidgetAction>

class MenuMnemonicsTest : public QObject
{
    Q_OBJECT
private:
    static bool hasMnemonic(QAction *action)
    {
        return !QKeySequence::mnemonic(action->text()).isEmpty();
    }
private slots:
    void assignsPopupMnemonicsWithoutChangingRootOrActions()
    {
        QMenu root;
        QMenu *menu = root.addMenu(QStringLiteral("Tools"));
        Material::manageMenuMnemonics(menu);
        QAction *open = menu->addAction(QStringLiteral("Open"));
        QAction *options = menu->addAction(QStringLiteral("Options"));
        open->setProperty("_dbusmenu_id", 77);
        QSignalSpy triggered(open, &QAction::triggered);
        QTRY_VERIFY(hasMnemonic(open) && hasMnemonic(options));
        QVERIFY(QKeySequence::mnemonic(open->text()) != QKeySequence::mnemonic(options->text()));
        QCOMPARE(root.actions().first()->text(), QStringLiteral("Tools"));
        QCOMPARE(open->parent(), menu);
        QCOMPARE(open->associatedObjects(), QList<QObject *>{menu});
        QCOMPARE(open->property("_dbusmenu_id").toInt(), 77);
        QVERIFY(triggered.isEmpty());
        open->trigger();
        QCOMPARE(triggered.size(), 1);
    }
    void preparesBeforeFirstPaint()
    {
        QMenu menu;
        Material::manageMenuMnemonics(&menu);
        QAction *action = menu.addAction(QStringLiteral("Open"));
        menu.popup(QPoint(10, 10));
        QVERIFY(hasMnemonic(action));
        menu.hide();
    }
    void updatesVisibleMenuAfterAsyncChanges()
    {
        QMenu menu;
        Material::manageMenuMnemonics(&menu);
        QAction *action = menu.addAction(QStringLiteral("Open"));
        menu.popup(QPoint(10, 10));
        QVERIFY(hasMnemonic(action));
        action->setText(QStringLiteral("Changed Label"));
        QTRY_VERIFY(hasMnemonic(action));
        QString plain = action->text();
        plain.remove(QLatin1Char('&'));
        QCOMPARE(plain, QStringLiteral("Changed Label"));
        QAction *late = menu.addAction(QStringLiteral("Late Entry"));
        QTRY_VERIFY(hasMnemonic(late));
        menu.removeAction(late);
        action->setText(QStringLiteral("Restored Label"));
        QTRY_VERIFY(hasMnemonic(action));
        menu.hide();
    }
    void refreshingDoesNotRescheduleItself()
    {
        QMenu menu;
        Material::manageMenuMnemonics(&menu);
        QAction *action = menu.addAction(QStringLiteral("Open"));
        QSignalSpy changed(action, &QAction::changed);
        QTRY_VERIFY(hasMnemonic(action));
        const auto count = changed.size();
        QTest::qWait(50);
        QCOMPARE(changed.size(), count);
    }
    void preservesEmbeddedWidgetOwnership()
    {
        QMenu menu;
        auto *header = new QWidgetAction(&menu);
        auto *widget = new QToolButton;
        widget->setText(QStringLiteral("Header"));
        header->setDefaultWidget(widget);
        menu.addAction(header);
        QAction *action = menu.addAction(QStringLiteral("Open"));
        Material::manageMenuMnemonics(&menu);
        menu.popup(QPoint(10, 10));
        QVERIFY(hasMnemonic(action));
        QCOMPARE(header->defaultWidget(), widget);
        QCOMPARE(widget->parentWidget(), &menu);
        QVERIFY(menu.actions().contains(header));
        menu.hide();
    }
    void destructionCancelsPendingRefresh()
    {
        auto *menu = new QMenu;
        Material::manageMenuMnemonics(menu);
        menu->addAction(QStringLiteral("Open"));
        const QPointer<QMenu> guard(menu);
        delete menu;
        QTest::qWait(10);
        QVERIFY(guard.isNull());
    }
};
QTEST_MAIN(MenuMnemonicsTest)
#include "MenuMnemonicsTest.moc"
