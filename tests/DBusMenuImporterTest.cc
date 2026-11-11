// SPDX-License-Identifier: GPL-2.0-or-later
#include "dbusmenuimporter.h"
#include "dbusmenutypes_p.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QDBusPendingCallWatcher>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QMenu>
#include <QKeyEvent>

class MenuService : public QDBusVirtualObject
{
public:
    QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "menu-test-server");
    QList<QPair<int, int>> layouts;
    QString actionLabel = QStringLiteral("_Save");
    bool emptyRoot = false;
    bool missingNested = false;
    bool removeAfterTree = false;
    QStringList treeProperties;
    int treeDelay = 0;
    int nestedOffset = 0;
    bool blankNestedAction = false;
    bool duplicateNestedAction = false;
    bool renameNestedAction = false;
    QMap<int, QList<int>> customLayouts;
    QMap<int, uint> menuRevisions;
    bool kdeTitle = false;
    int disabledId = -1;
    QList<int> clicked;
    QString introspect(const QString &) const override { return {}; }
    DBusMenuLayoutItem tree(int id, int depth) const
    {
        DBusMenuLayoutItem item;
        item.id = id;
        item.properties = {{"label", id == 1 ? "_File" : id == 2 ? actionLabel : id == 3 ? "_Nested" : "_Run"},
                           {"enabled", id != disabledId}, {"visible", true}};
        if (id == 2 && kdeTitle) item.properties.insert("x-kde-title", true);
        if (id == 0 || id == 1 || id == 3 || id == 7) item.properties.insert("children-display", "submenu");
        if (depth == 0) return item;
        const int next = depth < 0 ? -1 : depth - 1;
        if (id == 0 && !emptyRoot) item.children = {tree(1, next), tree(7, next)};
        if (id == 1) {
            item.children = {tree(2, next)};
            if (!missingNested) item.children.append(tree(3, next));
        }
        if (id == 3) {
            auto command = tree(4, next);
            command.id += nestedOffset;
            if (blankNestedAction) command.properties.insert("label", "");
            if (renameNestedAction) command.properties.insert("label", "_Other");
            command.properties.insert("icon-name", "utilities-terminal");
            item.children = {command};
            if (duplicateNestedAction) {
                ++command.id;
                item.children.append(command);
            }
        }
        if (id == 7) item.children = {tree(8, next)};
        if (customLayouts.contains(id)) {
            item.children.clear();
            for (int child : customLayouts.value(id)) item.children.append(tree(child, next));
        }
        return item;
    }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override
    {
        if (message.member() == "GetLayout") {
            const int id = message.arguments().at(0).toInt();
            const int depth = message.arguments().at(1).toInt();
            layouts.append({id, depth});
            const QVariantList result{QVariant::fromValue(menuRevisions.value(id, 1)), QVariant::fromValue(tree(id, depth))};
            if (depth < 0) {
                treeProperties = message.arguments().at(2).toStringList();
                if (removeAfterTree) missingNested = true;
            }
            QTimer::singleShot(depth < 0 ? treeDelay : id == 3 ? 150 : 0, this, [bus, message, result] { bus.send(message.createReply(result)); });
        } else {
            if (message.member() == "Event" && message.arguments().at(1).toString() == "clicked") {
                clicked.append(message.arguments().first().toInt());
            }
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

class DestroyImporterOnRetry : public QObject
{
public:
    QPointer<DBusMenuImporter> importer;
    bool armed = false;
    int openingKey = Qt::Key_Right;
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (armed && event->type() == QEvent::KeyPress
            && static_cast<QKeyEvent *>(event)->key() == openingKey) {
            armed = false;
            delete importer.data();
            return true;
        }
        return false;
    }
};

class DBusMenuImporterTest : public QObject
{
    Q_OBJECT
    MenuService service;
    QDBusMessage fetchLayout(int id)
    {
        auto message = QDBusMessage::createMethodCall(service.bus.baseService(), "/Menu",
                                                     "com.canonical.dbusmenu", "GetLayout");
        message << id << 1 << QStringList{};
        QDBusPendingCallWatcher watcher(QDBusConnection::sessionBus().asyncCall(message));
        QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
        if (!watcher.isFinished()) finished.wait(5000);
        return watcher.reply();
    }
    bool deliver(DBusMenuImporter &importer, int id, const QDBusMessage &reply)
    {
        if (reply.type() != QDBusMessage::ReplyMessage) return false;
        auto *watcher = new QDBusPendingCallWatcher(QDBusPendingCall::fromCompletedCall(reply), &importer);
        watcher->setProperty("_dbusmenu_id", id);
        // Exercise layout application directly; request ordering is tested via D-Bus.
        QMenu *menu = id == 0 ? importer.menu() : importer.actionForId(id)->menu();
        watcher->setProperty("_dbusmenu_request_menu", QVariant::fromValue(QPointer<QMenu>(menu)));
        watcher->setProperty("_dbusmenu_layout_generation", menu->property("_dbusmenu_layout_generation"));
        return QMetaObject::invokeMethod(&importer, "slotGetLayoutFinished", Qt::DirectConnection,
                                         Q_ARG(QDBusPendingCallWatcher *, watcher));
    }
private slots:
    void initTestCase()
    {
        DBusMenuTypes_register();
        QVERIFY(service.bus.registerVirtualObject("/Menu", &service));
    }
    void init()
    {
        service.kdeTitle = false;
        service.disabledId = -1;
        service.customLayouts.clear();
        service.menuRevisions.clear();
        service.layouts.clear();
        service.actionLabel = QStringLiteral("_Save");
        service.emptyRoot = false;
        service.missingNested = false;
        service.removeAfterTree = false;
        service.treeProperties.clear();
        service.treeDelay = 0;
        service.nestedOffset = 0;
        service.blankNestedAction = false;
        service.duplicateNestedAction = false;
        service.renameNestedAction = false;
        service.clicked.clear();
    }
    void duplicateIdsPreserveLayout_data()
    {
        QTest::addColumn<QList<int>>("ids");
        QTest::newRow("adjacent") << QList<int>{1, 1};
        QTest::newRow("separated") << QList<int>{1, 7, 1};
        QTest::newRow("multiple") << QList<int>{1, 1, 7, 7};
    }
    void duplicateIdsPreserveLayout()
    {
        QFETCH(QList<int>, ids);
        const auto valid = fetchLayout(0);
        service.customLayouts[0] = ids;
        const auto invalid = fetchLayout(0);
        service.customLayouts[0] = {7};
        const auto subsequent = fetchLayout(0);
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QVERIFY(deliver(importer, 0, valid));
        const auto original = importer.menu()->actions();
        importer.menu()->setActiveAction(original.first());
        QVERIFY(deliver(importer, 0, invalid));
        QCOMPARE(importer.menu()->activeAction(), original.first());
        QCOMPARE(importer.menu()->actions(), original);
        QVERIFY(importer.menu()->updatesEnabled());
        QVERIFY(deliver(importer, 0, subsequent));
        QCOMPARE(importer.menu()->actions(), QList<QAction *>{importer.actionForId(7)});
    }
    void invalidReferencesPreserveLayout_data()
    {
        QTest::addColumn<int>("parentId");
        QTest::addColumn<int>("childId");
        QTest::newRow("reserved-root-id") << 0 << 0;
        QTest::newRow("self-reference") << 1 << 1;
        QTest::newRow("ancestor-reference") << 3 << 1;
    }
    void invalidReferencesPreserveLayout()
    {
        QFETCH(int, parentId);
        QFETCH(int, childId);
        const auto root = fetchLayout(0);
        const auto parentLayout = fetchLayout(1);
        const auto childLayout = fetchLayout(3);
        service.customLayouts[parentId] = {childId};
        const auto invalid = fetchLayout(parentId);
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QVERIFY(deliver(importer, 0, root));
        QVERIFY(deliver(importer, 1, parentLayout));
        QVERIFY(deliver(importer, 3, childLayout));
        QMenu *menu = parentId == 0 ? importer.menu() : importer.actionForId(parentId)->menu();
        const auto original = menu->actions();
        const auto originalParent = menu->parentWidget();
        QVERIFY(deliver(importer, parentId, invalid));
        QCOMPARE(menu->actions(), original);
        QCOMPARE(menu->parentWidget(), originalParent);
        QVERIFY(menu->updatesEnabled());
        const auto valid = parentId == 0 ? root : parentId == 1 ? parentLayout : childLayout;
        QVERIFY(deliver(importer, parentId, valid));
        QCOMPARE(menu->actions(), original);
    }
    void mismatchedRootPreservesLayout()
    {
        const auto root = fetchLayout(0);
        const auto valid = fetchLayout(1);
        const auto wrongRoot = fetchLayout(7);
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QVERIFY(deliver(importer, 0, root));
        QVERIFY(deliver(importer, 1, valid));
        auto *menu = importer.actionForId(1)->menu();
        const auto original = menu->actions();
        QVERIFY(deliver(importer, 1, wrongRoot));
        QCOMPARE(menu->actions(), original);
        QVERIFY(!importer.actionForId(8));
        QVERIFY(deliver(importer, 1, valid));
        QCOMPARE(menu->actions(), original);
    }
    void kdeTitleRetiresAndReusesId()
    {
        service.kdeTitle = true;
        service.customLayouts[1] = {2};
        const auto root = fetchLayout(0);
        const auto populated = fetchLayout(1);
        service.customLayouts[1] = {};
        const auto empty = fetchLayout(1);
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QVERIFY(deliver(importer, 0, root));
        QVERIFY(deliver(importer, 1, populated));
        const QPointer<QAction> original(importer.actionForId(2));
        QVERIFY(original);
        QCOMPARE(original->property("_dbusmenu_id").toInt(), 2);
        QVERIFY(deliver(importer, 1, empty));
        QVERIFY(!importer.actionForId(2));
        QVERIFY(deliver(importer, 1, populated));
        const QPointer<QAction> replacement(importer.actionForId(2));
        QVERIFY(replacement);
        QVERIFY(replacement != original);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(original.isNull());
        QVERIFY(replacement);
        QCOMPARE(importer.actionForId(2), replacement.data());
        QCOMPARE(importer.actionForId(1)->menu()->actions(), QList<QAction *>{replacement.data()});
    }
    void independentMenuRevisions()
    {
        // Qt exports root revision 1 and separate counters for each submenu.
        service.menuRevisions = {{0, 1}, {1, 40}, {7, 3}, {3, 2}};
        const auto root = fetchLayout(0);
        const auto first = fetchLayout(1);
        const auto second = fetchLayout(7);
        const auto nested = fetchLayout(3);
        service.customLayouts[0] = {7, 1};
        const auto reorderedRoot = fetchLayout(0);
        service.customLayouts.remove(0);
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QVERIFY(deliver(importer, 0, root));
        QVERIFY(deliver(importer, 1, first));
        QVERIFY(deliver(importer, 7, second));
        QVERIFY(deliver(importer, 3, nested));
        QVERIFY(importer.actionForId(2));
        QVERIFY(importer.actionForId(4));
        QVERIFY(importer.actionForId(8));
        const QList<QAction *> firstActions{importer.actionForId(2), importer.actionForId(3)};
        const QList<QAction *> secondActions{importer.actionForId(8)};
        const QList<QAction *> nestedActions{importer.actionForId(4)};
        QCOMPARE(importer.actionForId(1)->menu()->actions(), firstActions);
        QCOMPARE(importer.actionForId(7)->menu()->actions(), secondActions);
        QCOMPARE(importer.actionForId(3)->menu()->actions(), nestedActions);
        QCOMPARE(importer.menu()->actions(), (QList<QAction *>{importer.actionForId(1), importer.actionForId(7)}));
        // The root still reports revision 1, even when its contents change.
        QVERIFY(deliver(importer, 0, reorderedRoot));
        QCOMPARE(importer.menu()->actions(), (QList<QAction *>{importer.actionForId(7), importer.actionForId(1)}));
        QCOMPARE(importer.actionForId(1)->menu()->actions(), firstActions);
        QCOMPARE(importer.actionForId(7)->menu()->actions(), secondActions);
        QCOMPARE(importer.actionForId(3)->menu()->actions(), nestedActions);
    }
    void childLimitStillApplies()
    {
        QList<int> ids;
        for (int id = 10; id < 1010; ++id) ids.append(id);
        // Entries beyond the existing limit are ignored, including duplicates.
        ids.append(10);
        service.customLayouts[0] = ids;
        const auto layout = fetchLayout(0);
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QVERIFY(deliver(importer, 0, layout));
        QCOMPARE(importer.menu()->actions().size(), 1000);
        QCOMPARE(importer.menu()->actions().first(), importer.actionForId(10));
        QCOMPARE(importer.menu()->actions().last(), importer.actionForId(1009));
    }
    void changedSignalSeesTransferredOwnership_data()
    {
        QTest::addColumn<int>("movedId");
        QTest::newRow("action") << 2;
        QTest::newRow("submenu") << 3;
    }
    void changedSignalSeesTransferredOwnership()
    {
        QFETCH(int, movedId);
        service.menuRevisions = {{0, 1}, {1, 40}, {7, 3}};
        service.customLayouts[1] = {movedId};
        const auto root = fetchLayout(0);
        const auto sourceLayout = fetchLayout(1);
        service.customLayouts[7] = {movedId};
        service.disabledId = movedId;
        const auto destinationLayout = fetchLayout(7);
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QVERIFY(deliver(importer, 0, root));
        QVERIFY(deliver(importer, 1, sourceLayout));
        auto *source = importer.actionForId(1)->menu();
        auto *destination = importer.actionForId(7)->menu();
        auto *action = importer.actionForId(movedId);
        QVERIFY(action);
        bool changed = false;
        connect(action, &QAction::changed, &importer, [&] {
            changed = true;
            QCOMPARE(action->parent(), destination);
            QVERIFY(!source->actions().contains(action));
            QVERIFY(destination->actions().contains(action));
            if (QMenu *subMenu = action->menu()) {
                QCOMPARE(subMenu->parentWidget(), destination);
            }
        });
        QVERIFY(deliver(importer, 7, destinationLayout));
        QVERIFY(changed);
        QVERIFY(!action->isEnabled());
        QCOMPARE(destination->actions(), QList<QAction *>{action});
    }
    void movesBetweenMenus_data()
    {
        QTest::addColumn<bool>("newFirst");
        QTest::addColumn<int>("movedId");
        QTest::newRow("leaf-new-parent-first") << true << 2;
        QTest::newRow("leaf-old-parent-first") << false << 2;
        QTest::newRow("submenu-new-parent-first") << true << 3;
        QTest::newRow("submenu-old-parent-first") << false << 3;
    }
    void movesBetweenMenus()
    {
        QFETCH(bool, newFirst);
        QFETCH(int, movedId);
        service.customLayouts[1] = {movedId};
        service.customLayouts[7] = {};
        const auto root = fetchLayout(0);
        const auto oldBefore = fetchLayout(1);
        const auto nested = fetchLayout(3);
        service.customLayouts[1] = {};
        service.customLayouts[7] = {movedId};
        const auto oldAfter = fetchLayout(1);
        const auto newAfter = fetchLayout(7);
        service.customLayouts[0] = {7};
        const auto removeOldParent = fetchLayout(0);
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QVERIFY(deliver(importer, 0, root));
        QVERIFY(deliver(importer, 1, oldBefore));
        if (movedId == 3) QVERIFY(deliver(importer, 3, nested));
        const QPointer<QAction> original(importer.actionForId(movedId));
        const QPointer<QAction> originalChild(importer.actionForId(4));
        if (newFirst) {
            QVERIFY(deliver(importer, 7, newAfter));
            QVERIFY(deliver(importer, 1, oldAfter));
        } else {
            QVERIFY(deliver(importer, 1, oldAfter));
            QVERIFY(!importer.actionForId(movedId));
            if (movedId == 3) QVERIFY(!importer.actionForId(4));
            QVERIFY(deliver(importer, 7, newAfter));
        }
        // Reuse descendant IDs before the retired branch is actually destroyed.
        if (movedId == 3) QVERIFY(deliver(importer, 3, nested));
        const QPointer<QAction> survivingChild(importer.actionForId(4));
        QVERIFY(deliver(importer, 0, removeOldParent));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        auto *moved = importer.actionForId(movedId);
        QVERIFY(moved);
        auto *destination = importer.actionForId(7)->menu();
        QCOMPARE(destination->actions(), QList<QAction *>{moved});
        QCOMPARE(moved->parent(), destination);
        if (movedId == 3) {
            QVERIFY(moved->menu());
            QCOMPARE(moved->menu()->parentWidget(), destination);
            QVERIFY(moved->menu()->windowFlags().testFlag(Qt::Popup));
            QVERIFY(survivingChild);
            QCOMPARE(importer.actionForId(4), survivingChild.data());
            QCOMPARE(moved->menu()->actions(), QList<QAction *>{survivingChild.data()});
        }
        if (newFirst) {
            QCOMPARE(moved, original.data());
            if (movedId == 3) QCOMPARE(survivingChild, originalChild);
        } else {
            QVERIFY(original.isNull());
            QVERIFY(originalChild.isNull());
        }
    }
    void reusedIdSurvivesDeferredDestruction()
    {
        // Obtain real, serialized replies before constructing the importer, so
        // their delivery can be controlled without processing deferred deletes.
        QList<QDBusMessage> replies;
        for (bool empty : {false, true}) {
            service.emptyRoot = empty;
            const auto reply = fetchLayout(0);
            QCOMPARE(reply.type(), QDBusMessage::ReplyMessage);
            replies.append(reply);
        }
        service.emptyRoot = false;

        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QVERIFY(deliver(importer, 0, replies.at(0)));
        const QPointer<QAction> original(importer.actionForId(1));
        QVERIFY(original);
        QVERIFY(deliver(importer, 0, replies.at(1)));
        QVERIFY(deliver(importer, 0, replies.at(0)));
        const QPointer<QAction> replacement(importer.actionForId(1));
        QVERIFY(replacement);
        QVERIFY(replacement != original);

        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(original.isNull());
        QCOMPARE(importer.actionForId(1), replacement.data());
        QCOMPARE(importer.menu()->actions().size(), 2);

        // A subsequent refresh must retain the same action, without creating a
        // duplicate or hitting the importer's layout consistency assertion.
        QVERIFY(deliver(importer, 0, replies.at(0)));
        QCOMPARE(importer.actionForId(1), replacement.data());
        QCOMPARE(importer.menu()->actions().size(), 2);
        replacement->trigger();
        QTRY_COMPARE(service.clicked, QList<int>{1});

        // Destruction outside layout removal must still clear the mapped ID.
        delete replacement.data();
        QVERIFY(replacement.isNull());
        QVERIFY(!importer.actionForId(1));
    }

    void importsFilenameUnderscores_data()
    {
        QTest::addColumn<QString>("label");
        QTest::addColumn<QString>("expected");
        const auto row = [](const char *name, const char *label, const char *expected) {
            QTest::newRow(name) << QString::fromUtf8(label) << QString::fromUtf8(expected);
        };
        row("filename", "nome_file.txt", "nome_file.txt");
        row("unlisted-extension", "nome_file.custom-format", "nome_file.custom-format");
        row("extension-with-symbols", "nome_file.c++", "nome_file.c++");
        row("numeric-extension", "nome_file.7z", "nome_file.7z");
        row("filename-with-brackets", "nome_file [draft].txt", "nome_file [draft].txt");
        row("numeric-filename", "video_1.txt", "video_1.txt");
        row("recent-file-with-path", "nome_file.txt [/home/user/nome_file.txt]", "nome_file.txt [/home/user/nome_file.txt]");
        row("filename-before-annotations", "nome_file.txt [/home/user/nome_file.txt] [modified]", "nome_file.txt [/home/user/nome_file.txt] [modified]");
        row("extensionless-with-path", "nome_file [/home/user/nome_file]", "nome&file [/home/user/nome_file]");
        row("extensionless-path", "/home/user/nome_file", "/home/user/nome&file");
        row("non-path-brackets", "Open_Recent [Ctrl+O]", "Open&Recent [Ctrl+O]");
        row("nested-path-in-description", "Open_Recent [help [/tmp]]", "Open&Recent [help [/tmp]]");
        row("extension-inside-description", "Open_Recent [help.txt [notes]]", "Open&Recent [help.txt [notes]]");
        row("command-with-path-suffix", "Open_Recent /tmp/file.txt", "Open&Recent /tmp/file.txt");
        row("command-with-underscore-path", "Open_Recent /tmp/file_name.txt", "Open&Recent /tmp/file_name.txt");
        row("command-with-windows-path", R"(Open_Recent C:\tmp\file.txt)", R"(Open&Recent C:\tmp\file.txt)");
        row("filename-with-spaces", "nome_file di prova.txt", "nome_file di prova.txt");
        row("filename-in-path", "/tmp/nome_file.txt", "/tmp/nome_file.txt");
        row("filename-in-windows-path", R"(C:\tmp\nome_file.txt)", R"(C:\tmp\nome_file.txt)");
        row("absolute-path-directory-underscore", "/home/my_folder/report_final.txt", "/home/my_folder/report_final.txt");
        row("absolute-path-with-spaces", "/home/my_folder/other folder/report_final.txt", "/home/my_folder/other folder/report_final.txt");
        row("windows-directory-underscore", R"(C:\my_folder\report_final.txt)", R"(C:\my_folder\report_final.txt)");
        row("unc-directory-underscore", R"(\\server\my_folder\report_final.txt)", R"(\\server\my_folder\report_final.txt)");
        row("extensionless-absolute-directory", "/home/my_folder/report_final", "/home/my&folder/report_final");
        row("literal-ampersand", "nome_file&copy.txt", "nome_file&&copy.txt");
        row("gtk-escaped", "nome__file.txt", "nome_file.txt");
        row("gtk-mixed-escapes", "nome__file____test__1.svg", "nome_file__test_1.svg");
        row("gtk-mnemonic-and-escape", "_nome__file.txt", "&nome_file.txt");
        row("numbered-mnemonic", "_1 nome_file.txt", "&1 nome_file.txt");
        row("path-mnemonic", "/_home/user/nome_file", "/&home/user/nome_file");
        row("ordinary-mnemonic", "Open_Recent", "Open&Recent");
        row("ordinary-ellipsis", "Save_as...", "Save&as...");
        row("ambiguous-extensionless", "nome_file", "nome&file");
    }
    void importsFilenameUnderscores()
    {
        QFETCH(QString, label);
        QFETCH(QString, expected);
        service.actionLabel = label;
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        auto *menu = importer.menu()->actions().first()->menu();
        importer.updateMenu(menu);
        QTRY_COMPARE(menu->actions().size(), 2);
        QAction *action = menu->actions().first();
        QCOMPARE(action->text(), expected);
        QCOMPARE(action->property("_dbusmenu_label").toString(), label);
    }
    void refreshedIdsPreserveSelection_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("same-command") << 0;
        QTest::newRow("icon-only-command") << 1;
        QTest::newRow("ambiguous-replacement") << 2;
        QTest::newRow("different-command") << 3;
        QTest::newRow("reassigned-mnemonic") << 4;
    }
    void refreshedIdsPreserveSelection()
    {
        QFETCH(int, mode);
        service.blankNestedAction = mode == 1;
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        auto *parent = importer.menu()->actions().first()->menu();
        importer.updateMenu(parent);
        QTRY_COMPARE(parent->actions().size(), 2);
        auto *child = parent->actions().at(1)->menu();
        importer.updateMenu(child);
        QTRY_COMPARE(child->actions().size(), 1);
        child->popup(QPoint(10, 10));
        QTest::keyClick(child, Qt::Key_Down);
        QCOMPARE(child->activeAction(), child->actions().first());
        if (mode == 4) child->activeAction()->setText(QStringLiteral("R&un"));

        service.nestedOffset = 100;
        service.duplicateNestedAction = mode == 2;
        service.renameNestedAction = mode == 3;
        QSignalSpy updated(&importer, &DBusMenuImporter::menuUpdated);
        importer.updateMenu(child);
        QTRY_VERIFY(!updated.isEmpty());
        QTRY_COMPARE(child->actions().first()->property("_dbusmenu_id").toInt(), 104);
        if (mode == 2 || mode == 3) {
            QVERIFY(!child->activeAction());
            QVERIFY(service.clicked.isEmpty());
        } else {
            QCOMPARE(child->activeAction(), child->actions().first());
            QTest::keyClick(child, Qt::Key_Return);
            QTRY_COMPARE(service.clicked, QList<int>{104});
        }
        child->hide();
    }
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
    void destroyDuringRetry_data()
    {
        QTest::addColumn<Qt::LayoutDirection>("direction");
        QTest::newRow("ltr") << Qt::LeftToRight;
        QTest::newRow("rtl") << Qt::RightToLeft;
    }
    void destroyDuringRetry()
    {
        QFETCH(Qt::LayoutDirection, direction);
        DestroyImporterOnRetry filter;
        filter.openingKey = direction == Qt::RightToLeft ? Qt::Key_Left : Qt::Key_Right;
        filter.importer = new DBusMenuImporter(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(filter.importer->menu()->actions().size(), 2);
        auto *parent = filter.importer->menu()->actions().first()->menu();
        filter.importer->updateMenu(parent);
        QTRY_COMPARE(parent->actions().size(), 2);
        auto *child = parent->actions().at(1)->menu();
        parent->setLayoutDirection(direction);
        parent->installEventFilter(&filter);
        connect(filter.importer, &DBusMenuImporter::menuUpdated, &filter, [&filter, child](QMenu *updated) {
            if (updated == child) filter.armed = true;
        });
        parent->popup(QPoint(10, 10));
        QTest::keyClick(parent, Qt::Key_N);
        QTRY_VERIFY(filter.importer.isNull());
    }
    void replacedSubmenuCancelsRetry()
    {
        DBusMenuImporter importer(service.bus.baseService(), "/Menu");
        QTRY_COMPARE(importer.menu()->actions().size(), 2);
        auto *parent = importer.menu()->actions().first()->menu();
        importer.updateMenu(parent);
        QTRY_COMPARE(parent->actions().size(), 2);
        auto *action = parent->actions().at(1);
        auto *original = action->menu();
        QMenu replacement;
        replacement.addAction("Replacement");
        bool replaced = false;
        connect(&importer, &DBusMenuImporter::menuUpdated, &importer, [&](QMenu *updated) {
            if (updated == original) {
                action->setMenu(&replacement);
                replaced = true;
            }
        });
        parent->popup(QPoint(10, 10));
        QTest::keyClick(parent, Qt::Key_N);
        QTRY_VERIFY(replaced);
        QTest::qWait(50);
        QVERIFY(!replacement.isVisible());
        QCOMPARE(action->menu(), &replacement);
        parent->hide();
        action->setMenu(original);
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
