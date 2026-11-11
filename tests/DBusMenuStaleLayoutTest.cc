// SPDX-License-Identifier: GPL-2.0-or-later
#include "dbusmenuimporter.h"
#include "dbusmenutypes_p.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusVirtualObject>
#include <QMenu>
#include <QPointer>
#include <QTest>

class StaleLayoutService : public QDBusVirtualObject
{
public:
    QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "stale-layout-server");
    QList<int> rootChildren{1, 7};
    QMap<int, QDBusMessage> replies;
    QMap<int, QDBusMessage> aboutReplies;
    int layouts = 0;
    int aboutCalls = 0;
    bool holdAboutToShow = false;
    bool failNextLayout = false;

    QString introspect(const QString &) const override { return {}; }

    DBusMenuLayoutItem item(int id, int depth) const
    {
        DBusMenuLayoutItem result;
        result.id = id;
        result.properties = {{"label", QStringLiteral("Item %1").arg(id)}, {"enabled", true}, {"visible", true}};
        if (id == 0 || id == 1 || id == 7) result.properties.insert("children-display", "submenu");
        if (depth == 0) return result;
        const auto children = id == 0 ? rootChildren : id == 1 ? QList<int>{2} : id == 7 ? QList<int>{8} : QList<int>{};
        for (int child : children) result.children.append(item(child, depth - 1));
        return result;
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.member() == "GetLayout") {
            const int id = message.arguments().at(0).toInt();
            const int depth = message.arguments().at(1).toInt();
            // Model Qt's fixed root revision and independent submenu counters.
            const uint revision = id == 0 ? 1 : id == 1 ? 40 : 3;
            const QVariantList result{QVariant::fromValue(revision), QVariant::fromValue(item(id, depth))};
            replies.insert(++layouts, failNextLayout
                ? message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.Failed"), QStringLiteral("planned failure"))
                : message.createReply(result));
            failNextLayout = false;
        } else if (message.member() == "AboutToShow") {
            ++aboutCalls;
            if (holdAboutToShow) aboutReplies.insert(aboutCalls, message);
            else connection.send(message.createReply(QVariantList{false}));
        } else {
            connection.send(message.createReply());
        }
        return true;
    }

    bool reply(int request)
    {
        return replies.contains(request) && bus.send(replies.take(request));
    }

    bool replyAbout(int request, bool error)
    {
        if (!aboutReplies.contains(request)) return false;
        const auto message = aboutReplies.take(request);
        return bus.send(error ? message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.Failed"), QStringLiteral("planned failure"))
                              : message.createReply(QVariantList{false}));
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

    static QList<int> ids(QMenu *menu)
    {
        QList<int> result;
        for (auto *action : menu->actions()) result.append(action->property("_dbusmenu_id").toInt());
        return result;
    }

    static QPointer<QDBusPendingCallWatcher> pendingRequest(DBusMenuImporter &importer, int id)
    {
        for (auto *watcher : importer.findChildren<QDBusPendingCallWatcher *>()) {
            if (!watcher->isFinished() && watcher->property("_dbusmenu_id").isValid()
                && watcher->property("_dbusmenu_id").toInt() == id) return watcher;
        }
        return {};
    }

private slots:
    void initTestCase()
    {
        DBusMenuTypes_register();
        QVERIFY(service.bus.registerVirtualObject("/Menu", &service));
    }

    void init()
    {
        service.rootChildren = {1, 7};
        service.replies.clear();
        service.aboutReplies.clear();
        service.layouts = 0;
        service.aboutCalls = 0;
        service.holdAboutToShow = false;
        service.failNextLayout = false;
    }

    void staleRootReplyDoesNotRestorePreviousLayout_data()
    {
        QTest::addColumn<bool>("layoutSignal");
        QTest::newRow("layout-update") << true;
        QTest::newRow("about-to-show") << false;
    }

    void staleRootReplyDoesNotRestorePreviousLayout()
    {
        QFETCH(bool, layoutSignal);
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(service.layouts, 1);
        const auto oldRequest = pendingRequest(importer, 0);
        QVERIFY(oldRequest);
        QVERIFY(importer.menu()->actions().isEmpty());
        service.rootChildren = {7, 1};
        if (layoutSignal) service.layoutUpdated(0);
        else importer.updateMenu(importer.menu());
        QTRY_COMPARE(service.layouts, 2);
        QVERIFY(service.reply(2));
        QTRY_COMPARE(ids(importer.menu()), (QList<int>{7, 1}));
        QVERIFY(service.reply(1));
        QTRY_VERIFY(oldRequest.isNull()); // Wait for the obsolete callback, not a guessed delay.
        QCOMPARE(ids(importer.menu()), (QList<int>{7, 1}));
    }

    void repeatedUpdatesApplyOnlyLatest()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(service.layouts, 1);
        const auto first = pendingRequest(importer, 0);
        QVERIFY(first);
        service.rootChildren = {7};
        service.layoutUpdated(0);
        QTRY_COMPARE(service.layouts, 2);
        // The first request is still pending; select the other watcher.
        QPointer<QDBusPendingCallWatcher> second;
        for (auto *watcher : importer.findChildren<QDBusPendingCallWatcher *>()) {
            if (!watcher->isFinished() && watcher->property("_dbusmenu_id").isValid() && watcher != first) second = watcher;
        }
        QVERIFY(second);
        service.rootChildren = {1};
        service.layoutUpdated(0);
        QTRY_COMPARE(service.layouts, 3);
        QVERIFY(service.reply(3));
        QTRY_COMPARE(ids(importer.menu()), QList<int>{1});
        QVERIFY(service.reply(2));
        QTRY_VERIFY(second.isNull());
        QCOMPARE(ids(importer.menu()), QList<int>{1});
        QVERIFY(service.reply(1));
        QTRY_VERIFY(first.isNull());
        QCOMPARE(ids(importer.menu()), QList<int>{1});
    }

    void dbusErrorDoesNotBlockLaterUpdates()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(service.layouts, 1);
        QVERIFY(service.reply(1));
        QTRY_COMPARE(ids(importer.menu()), (QList<int>{1, 7}));
        service.failNextLayout = true;
        importer.updateMenu(importer.menu());
        QTRY_COMPARE(service.layouts, 2);
        const auto failed = pendingRequest(importer, 0);
        QVERIFY(failed);
        QVERIFY(service.reply(2));
        QTRY_VERIFY(failed.isNull());
        QCOMPARE(ids(importer.menu()), (QList<int>{1, 7}));
        service.rootChildren = {7, 1};
        importer.updateMenu(importer.menu());
        QTRY_COMPARE(service.layouts, 3);
        QVERIFY(service.reply(3));
        QTRY_COMPARE(ids(importer.menu()), (QList<int>{7, 1}));
    }

    void obsoleteAboutErrorDoesNotUnlockNewRequest()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(service.layouts, 1);
        QVERIFY(service.reply(1));
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        service.holdAboutToShow = true;
        importer.updateMenu(importer.menu());
        QTRY_COMPARE(service.aboutCalls, 1);
        const auto oldAbout = pendingRequest(importer, 0);
        QVERIFY(oldAbout);
        service.layoutUpdated(0);
        QTRY_COMPARE(service.layouts, 2);
        importer.updateMenu(importer.menu());
        QTRY_COMPARE(service.aboutCalls, 2);
        QVERIFY(service.replyAbout(1, true));
        QTRY_VERIFY(oldAbout.isNull());
        importer.updateMenu(importer.menu());
        // Complete the latest AboutToShow; it must be the only pending activation.
        QVERIFY(service.replyAbout(2, false));
        QTRY_COMPARE(service.layouts, 3);
        QCOMPARE(service.aboutCalls, 2);
        QVERIFY(service.reply(3));
    }

    void recreatedMenuIgnoresPreviousInstanceReply()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(service.layouts, 1);
        QVERIFY(service.reply(1));
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        const QPointer<QMenu> original(importer.actionForId(1)->menu());
        importer.updateMenu(original);
        QTRY_COMPARE(service.layouts, 2);
        const auto oldRequest = pendingRequest(importer, 1);
        QVERIFY(oldRequest);
        service.rootChildren = {7};
        service.layoutUpdated(0);
        QTRY_COMPARE(service.layouts, 3);
        QVERIFY(service.reply(3));
        QTRY_VERIFY(original.isNull());
        service.rootChildren = {1, 7};
        service.layoutUpdated(0);
        QTRY_COMPARE(service.layouts, 4);
        QVERIFY(service.reply(4));
        QTRY_VERIFY(importer.actionForId(1));
        auto *recreated = importer.actionForId(1)->menu();
        QVERIFY(recreated);
        QVERIFY(recreated->actions().isEmpty());
        importer.updateMenu(recreated);
        QTRY_COMPARE(service.layouts, 5);
        // Equal counters do not make two different menu instances equivalent.
        QCOMPARE(recreated->property("_dbusmenu_layout_generation"), oldRequest->property("_dbusmenu_layout_generation"));
        QVERIFY(service.reply(2));
        QTRY_VERIFY(oldRequest.isNull());
        QVERIFY(recreated->actions().isEmpty());
        importer.updateMenu(recreated);
        QVERIFY(service.reply(5));
        QTRY_COMPARE(ids(recreated), QList<int>{2});
        QCOMPARE(service.layouts, 5);
    }

    void customRootMenuFactory()
    {
        class CustomImporter : public DBusMenuImporter
        {
        public:
            using DBusMenuImporter::DBusMenuImporter;
            QMenu *createMenu(QWidget *parent) override
            {
                auto *menu = new QMenu(parent);
                menu->setObjectName(QStringLiteral("custom-menu"));
                return menu;
            }
        } importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(service.layouts, 1);
        QCOMPARE(importer.menu()->objectName(), QStringLiteral("custom-menu"));
        QVERIFY(service.reply(1));
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
    }
};

QTEST_MAIN(DBusMenuStaleLayoutTest)
#include "DBusMenuStaleLayoutTest.moc"
