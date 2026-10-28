// SPDX-License-Identifier: GPL-2.0-or-later
#include "dbusmenuimporter.h"
#include "dbusmenutypes_p.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QMenu>

class MenuService : public QDBusVirtualObject
{
public:
    QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "menu-test-server");
    QList<QPair<int, int>> layouts;
    bool missingNested = false;
    bool removeAfterTree = false;
    QStringList treeProperties;
    int treeDelay = 0;
    QString introspect(const QString &) const override { return {}; }
    DBusMenuLayoutItem tree(int id, int depth) const
    {
        DBusMenuLayoutItem item;
        item.id = id;
        item.properties = {{"label", id == 1 ? "_File" : id == 2 ? "_Save" : id == 3 ? "_Nested" : "_Run"},
                           {"enabled", true}, {"visible", true}};
        if (id == 0 || id == 1 || id == 3 || id == 7) item.properties.insert("children-display", "submenu");
        if (depth == 0) return item;
        const int next = depth < 0 ? -1 : depth - 1;
        if (id == 0) item.children = {tree(1, next), tree(7, next)};
        if (id == 1) {
            item.children = {tree(2, next)};
            if (!missingNested) item.children.append(tree(3, next));
        }
        if (id == 3) item.children = {tree(4, next)};
        if (id == 7) item.children = {tree(8, next)};
        return item;
    }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override
    {
        if (message.member() == "GetLayout") {
            const int id = message.arguments().at(0).toInt();
            const int depth = message.arguments().at(1).toInt();
            layouts.append({id, depth});
            const QVariantList result{QVariant::fromValue(uint(1)), QVariant::fromValue(tree(id, depth))};
            if (depth < 0) {
                treeProperties = message.arguments().at(2).toStringList();
                if (removeAfterTree) missingNested = true;
            }
            QTimer::singleShot(depth < 0 ? treeDelay : id == 3 ? 150 : 0, this, [bus, message, result] { bus.send(message.createReply(result)); });
        } else {
            bus.send(message.createReply(message.member() == "AboutToShow" ? QVariantList{false} : QVariantList{}));
        }
        return true;
    }
    void activate(int id)
    {
        auto message = QDBusMessage::createSignal("/Menu", "com.canonical.dbusmenu", "ItemActivationRequested");
        message << id << uint(0);
        bus.send(message);
    }
};

class DBusMenuImporterTest : public QObject
{
    Q_OBJECT
    MenuService service;
private slots:
    void initTestCase()
    {
        DBusMenuTypes_register();
        QVERIFY(service.bus.registerVirtualObject("/Menu", &service));
    }
    void init() { service.layouts.clear(); service.missingNested = false; service.removeAfterTree = false; service.treeProperties.clear(); service.treeDelay = 0; }
    void initialLoadDoesNotFetchSubmenus()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        QCOMPARE(service.layouts, (QList<QPair<int, int>>{{0, 1}}));
        QVERIFY(importer.menu()->actions().first()->menu()->actions().isEmpty());
    }
    void unknownItemLoadsOnlyItsAncestors()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        QSignalSpy requested(&importer, &DBusMenuImporter::actionActivationRequested);
        service.activate(4);
        QTRY_COMPARE(requested.size(), 1);
        auto *action = qvariant_cast<QAction *>(requested.first().first());
        QVERIFY(action);
        QCOMPARE(action->property("_dbusmenu_id").toInt(), 4);
        QVERIFY(service.layouts.contains({0, -1}));
        QCOMPARE(service.treeProperties, QStringList{QStringLiteral("children-display")});
        QVERIFY(service.layouts.contains({1, 1}));
        QVERIFY(service.layouts.contains({3, 1}));
        QVERIFY(!service.layouts.contains({7, 1}));
        QVERIFY(importer.menu()->actions().at(1)->menu()->actions().isEmpty());
    }
    void unknownIdDoesNotLoadUnrelatedMenus()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        QSignalSpy requested(&importer, &DBusMenuImporter::actionActivationRequested);
        service.activate(999);
        QTRY_COMPARE(service.layouts.size(), 2);
        QTest::qWait(100);
        QCOMPARE(requested.size(), 0);
        QCOMPARE(service.layouts, (QList<QPair<int, int>>{{0, 1}, {0, -1}}));
    }
    void removedItemDoesNotRetryForever()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        QSignalSpy requested(&importer, &DBusMenuImporter::actionActivationRequested);
        service.removeAfterTree = true;
        service.activate(4);
        QTRY_COMPARE(service.layouts.size(), 3);
        QTest::qWait(100);
        QCOMPARE(requested.size(), 0);
        QCOMPARE(service.layouts.size(), 3);
    }
    void newerRequestSupersedesPendingRequest()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        QSignalSpy requested(&importer, &DBusMenuImporter::actionActivationRequested);
        service.activate(4);
        QTRY_VERIFY(service.layouts.contains({3, 1}));
        service.activate(2);
        QTRY_COMPARE(requested.size(), 1);
        QCOMPARE(qvariant_cast<QAction *>(requested.first().first())->property("_dbusmenu_id").toInt(), 2);
        QTest::qWait(200);
        QCOMPARE(requested.size(), 1);
    }
    void cancelledRequestDoesNotActivateAfterLoading()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        QSignalSpy requested(&importer, &DBusMenuImporter::actionActivationRequested);
        service.activate(4);
        QTRY_VERIFY(service.layouts.contains({3, 1}));
        importer.cancelActivationRequest();
        QTest::qWait(200);
        QCOMPARE(requested.size(), 0);
    }
    void receiverCanCancelRequestDuringActivation()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        QSignalSpy requested(&importer, &DBusMenuImporter::actionActivationRequested);
        connect(&importer, &DBusMenuImporter::actionActivationRequested,
                &importer, &DBusMenuImporter::cancelActivationRequest);
        service.activate(4);
        QTRY_COMPARE(requested.size(), 1);
        QTest::qWait(50);
        QCOMPARE(requested.size(), 1);
    }
    void itemLoadedWhileTreeRequestIsPending()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        QSignalSpy requested(&importer, &DBusMenuImporter::actionActivationRequested);
        connect(&importer, &DBusMenuImporter::actionActivationRequested,
                &importer, &DBusMenuImporter::cancelActivationRequest);
        service.treeDelay = 200;
        service.activate(2);
        QTRY_VERIFY(service.layouts.contains({0, -1}));
        importer.updateMenu(importer.menu()->actions().first()->menu());
        QTRY_COMPARE(importer.menu()->actions().first()->menu()->actions().size(), 2);
        QTRY_COMPARE(requested.size(), 1);
        QCOMPARE(qvariant_cast<QAction *>(requested.first().first())->property("_dbusmenu_id").toInt(), 2);
    }
    void coldSubmenu_data()
    {
        QTest::addColumn<int>("cancel");
        QTest::newRow("open-after-loading") << 0;
        QTest::newRow("close-parent") << 1;
        QTest::newRow("select-other-item") << 2;
    }
    void coldSubmenu()
    {
        QFETCH(int, cancel);
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        auto *parent = importer.menu()->actions().first()->menu();
        importer.updateMenu(parent);
        QTRY_COMPARE(parent->actions().size(), 2);
        auto *child = parent->actions().at(1)->menu();
        parent->popup(QPoint(10, 10));
        QTest::keyClick(parent, Qt::Key_N);
        if (cancel == 1) parent->hide();
        if (cancel == 2) parent->setActiveAction(parent->actions().first());
        QTRY_COMPARE(child->actions().size(), 1);
        if (cancel == 0) QTRY_VERIFY(child->isVisible());
        else { QTest::qWait(50); QVERIFY(!child->isVisible()); }
        child->hide();
        parent->hide();
    }
};
QTEST_MAIN(DBusMenuImporterTest)
#include "DBusMenuImporterTest.moc"
