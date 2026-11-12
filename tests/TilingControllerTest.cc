// SPDX-License-Identifier: GPL-2.0-or-later
#include "TilingController.h"
#include "DecorationTestBackend.h"
#include <KDecoration3/Decoration>
#include <QDBusConnection>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTest>
#include <QUuid>

class TestDecoration : public KDecoration3::Decoration {
public:
    TestDecoration(QObject *owner, MaterialTest::Bridge *bridge)
        : Decoration(owner, {QVariantMap{{QStringLiteral("bridge"),
            QVariant::fromValue<KDecoration3::DecorationBridge *>(bridge)}}}) {}
    using KDecoration3::Decoration::setTitleBar;
    bool init() override { return true; }
    void paint(QPainter *, const QRectF &) override {}
};

class DecorationDestroyer : public QObject {
    Q_OBJECT
public:
    QPointer<TestDecoration> decoration;
    Q_INVOKABLE void destroyDecoration() { delete decoration.data(); }
};

class TilingControllerTest : public QObject {
    Q_OBJECT
    std::unique_ptr<QObject> scripting;
    QJSEngine *engine = nullptr;
    QObject window;
    MaterialTest::Bridge bridge;
    void evaluate(const QString &code) {
        const auto result = engine->evaluate(code);
        QVERIFY2(!result.isError(), qPrintable(result.toString()));
    }
private slots:
    void init() {
        scripting = std::make_unique<QObject>();
        QVERIFY(QDBusConnection::sessionBus().registerObject("/Scripting", scripting.get()));
        auto *unrelated = new QJSEngine(scripting.get());
        unrelated->evaluate("var materialTiling = null;");
        engine = new QJSEngine(scripting.get());
        evaluate("var requests = [], layout = '[]'; function materialTiling(window, json) { ownerName = window.objectName; var request = JSON.parse(json); requests.push(request); return request.operation === 'layout' ? layout : true; }");
        window.setObjectName("target-window");
        window.setProperty("internalId", QUuid::createUuid());
        window.setProperty("specialWindow", false);
    }
    void cleanup() {
        QDBusConnection::sessionBus().unregisterObject("/Scripting");
        scripting.reset();
    }
    void identifiesOwnerAndRejectsInvalidTargets() {
        TestDecoration valid(&window, &bridge);
        QVERIFY(Material::tilingScriptAvailable());
        QVERIFY(Material::canTileWindow(&valid));
        QVERIFY(!Material::canTileWindow(nullptr));
        MaterialTest::Bridge unknownBridge;
        TestDecoration unknown(nullptr, &unknownBridge);
        QVERIFY(!Material::canTileWindow(&unknown));
        QVERIFY(!Material::tileWindow(&valid, Qt::LeftEdge | Qt::RightEdge));
        QVERIFY(!Material::tileWindow(&unknown, Qt::LeftEdge));
        bridge.client->setResizeable(false);
        QVERIFY(!Material::tileWindow(&valid, Qt::LeftEdge));
        bridge.client->setResizeable(true);
        window.setProperty("specialWindow", true);
        QVERIFY(!Material::tileWindow(&valid, Qt::LeftEdge));
        window.setProperty("specialWindow", false);
        QVERIFY(!Material::tileCustomWindow(&valid, {}));
        QCOMPARE(engine->evaluate("requests.length").toInt(), 0);
    }
    void returnsTilingResultsImmediately() {
        TestDecoration valid(&window, &bridge);
        for (int i = 0; i < 12; ++i) QVERIFY(Material::tileWindow(&valid, Qt::LeftEdge));
        QCOMPARE(engine->evaluate("requests.length").toInt(), 12);
        QCOMPARE(engine->evaluate("requests[0].edges").toInt(), 2);
        QCOMPARE(engine->globalObject().property("ownerName").toString(), "target-window");
        QVERIFY(Material::tileCustomWindow(&valid, "snapshot:0"));
        QCOMPARE(engine->evaluate("requests[12].tileId").toString(), "snapshot:0");
        evaluate("materialTiling = function() { return false; };");
        QVERIFY(!Material::tileWindow(&valid, Qt::RightEdge));
        evaluate("materialTiling = function() { throw new Error('layout changed'); };");
        QVERIFY(!Material::tileCustomWindow(&valid, "stale:0"));
        evaluate("materialTiling = function() { return 'true'; };");
        QVERIFY(!Material::tileWindow(&valid, Qt::RightEdge));
    }
    void validatesLayoutWithoutPartialResults() {
        TestDecoration valid(&window, &bridge);
        Material::CustomTilingLayout layout;
        engine->globalObject().setProperty("layout", R"([{"id":"snapshot:0","x":0,"y":0,"width":0.4,"height":1}])");
        QVERIFY(Material::fetchTilingLayout(&valid, layout));
        QCOMPARE(layout.size(), 1);
        QCOMPARE(layout[0].rect, QRectF(0, 0, 0.4, 1));
        QCOMPARE(layout[0].id, "snapshot:0");
        for (const auto &json : {QStringLiteral("not json"), QStringLiteral("{}"),
             QStringLiteral(R"([{"id":"missing","width":0.5,"height":1}])"),
             QStringLiteral(R"([{"id":"bad","x":0,"y":0,"width":2,"height":1}])"),
             QStringLiteral(R"([{"id":"same","x":0,"y":0,"width":0.5,"height":1},{"id":"same","x":0.5,"y":0,"width":0.5,"height":1}])")}) {
            engine->globalObject().setProperty("layout", json);
            QVERIFY(!Material::fetchTilingLayout(&valid, layout));
            QVERIFY(layout.isEmpty());
        }
        engine->globalObject().setProperty("layout", "[]");
        QVERIFY(Material::fetchTilingLayout(&valid, layout));
        QVERIFY(layout.isEmpty());
    }
    void handlesMissingAndReloadedScript() {
        TestDecoration valid(&window, &bridge);
        evaluate("materialTiling = undefined;");
        QVERIFY(!Material::tilingScriptAvailable());
        QVERIFY(!Material::tileWindow(&valid, Qt::LeftEdge));
        Material::CustomTilingLayout layout;
        QVERIFY(!Material::fetchTilingLayout(&valid, layout));
        evaluate("materialTiling = function() { return true; };");
        QVERIFY(Material::tileWindow(&valid, Qt::LeftEdge));
        QDBusConnection::sessionBus().unregisterObject("/Scripting");
        QVERIFY(!Material::tilingScriptAvailable());
        QVERIFY(!Material::tileWindow(&valid, Qt::LeftEdge));
    }
    void restoresFixedSizeWindowsSynchronously() {
        TestDecoration valid(&window, &bridge);
        valid.setTitleBar(QRectF(0, 0, 640, 32.5));
        bridge.client->setResizeable(false);
        const QVariantMap state{{"version", 3}, {"desktopIds", QStringList{"desktop-1"}}};
        QCOMPARE(Material::restoreWindowStateImmediately(&valid, state), Material::WindowRestoreResult::Unavailable);
        evaluate("function materialRestoreWindowState(window, json) { restored = JSON.parse(json); ownerName = window.objectName; return true; }");
        QCOMPARE(Material::restoreWindowStateImmediately(&valid, state), Material::WindowRestoreResult::Succeeded);
        QCOMPARE(engine->globalObject().property("ownerName").toString(), "target-window");
        QCOMPARE(engine->evaluate("restored.titleBarHeight").toNumber(), 32.5);
        QCOMPARE(engine->evaluate("restored.desktopIds[0]").toString(), "desktop-1");
        QVERIFY(!state.contains("titleBarHeight"));
        for (const auto &body : {QStringLiteral("return false;"), QStringLiteral("return undefined;"), QStringLiteral("throw new Error('invalid state');")}) {
            evaluate("materialRestoreWindowState = function() { " + body + " }; ");
            QCOMPARE(Material::restoreWindowStateImmediately(&valid, state), Material::WindowRestoreResult::Failed);
        }
        QCOMPARE(Material::prepareWindowStateImmediately(&valid, state), Material::WindowRestoreResult::Unavailable);
        evaluate("function materialPrepareWindowState(window, json) { return true; }");
        QCOMPARE(Material::prepareWindowStateImmediately(&valid, state), Material::WindowRestoreResult::Succeeded);
        for (const auto &body : {QStringLiteral("return false;"), QStringLiteral("return undefined;"), QStringLiteral("throw new Error('prepare failed');")}) {
            evaluate("materialPrepareWindowState = function() { " + body + " }; ");
            QCOMPARE(Material::prepareWindowStateImmediately(&valid, state), Material::WindowRestoreResult::Failed);
        }
        bridge.client->setResizeable(true);
    }
    void toleratesDecorationDestructionDuringTiling() {
        DecorationDestroyer destroyer;
        destroyer.decoration = new TestDecoration(&window, &bridge);
        QJSEngine::setObjectOwnership(&destroyer, QJSEngine::CppOwnership);
        engine->globalObject().setProperty("destroyer", engine->newQObject(&destroyer));
        evaluate("materialTiling = function() { destroyer.destroyDecoration(); return true; };");
        QVERIFY(Material::tileWindow(destroyer.decoration, Qt::LeftEdge));
        QVERIFY(destroyer.decoration.isNull());
    }
};
QTEST_MAIN(TilingControllerTest)
#include "TilingControllerTest.moc"
