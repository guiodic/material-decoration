// SPDX-License-Identifier: GPL-2.0-or-later
#include "TilingController.h"
#include "DecorationTestBackend.h"
#include <KDecoration3/Decoration>
#include <KDecoration3/DecoratedWindow>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QSet>
#include <QTest>
#include <QTimer>
#include <QUuid>
#include <QVariantMap>

using TestBridge = MaterialTest::Bridge;

class TestDecoration : public KDecoration3::Decoration {
public:
    TestDecoration(QObject *owner, TestBridge *bridge)
        : Decoration(owner, {QVariantMap{{QStringLiteral("bridge"),
            QVariant::fromValue<KDecoration3::DecorationBridge *>(bridge)}}}) {}
    bool init() override { return true; }
    void paint(QPainter *, const QRectF &) override {}
};

class ScriptAction : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kglobalaccel.Component")
public:
    QList<QVariantMap> requests;
    bool respond = true;
    bool legacyReply = false;
    QString layoutJson = QStringLiteral("[]");
public slots:
    void invokeShortcut(const QString &name) {
        if (name != QStringLiteral("MaterialTilingPopupDispatch") || !respond) return;
        auto message = QDBusMessage::createMethodCall(QStringLiteral("com.github.guiodic.MaterialTiling"), QStringLiteral("/Tiling"),
            QStringLiteral("com.github.guiodic.MaterialTiling"), QStringLiteral("takeRequest"));
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
            QDBusPendingReply<QVariantMap> reply = *call;
            call->deleteLater();
            if (reply.isError() || reply.value().isEmpty()) return;
            requests.append(reply.value());
            auto message = QDBusMessage::createMethodCall(QStringLiteral("com.github.guiodic.MaterialTiling"), QStringLiteral("/Tiling"),
                QStringLiteral("com.github.guiodic.MaterialTiling"), QStringLiteral("complete"));
            if (reply.value().value(QStringLiteral("layout")).toBool() && !legacyReply) {
                message = QDBusMessage::createMethodCall(QStringLiteral("com.github.guiodic.MaterialTiling"), QStringLiteral("/Tiling"),
                    QStringLiteral("com.github.guiodic.MaterialTiling"), QStringLiteral("completeLayout"));
                message << reply.value().value(QStringLiteral("id")).toString() << layoutJson;
            } else {
                message << reply.value().value(QStringLiteral("id")).toString() << true << QString();
            }
            QDBusConnection::sessionBus().asyncCall(message);
        });
    }
};

class TilingControllerTest : public QObject {
    Q_OBJECT
    ScriptAction action;
    QObject window;
    TestBridge bridge;
private slots:
    void initTestCase() {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerService(QStringLiteral("org.kde.kglobalaccel")));
        QVERIFY(bus.registerObject(QStringLiteral("/component/kwin"), &action, QDBusConnection::ExportAllSlots));
        window.setProperty("internalId", QUuid::createUuid());
        window.setProperty("resizeable", true);
    }
    void identifiesOwnerAndRejectsInvalidTargets() {
        const auto requests = action.requests.size();
        TestDecoration valid(&window, &bridge);
        QVERIFY(Material::canTileWindow(&valid));
        QVERIFY(!Material::canTileWindow(nullptr));
        TestBridge unknownBridge;
        TestDecoration unknown(nullptr, &unknownBridge);
        QVERIFY(!Material::canTileWindow(&unknown));
        QVERIFY(!Material::tileWindow(&valid, Qt::LeftEdge | Qt::RightEdge));
        QVERIFY(!Material::tileWindow(&unknown, Qt::LeftEdge));
        // The public capability takes precedence over the owner's property.
        bridge.client->setResizeable(false);
        QVERIFY(!Material::canTileWindow(&valid));
        QVERIFY(!Material::tileWindow(&valid, Qt::LeftEdge));
        bridge.client->setResizeable(true);
        window.setProperty("specialWindow", true);
        QVERIFY(!Material::canTileWindow(&valid));
        window.setProperty("specialWindow", false);
        window.setProperty("resizeable", false);
        QVERIFY(Material::canTileWindow(&valid));
        bool finished = false;
        Material::fetchTilingLayout(&valid, [&](bool success, const Material::CustomTilingLayout &) {
            QVERIFY(success);
            finished = true;
        });
        QTRY_VERIFY(finished);
        QCOMPARE(action.requests.size(), requests + 1);
        QCOMPARE(action.requests.last().value(QStringLiteral("operation")).toString(), QStringLiteral("ping"));
    }
    void rejectsQueuedRequestAfterCapabilityLoss() {
        TestDecoration valid(&window, &bridge);
        QObject otherWindow;
        otherWindow.setProperty("internalId", QUuid::createUuid());
        TestBridge otherBridge;
        TestDecoration other(&otherWindow, &otherBridge);
        const auto requests = action.requests.size();
        QList<bool> results;
        for (int i = 0; i < 2; ++i) {
            Material::fetchTilingLayout(&valid, [&](bool success, const Material::CustomTilingLayout &) {
                results.append(success);
            });
        }
        bool customFinished = false;
        bool customSucceeded = true;
        QVERIFY(Material::tileCustomWindow(&valid, QStringLiteral("snapshot:0"), [&](bool success) {
            customSucceeded = success;
            customFinished = true;
        }));
        QVERIFY(Material::tileWindow(&valid, Qt::LeftEdge));
        bool otherFinished = false;
        Material::fetchTilingLayout(&other, [&](bool success, const Material::CustomTilingLayout &) {
            QVERIFY(success);
            otherFinished = true;
        });
        // Only the first request has been dispatched; the others are queued.
        bridge.client->setResizeable(false);
        QTRY_COMPARE(results.size(), 2);
        QVERIFY(results[0]);
        QVERIFY(!results[1]);
        QTRY_VERIFY(customFinished);
        QVERIFY(!customSucceeded);
        QTRY_VERIFY(otherFinished);
        QCOMPARE(action.requests.size(), requests + 2);
        QCOMPARE(action.requests.last().value(QStringLiteral("uuid")).toString(),
                 otherWindow.property("internalId").toUuid().toString(QUuid::WithoutBraces));
    }
    void correlatesAndSerializesRequests() {
        TestDecoration valid(&window, &bridge);
        action.requests.clear();
        int replies = 0;
        bool allSucceeded = true;
        for (int i = 0; i < 3; ++i) {
            Material::fetchTilingLayout(&valid, [&](bool success, const Material::CustomTilingLayout &) { ++replies; allSucceeded &= success; });
        }
        QTRY_COMPARE(replies, 3);
        QVERIFY(allSucceeded);
        QCOMPARE(action.requests.size(), 3);
        QSet<QString> ids;
        for (const auto &request : action.requests) {
            ids.insert(request.value(QStringLiteral("id")).toString());
            QCOMPARE(request.value(QStringLiteral("uuid")).toString(), window.property("internalId").toUuid().toString(QUuid::WithoutBraces));
        }
        QCOMPARE(ids.size(), 3);
    }
    void readsLayoutAndSelectsCustomTile() {
        TestDecoration valid(&window, &bridge);
        action.layoutJson = QStringLiteral(R"([{"id":"snapshot:0","x":0,"y":0,"width":0.3333333333333333,"height":1}])");
        bool finished = false;
        Material::CustomTilingLayout layout;
        Material::fetchTilingLayout(&valid, [&](bool success, const Material::CustomTilingLayout &result) {
            QVERIFY(success);
            layout = result;
            finished = true;
        });
        QTRY_VERIFY(finished);
        QCOMPARE(layout.size(), 1);
        QCOMPARE(layout[0].id, QStringLiteral("snapshot:0"));
        QCOMPARE(layout[0].rect, QRectF(0, 0, 1.0 / 3, 1));
        finished = false;
        QVERIFY(Material::tileCustomWindow(&valid, layout[0].id, [&](bool success) { QVERIFY(success); finished = true; }));
        QTRY_VERIFY(finished);
        QCOMPARE(action.requests.last().value(QStringLiteral("operation")).toString(), QStringLiteral("tileCustom"));
        QCOMPARE(action.requests.last().value(QStringLiteral("tileId")).toString(), layout[0].id);
        action.layoutJson = QStringLiteral("[]");
    }
    void emptyCustomTileReportsFailureWithoutDispatch() {
        TestDecoration valid(&window, &bridge);
        const auto requests = action.requests.size();
        int callbacks = 0;
        bool succeeded = true;
        QVERIFY(!Material::tileCustomWindow(&valid, {}, [&](bool success) {
            ++callbacks;
            succeeded = success;
        }));
        QCOMPARE(callbacks, 0);
        QTRY_COMPARE(callbacks, 1);
        QVERIFY(!succeeded);
        QCoreApplication::processEvents();
        QCOMPARE(callbacks, 1);
        QCOMPARE(action.requests.size(), requests);

        auto *decoration = new TestDecoration(&window, &bridge);
        bool called = false;
        QVERIFY(!Material::tileCustomWindow(decoration, {}, [&](bool) { called = true; }));
        delete decoration;
        QCoreApplication::processEvents();
        QVERIFY(!called);
        QCOMPARE(action.requests.size(), requests);
    }
    void ignoresMalformedLayoutAndSupportsOlderScript() {
        TestDecoration valid(&window, &bridge);
        for (const auto &json : {QStringLiteral("not json"),
             QStringLiteral(R"([{"id":"missing-coordinates","width":0.5,"height":1}])"),
             QStringLiteral(R"([{"id":"bad","x":0,"y":0,"width":2,"height":1}])"),
             QStringLiteral(R"([{"id":"same","x":0,"y":0,"width":0.5,"height":1},{"id":"same","x":0.5,"y":0,"width":0.5,"height":1}])")}) {
            action.layoutJson = json;
            bool finished = false;
            Material::fetchTilingLayout(&valid, [&](bool success, const Material::CustomTilingLayout &layout) {
                QVERIFY(success);
                QVERIFY(layout.isEmpty());
                finished = true;
            });
            QTRY_VERIFY(finished);
        }
        action.legacyReply = true;
        bool finished = false;
        Material::fetchTilingLayout(&valid, [&](bool success, const Material::CustomTilingLayout &layout) {
            QVERIFY(success);
            QVERIFY(layout.isEmpty());
            finished = true;
        });
        QTRY_VERIFY(finished);
        action.legacyReply = false;
        action.layoutJson = QStringLiteral("[]");
    }
    void timesOutAndRecovers() {
        TestDecoration valid(&window, &bridge);
        action.respond = false;
        bool finished = false;
        bool succeeded = true;
        Material::fetchTilingLayout(&valid, [&](bool result, const Material::CustomTilingLayout &) { succeeded = result; finished = true; });
        QTRY_VERIFY_WITH_TIMEOUT(finished, 3000);
        QVERIFY(!succeeded);
        action.respond = true;
        finished = false;
        Material::fetchTilingLayout(&valid, [&](bool result, const Material::CustomTilingLayout &) { succeeded = result; finished = true; });
        QTRY_VERIFY(finished);
        QVERIFY(succeeded);
    }
    void deletedDecorationDoesNotReceiveCallback() {
        auto *decoration = new TestDecoration(&window, &bridge);
        bool called = false;
        Material::fetchTilingLayout(decoration, [&](bool, const Material::CustomTilingLayout &) { called = true; });
        delete decoration;
        QTest::qWait(100);
        QVERIFY(!called);
    }
};
QTEST_MAIN(TilingControllerTest)
#include "TilingControllerTest.moc"
