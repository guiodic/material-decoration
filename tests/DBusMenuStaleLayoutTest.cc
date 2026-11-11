// SPDX-License-Identifier: GPL-2.0-or-later
#include "dbusmenuimporter.h"
#include "dbusmenutypes_p.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusVirtualObject>
#include <QMenu>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

class StaleLayoutService : public QDBusVirtualObject
{
public:
    QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "stale-layout-server");
    QList<int> rootChildren{1, 7};
    QList<int> rootDelays;
    QMap<int, int> layoutDelays;
    bool failNextRootLayout = false;
    QList<QPair<int, int>> layouts;

    QString introspect(const QString &) const override { return {}; }

    DBusMenuLayoutItem item(int id) const
    {
        DBusMenuLayoutItem result;
        result.id = id;
        result.properties = {{"label", id == 1 ? "_File" : id == 2 ? "_Save" : id == 3 ? "_Nested" : id == 4 ? "_Run" : "_Other"},
                             {"enabled", true}, {"visible", true}};
        if (id == 0 || id == 1 || id == 3 || id == 7) {
            result.properties.insert("children-display", "submenu");
        }
        if (id == 0) {
            for (int child : rootChildren) result.children.append(item(child));
        } else if (id == 1) {
            result.children.append(item(2));
        } else if (id == 3) {
            result.children.append(item(4));
        } else if (id == 7) {
            result.children.append(item(8));
        }
        return result;
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.member() == "GetLayout") {
            const int id = message.arguments().at(0).toInt();
            const int depth = message.arguments().at(1).toInt();
            layouts.append({id, depth});

            const QVariantList result{QVariant::fromValue(uint(1)), QVariant::fromValue(item(id))};
            int delay = 0;
            if (id == 0) {
                delay = rootDelays.isEmpty() ? 0 : rootDelays.takeFirst();
                if (failNextRootLayout) {
                    failNextRootLayout = false;
                    QTimer::singleShot(delay, this, [connection, message] {
                        connection.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                                                                 QStringLiteral("planned failure")));
                    });
                    return true;
                }
            } else {
                delay = layoutDelays.value(id, 0);
            }

            QTimer::singleShot(delay, this, [connection, message, result] {
                connection.send(message.createReply(result));
            });
            return true;
        }

        if (message.member() == "AboutToShow") {
            connection.send(message.createReply(QVariantList{false}));
        } else {
            connection.send(message.createReply(QVariantList{}));
        }
        return true;
    }

    void layoutUpdated(int parentId)
    {
        auto message = QDBusMessage::createSignal("/Menu", "com.canonical.dbusmenu", "LayoutUpdated");
        message << uint(1) << parentId;
        bus.send(message);
    }
};

class DBusMenuStaleLayoutTest : public QObject
{
    Q_OBJECT

    StaleLayoutService service;

private slots:
    void initTestCase()
    {
        DBusMenuTypes_register();
        QVERIFY(service.bus.registerVirtualObject("/Menu", &service));
    }

    void init()
    {
        service.rootChildren = {1, 7};
        service.rootDelays.clear();
        service.layoutDelays.clear();
        service.failNextRootLayout = false;
        service.layouts.clear();
    }

    void staleRootReplyDoesNotRestorePreviousLayout()
    {
        service.rootDelays = {200, 0};
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        QCOMPARE(importer.menu()->actions().first()->property("_dbusmenu_id").toInt(), 1);

        service.rootChildren = {7, 1};
        service.layoutUpdated(0);
        QTRY_COMPARE(importer.menu()->actions().first()->property("_dbusmenu_id").toInt(), 7);
        QCOMPARE(importer.menu()->actions().size(), 2);

        QTest::qWait(250);
        QCOMPARE(importer.menu()->actions().first()->property("_dbusmenu_id").toInt(), 7);
        QCOMPARE(importer.menu()->actions().size(), 2);
    }

    void repeatedUpdatesApplyOnlyLatest()
    {
        service.rootDelays = {150, 100, 0};
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        QCOMPARE(importer.menu()->actions().first()->property("_dbusmenu_id").toInt(), 1);

        service.rootChildren = {7, 1};
        service.layoutUpdated(0);
        QTRY_COMPARE(service.layouts.size(), 2);

        service.rootChildren = {1, 7};
        service.layoutUpdated(0);
        QTRY_COMPARE(service.layouts.size(), 3);

        service.rootChildren = {7, 1};
        service.layoutUpdated(0);
        QTRY_COMPARE(service.layouts.size(), 4);
        QTRY_COMPARE(importer.menu()->actions().first()->property("_dbusmenu_id").toInt(), 7);

        QTest::qWait(300);
        QCOMPARE(importer.menu()->actions().first()->property("_dbusmenu_id").toInt(), 7);
        QCOMPARE(importer.menu()->actions().size(), 2);
    }

    void dbusErrorDoesNotBlockLaterUpdates()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        QCOMPARE(importer.menu()->actions().first()->property("_dbusmenu_id").toInt(), 1);

        service.failNextRootLayout = true;
        service.rootChildren = {7, 1};
        service.layoutUpdated(0);
        QTRY_COMPARE(service.layouts.size(), 2);
        QCOMPARE(importer.menu()->actions().first()->property("_dbusmenu_id").toInt(), 1);

        service.layoutUpdated(0);
        QTRY_COMPARE(importer.menu()->actions().first()->property("_dbusmenu_id").toInt(), 7);
        QCOMPARE(importer.menu()->actions().size(), 2);
    }

    void recreatedMenuIgnoresPreviousInstanceReply()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);

        QMenu *menu = importer.actionForId(1)->menu();
        importer.updateMenu(menu);
        QTRY_COMPARE(menu->actions().size(), 1);

        service.layoutDelays[1] = 200;
        service.layoutUpdated(1);
        QTRY_COMPARE(service.layouts.size(), 3);

        service.rootChildren = {7};
        service.layoutUpdated(0);
        QTRY_COMPARE(service.layouts.size(), 4);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        service.rootChildren = {1, 7};
        service.layoutUpdated(0);
        QTRY_COMPARE(service.layouts.size(), 5);

        QMenu *recreated = importer.actionForId(1)->menu();
        QVERIFY(recreated);
        QVERIFY(recreated != menu);
        QTest::qWait(300);
        QVERIFY(recreated->actions().isEmpty());
    }
};

QTEST_MAIN(DBusMenuStaleLayoutTest)
#include "DBusMenuStaleLayoutTest.moc"
