// SPDX-License-Identifier: GPL-2.0-or-later
#include "WindowState.h"
#include "WindowStateScope.h"
#include "TilingController.h"
#include "InternalSettings.h"
#include "ExceptionList.h"
#include "SettingsProvider.h"
#include "DecorationTestBackend.h"
#include <KDecoration3/Decoration>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDBusConnection>
#include <QRectF>
#include <QScopedValueRollback>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <functional>

// Match the runtime base-class name without including any private KWin header.
namespace KWin {
class Tile : public QObject { Q_OBJECT public: using QObject::QObject; };
class QuickRootTile : public QObject { Q_OBJECT public: using QObject::QObject; };
class WaylandWindow : public QObject {
    Q_OBJECT
public:
    void close() { Q_EMIT closed(); }
    void map() { Q_EMIT readyForPaintingChanged(); }
    void changedTile() { Q_EMIT tileChanged(reinterpret_cast<KWin::Tile *>(property("tile").value<QObject *>())); }
    void startInteraction() { Q_EMIT interactiveMoveResizeStarted(); }
    void finishInteraction() { Q_EMIT interactiveMoveResizeFinished(); }
signals:
    void closed();
    void readyForPaintingChanged();
    void tileChanged(KWin::Tile *tile);
    void requestedTileChanged();
    void frameGeometryAboutToChange();
    void frameGeometryChanged();
    void maximizedAboutToChange(int mode);
    void maximizedChanged();
    void fullScreenChanged();
    void interactiveMoveResizeStarted();
    void interactiveMoveResizeFinished();
};
}

class NativeRectangle {
    Q_GADGET
    Q_PROPERTY(double x MEMBER x)
    Q_PROPERTY(double y MEMBER y)
    Q_PROPERTY(double width MEMBER width)
    Q_PROPERTY(double height MEMBER height)
public:
    double x = 0, y = 0, width = 0, height = 0;
};
Q_DECLARE_METATYPE(NativeRectangle)

class ScopeWorkspace : public QObject {
    Q_OBJECT
public:
    void removeOutput(QObject *output) { Q_EMIT outputRemoved(output); }
    void addOutput(QObject *output) { Q_EMIT outputAdded(output); }
    void finish() { Q_EMIT outputsChanged(); }
signals:
    void outputRemoved(QObject *output);
    void outputAdded(QObject *output);
    void outputsChanged();
    void desktopsChanged();
    void desktopLayoutChanged();
};

class StateDecoration : public KDecoration3::Decoration {
public:
    StateDecoration(QObject *owner, MaterialTest::Bridge *bridge)
        : Decoration(owner, {QVariantMap{{QStringLiteral("bridge"), QVariant::fromValue<KDecoration3::DecorationBridge *>(bridge)}}}) {}
    bool init() override { return true; }
    void paint(QPainter *, const QRectF &) override {}
};

static QList<QVariantMap> immediateRequests;
static bool immediateAvailable = true;
static bool immediateSucceeds = true;
static std::function<void()> immediateHook;
static QList<QVariantMap> prepareRequests;
static bool prepareAvailable = false;
static bool prepareSucceeds = true;
static int prepareCalls = 0;
static bool prepareMinimizes = false;
static QVariant preparedDesktops;
static QObject *testWorkspace = nullptr;
namespace Material {
QObject *windowStateWorkspace() { return testWorkspace; }
WindowRestoreResult prepareWindowStateImmediately(KDecoration3::Decoration *decoration, const QVariantMap &state)
{
    ++prepareCalls;
    if (!prepareAvailable) return WindowRestoreResult::Unavailable;
    prepareRequests.append(state);
    if (!prepareSucceeds) return WindowRestoreResult::Failed;
    if (prepareMinimizes) decoration->parent()->setProperty("minimized", true);
    if (preparedDesktops.isValid()) decoration->parent()->setProperty("desktops", preparedDesktops);
    return WindowRestoreResult::Succeeded;
}
WindowRestoreResult restoreWindowStateImmediately(KDecoration3::Decoration *, const QVariantMap &state)
{
    if (!immediateAvailable) return WindowRestoreResult::Unavailable;
    immediateRequests.append(state);
    if (immediateHook) immediateHook();
    return immediateSucceeds ? WindowRestoreResult::Succeeded : WindowRestoreResult::Failed;
}

}

class WindowStateTest : public QObject {
    Q_OBJECT
    void setup(KWin::WaylandWindow &window, const QString &app) {
        window.setProperty("desktopFileName", app);
        window.setProperty("normalWindow", true);
        window.setProperty("maximizeMode", 0);
        window.setProperty("frameGeometry", QVariant::fromValue(NativeRectangle{40, 60, 800, 500}));
    }
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void disabledByDefault() {
        Material::InternalSettings settings;
        settings.setDefaults();
        QVERIFY(!settings.rememberWindowState());
        auto source = Material::InternalSettingsPtr::create();
        auto copy = Material::InternalSettingsPtr::create();
        source->setRememberWindowState(true);
        Material::copyInternalSettings(source, copy);
        QVERIFY(copy->rememberWindowState());
    }
    void identifiesApplicationsAndTags() {
        KWin::WaylandWindow a, b;
        setup(a, "org.example.StateTest");
        setup(b, "org.example.StateTest");
        QCOMPARE(Material::windowStateKey(&a), Material::windowStateKey(&b));
        b.setProperty("tag", "another-window");
        QVERIFY(Material::windowStateKey(&a) != Material::windowStateKey(&b));
        b.setProperty("desktopFileName", "");
        b.setProperty("resourceClass", "fallback-app");
        QVERIFY(!Material::windowStateKey(&b).isEmpty());
        b.setProperty("resourceClass", "");
        QVERIFY(Material::windowStateKey(&b).isEmpty());
    }
    void windowPositionExceptions() {
        Material::SettingsProvider provider;
        auto defaults = Material::InternalSettingsPtr::create();
        auto exception = Material::InternalSettingsPtr::create();
        exception->setExceptionPattern("org.example.Excluded");
        exception->setMask(Material::ExceptionMask::ExcludeWindowPosition);
        for (bool global : {false, true}) {
            defaults->setRememberWindowState(global);
            for (bool excluded : {false, true}) {
                exception->setExcludeWindowPosition(excluded);
                QCOMPARE(Material::cloneInternalSettings(exception)->excludeWindowPosition(), excluded);
                QCOMPARE(provider.createMergedSettings(defaults, exception)->rememberWindowState(), global && !excluded);
            }
        }
        defaults->setRememberWindowState(true);
        exception->setMask(Material::ExceptionMask::None);
        QVERIFY(provider.createMergedSettings(defaults, exception)->rememberWindowState());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto config = KSharedConfig::openConfig(directory.filePath("exceptionsrc"), KConfig::SimpleConfig);
        exception->setMask(Material::ExceptionMask::ExcludeWindowPosition);
        Material::ExceptionList list;
        for (bool excluded : {true, false}) {
            exception->setExcludeWindowPosition(excluded);
            list.setExceptions({exception});
            list.writeConfig(config);
            Material::ExceptionList loaded;
            loaded.readConfig(config);
            QCOMPARE(loaded.exceptions().size(), 1);
            QCOMPARE(loaded.exceptions().first()->excludeWindowPosition(), excluded);
            QCOMPARE(loaded.exceptions().first()->mask(), int(Material::ExceptionMask::ExcludeWindowPosition));
        }
    }
    void capturesNativeValuesAndRoundTrips() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.RoundTrip");
        QObject output, desktop;
        output.setProperty("name", "DP-1");
        output.setProperty("uuid", "monitor-uuid");
        output.setProperty("geometry", QRect(1920, 0, 1280, 800));
        desktop.setProperty("id", "desktop-uuid");
        window.setProperty("output", QVariant::fromValue(&output));
        window.setProperty("desktops", QVariant::fromValue(QList<QObject *>{&desktop}));
        window.setProperty("maximizeMode", 3);
        window.setProperty("fullScreen", true);
        const auto state = Material::captureWindowState(&window);
        QCOMPARE(state.value("position").toMap().value("x").toDouble(), 40);
        QVERIFY(!state.contains("geometry"));
        QVERIFY(!QJsonDocument::fromVariant(state).toJson().contains("width"));
        QVERIFY(!QJsonDocument::fromVariant(state).toJson().contains("height"));
        QCOMPARE(state.value("desktopIds").toStringList(), QStringList{"desktop-uuid"});
        QCOMPARE(state.value("outputPosition").toMap().value("x").toDouble(), 1920);
        const auto key = Material::windowStateKey(&window);
        QVERIFY(Material::writeWindowState(key, state));
        const auto saved = Material::readWindowState(key);
        QCOMPARE(saved.value("outputUuid").toString(), "monitor-uuid");
        QVERIFY(!saved.contains("maximizeMode"));
        QVERIFY(!saved.contains("fullScreen"));
        QVERIFY(!Material::writeWindowState(key, {{"version", 3}, {"position", QVariantMap{{"x", 1}}}}));
        QCOMPARE(Material::readWindowState(key), saved);
    }
    void rejectsUnsupportedStateVersions() {
        const QString key = "unsupported-" + QUuid::createUuid().toString();
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.Unsupported");
        const auto current = Material::captureWindowState(&window);
        auto config = KSharedConfig::openStateConfig(QStringLiteral("material-window-staterc"));
        KConfigGroup group(config, key);
        for (int version : {1, 2, 4}) {
            auto state = current;
            state.insert("version", version);
            group.writeEntry("State", QJsonDocument::fromVariant(state).toJson(QJsonDocument::Compact));
            QVERIFY(config->sync());
            QVERIFY(Material::readWindowState(key).isEmpty());
            QVERIFY(!Material::writeWindowState(key, state));
        }
    }
    void capturesNativeTileBindings() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.Tile");
        KWin::QuickRootTile quickRoot;
        QObject quick;
        quick.setProperty("parent", QVariant::fromValue<QObject *>(&quickRoot));
        quick.setProperty("relativeGeometry", QRectF(0, 0, 0.4, 1));
        window.setProperty("tile", QVariant::fromValue(&quick));
        auto state = Material::captureWindowState(&window);
        QCOMPARE(state.value("tile").toMap().value("kind").toString(), "quick");
        QCOMPARE(state.value("tile").toMap().value("edges").toInt(), 2);
        const auto key = Material::windowStateKey(&window);
        QVERIFY(Material::writeWindowState(key, state));
        QCOMPARE(Material::readWindowState(key).value("tile"), state.value("tile"));
        QObject root, first, custom;
        root.setProperty("tiles", QVariant::fromValue(QList<QObject *>{&first, &custom}));
        custom.setProperty("parent", QVariant::fromValue(&root));
        custom.setProperty("relativeGeometry", QRectF(0.4, 0, 0.6, 1));
        window.setProperty("tile", QVariant::fromValue(&custom));
        state = Material::captureWindowState(&window);
        QCOMPARE(state.value("tile").toMap().value("kind").toString(), "custom");
        QCOMPARE(state.value("tile").toMap().value("path").toList().first().toInt(), 1);
        QVERIFY(Material::writeWindowState(key, state));
        QCOMPARE(Material::readWindowState(key).value("tile"), state.value("tile"));
    }
    void keepsTileBindingWhenCloseCleanupClearsTheProperty() {
        for (bool unsnap : {false, true}) {
            KWin::WaylandWindow window;
            setup(window, "org.example.StateTest.CloseTile." + QUuid::createUuid().toString());
            MaterialTest::Bridge bridge;
            StateDecoration decoration(&window, &bridge);
            Material::configureWindowState(&decoration, true);
            window.map();
            KWin::QuickRootTile root;
            QObject tile;
            tile.setProperty("parent", QVariant::fromValue<QObject *>(&root));
            tile.setProperty("relativeGeometry", QRectF(0, 0, 0.5, 1));
            window.setProperty("tile", QVariant::fromValue(&tile));
            window.changedTile();
            window.setProperty("tile", QVariant());
            if (unsnap) window.changedTile();
            window.setProperty("deleted", true);
            window.close();
            QCOMPARE(Material::readWindowState(Material::windowStateKey(&window)).contains("tile"), !unsnap);
        }
    }
    void savesNormalSizeOnlyWithValidTileMetadata() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.NormalSizeValidation." + QUuid::createUuid().toString());
        auto state = Material::captureWindowState(&window);
        const auto key = Material::windowStateKey(&window);
        const QVariantMap size{{"width", 900.5}, {"height", 650.25}};
        state.insert("tile", QVariantMap{{"kind", "quick"}, {"edges", 2}, {"normalSize", size}});
        QVERIFY(Material::writeWindowState(key, state));
        QCOMPARE(Material::readWindowState(key).value("tile").toMap().value("normalSize").toMap(), size);
        for (const auto &bad : {QVariantMap{{"width", 0}, {"height", 500}}, QVariantMap{{"width", -1}, {"height", 500}}, QVariantMap{{"width", 900}}}) {
            state.insert("tile", QVariantMap{{"kind", "quick"}, {"edges", 2}, {"normalSize", bad}});
            QVERIFY(Material::writeWindowState(key, state));
            QVERIFY(!Material::readWindowState(key).value("tile").toMap().contains("normalSize"));
        }
        state.insert("tile", QVariantMap{{"kind", "quick"}, {"edges", 6}, {"normalSize", size}});
        QVERIFY(Material::writeWindowState(key, state));
        QVERIFY(!Material::readWindowState(key).contains("tile"));
    }
    void remembersNormalSizeAcrossTilingAndMaximizing_data() {
        QTest::addColumn<bool>("maximizeFirst");
        QTest::newRow("normal-to-tile") << false;
        QTest::newRow("maximize-to-tile") << true;
    }
    void remembersNormalSizeAcrossTilingAndMaximizing() {
        QFETCH(bool, maximizeFirst);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.NormalSize." + QUuid::createUuid().toString());
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true);
        window.map();
        window.setProperty("frameGeometry", QRectF(100, 100, 900, 650));
        Q_EMIT window.frameGeometryChanged();
        QCoreApplication::processEvents();
        if (maximizeFirst) {
            Q_EMIT window.maximizedAboutToChange(3);
            Q_EMIT window.frameGeometryAboutToChange();
            window.setProperty("frameGeometry", QRectF(0, 0, 1280, 800));
            Q_EMIT window.frameGeometryChanged();
            window.setProperty("maximizeMode", 3);
            Q_EMIT window.maximizedChanged();
            QCoreApplication::processEvents();
            Q_EMIT window.maximizedAboutToChange(0);
        }
        KWin::QuickRootTile root;
        KWin::Tile tile;
        tile.setProperty("parent", QVariant::fromValue<QObject *>(&root));
        tile.setProperty("relativeGeometry", QRectF(0, 0, 0.5, 1));
        window.setProperty("tile", QVariant::fromValue<QObject *>(&tile));
        Q_EMIT window.frameGeometryAboutToChange();
        Q_EMIT window.requestedTileChanged();
        window.setProperty("frameGeometry", QRectF(0, 0, 640, 800));
        Q_EMIT window.frameGeometryChanged();
        if (maximizeFirst) {
            window.setProperty("maximizeMode", 0);
            Q_EMIT window.maximizedChanged();
        }
        window.changedTile();
        QCoreApplication::processEvents();
        window.close();
        auto state = Material::readWindowState(Material::windowStateKey(&window));
        QCOMPARE(state.value("tile").toMap().value("normalSize").toMap(), (QVariantMap{{"width", 900}, {"height", 650}}));
        // A tile-to-tile resize must preserve the original normal dimensions.
        window.setProperty("frameGeometry", QRectF(0, 0, 450, 800));
        Q_EMIT window.frameGeometryChanged();
        window.changedTile();
        QCoreApplication::processEvents();
        window.close();
        QCOMPARE(Material::readWindowState(Material::windowStateKey(&window)).value("tile"), state.value("tile"));
        // After unsnapping, normal sizes can change again and floating saves
        // must never acquire dimensions from this observer.
        window.setProperty("tile", QVariant::fromValue<QObject *>(nullptr));
        Q_EMIT window.frameGeometryAboutToChange();
        Q_EMIT window.requestedTileChanged();
        QCoreApplication::processEvents(); // Still tiled until the client commits.
        window.setProperty("frameGeometry", QRectF(150, 150, 900, 650));
        Q_EMIT window.frameGeometryChanged();
        window.changedTile();
        QCoreApplication::processEvents();
        window.close();
        state = Material::readWindowState(Material::windowStateKey(&window));
        QVERIFY(!state.contains("tile"));
        QVERIFY(!state.contains("normalSize"));
        QVERIFY(!state.contains("width"));
    }
    void preservesStoredNormalSizeDuringRestore() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.SeededNormalSize." + QUuid::createUuid().toString());
        window.setProperty("frameGeometry", QRectF(0, 0, 640, 800));
        const QVariantMap size{{"width", 900}, {"height", 650}};
        auto state = Material::captureWindowState(&window);
        state.insert("tile", QVariantMap{{"kind", "quick"}, {"edges", 2}, {"normalSize", size}});
        const auto key = Material::windowStateKey(&window);
        QVERIFY(Material::writeWindowState(key, state));
        KWin::QuickRootTile root;
        KWin::Tile tile;
        tile.setProperty("parent", QVariant::fromValue<QObject *>(&root));
        tile.setProperty("relativeGeometry", QRectF(0, 0, 0.5, 1));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        immediateAvailable = true;
        immediateHook = [&] {
            // Simulate the synchronous request followed by its eventual commit.
            Q_EMIT window.frameGeometryAboutToChange();
            window.setProperty("frameGeometry", QRectF(0, 0, 900, 650));
            Q_EMIT window.frameGeometryChanged();
            window.setProperty("tile", QVariant::fromValue<QObject *>(&tile));
            Q_EMIT window.frameGeometryAboutToChange();
            Q_EMIT window.requestedTileChanged();
            window.setProperty("frameGeometry", QRectF(0, 0, 640, 800));
            Q_EMIT window.frameGeometryChanged();
            window.changedTile();
        };
        Material::configureWindowState(&decoration, true);
        window.map();
        immediateHook = {};
        QCoreApplication::processEvents();
        window.close();
        QCOMPARE(Material::readWindowState(key).value("tile").toMap().value("normalSize").toMap(), size);
    }
    void newSiblingSizeHint_data() {
        QTest::addColumn<QString>("scenario");
        for (const auto *scenario : {"matching", "different-size", "different-identity", "excluded-source", "special-target", "fixed-target", "uncommitted-source", "ambiguous-source"})
            QTest::newRow(scenario) << QString::fromLatin1(scenario);
    }
    void newSiblingSizeHint() {
        QFETCH(QString, scenario);
        QObject workspace;
        QScopedValueRollback restoreWorkspace(testWorkspace, &workspace);
        KWin::WaylandWindow source, target;
        const auto app = "org.example.StateTest.Sibling." + QUuid::createUuid().toString();
        setup(source, app);
        setup(target, app);
        source.setProperty("resizeable", true);
        target.setProperty("resizeable", scenario != "fixed-target");
        workspace.setProperty("stackingOrder", QVariant::fromValue(QList<QObject *>{&source, &target}));
        MaterialTest::Bridge sourceBridge, targetBridge;
        StateDecoration sourceDecoration(&source, &sourceBridge), targetDecoration(&target, &targetBridge);
        Material::configureWindowState(&sourceDecoration, true);
        source.map();
        KWin::QuickRootTile root;
        KWin::Tile tile;
        tile.setProperty("parent", QVariant::fromValue<QObject *>(&root));
        tile.setProperty("relativeGeometry", QRectF(0, 0, 0.5, 1));
        source.setProperty("tile", QVariant::fromValue<QObject *>(&tile));
        Q_EMIT source.frameGeometryAboutToChange();
        Q_EMIT source.requestedTileChanged();
        source.setProperty("frameGeometry", QRectF(0, 0, 640, 800));
        Q_EMIT source.frameGeometryChanged();
        if (scenario != "uncommitted-source") source.changedTile();
        QCoreApplication::processEvents();
        target.setProperty("frameGeometry", QRectF(100, 100, scenario == "different-size" ? 700 : 640, 800));
        if (scenario == "different-identity") target.setProperty("tag", "another-window");
        if (scenario == "excluded-source") Material::configureWindowState(&sourceDecoration, false);
        if (scenario == "special-target") target.setProperty("maximizeMode", 3);
        KWin::WaylandWindow extra;
        MaterialTest::Bridge extraBridge;
        StateDecoration extraDecoration(&extra, &extraBridge);
        if (scenario == "ambiguous-source") {
            setup(extra, app);
            extra.setProperty("resizeable", true);
            extra.setProperty("frameGeometry", QRectF(100, 100, 1000, 700));
            Material::configureWindowState(&extraDecoration, true);
            extra.map();
            extra.setProperty("tile", QVariant::fromValue<QObject *>(&tile));
            Q_EMIT extra.frameGeometryAboutToChange();
            Q_EMIT extra.requestedTileChanged();
            extra.setProperty("frameGeometry", QRectF(0, 0, 640, 800));
            Q_EMIT extra.frameGeometryChanged();
            extra.changedTile();
            QCoreApplication::processEvents();
            workspace.setProperty("stackingOrder", QVariant::fromValue(QList<QObject *>{&source, &extra, &target}));
        }
        immediateAvailable = true;
        const auto count = immediateRequests.size();
        Material::configureWindowState(&targetDecoration, true);
        target.map();
        if (scenario == "matching") {
            QCOMPARE(immediateRequests.size(), count + 1);
            const auto hint = immediateRequests.last().value("inheritedTileSize").toMap();
            QCOMPARE(hint.value("normalSize").toMap(), (QVariantMap{{"width", 800}, {"height", 500}}));
            QCOMPARE(hint.value("tileSize").toMap(), (QVariantMap{{"width", 640}, {"height", 800}}));
            // A live sibling can provide this information before any close/save.
            QVERIFY(Material::readWindowState(Material::windowStateKey(&source)).isEmpty());
        } else {
            QCOMPARE(immediateRequests.size(), count);
        }
    }
    void savesOnlyOnCloseAndSurvivesDecorationRemoval() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.Close." + QUuid::createUuid().toString());
        MaterialTest::Bridge bridge;
        auto *decoration = new StateDecoration(&window, &bridge);
        Material::configureWindowState(decoration, true);
        const auto key = Material::windowStateKey(&window);
        QCoreApplication::processEvents();
        QVERIFY(Material::readWindowState(key).isEmpty());
        window.setProperty("frameGeometry", QVariant::fromValue(NativeRectangle{120, 150, 900, 700}));
        QVERIFY(Material::readWindowState(key).isEmpty());
        delete decoration;
        window.close();
        // Direct connection: no event loop iteration is needed to persist state.
        QCOMPARE(Material::readWindowState(key).value("position").toMap().value("x").toDouble(), 120);
    }
    void disabledNeverRestoresOrWrites() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.Disabled." + QUuid::createUuid().toString());
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        const auto key = Material::windowStateKey(&window);
        QVERIFY(Material::writeWindowState(key, Material::captureWindowState(&window)));
        const auto saved = Material::readWindowState(key);
        const auto count = immediateRequests.size();
        Material::configureWindowState(&decoration, false);
        window.map();
        QCoreApplication::processEvents();
        QCOMPARE(immediateRequests.size(), count);
        window.setProperty("frameGeometry", QRectF(200, 200, 1000, 700));
        window.close();
        QCOMPARE(Material::readWindowState(key), saved);
    }
    void restoresOnceAndRespectsDisabling() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.Restore." + QUuid::createUuid().toString());
        const auto key = Material::windowStateKey(&window);
        QVERIFY(Material::writeWindowState(key, Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        const auto count = immediateRequests.size();
        Material::configureWindowState(&decoration, true);
        QCoreApplication::processEvents();
        QCOMPARE(immediateRequests.size(), count); // Wait for the first committed buffer.
        window.map();
        QTRY_COMPARE(immediateRequests.size(), count + 1);
        Material::configureWindowState(&decoration, true);
        QCoreApplication::processEvents();
        QCOMPARE(immediateRequests.size(), count + 1);
        const auto saved = Material::readWindowState(key);
        Material::configureWindowState(&decoration, false);
        window.setProperty("frameGeometry", QRectF(200, 200, 1000, 700));
        window.close();
        QCOMPARE(Material::readWindowState(key), saved);
    }
    void retriesWhenSynchronousScriptBecomesAvailable() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.Retry." + QUuid::createUuid().toString());
        QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        const auto count = immediateRequests.size();
        immediateAvailable = false;
        Material::configureWindowState(&decoration, true);
        window.map();
        QCOMPARE(immediateRequests.size(), count);
        immediateAvailable = true;
        QTRY_COMPARE(immediateRequests.size(), count + 1);
    }
    void userInteractionCancelsPendingRestore_data() {
        QTest::addColumn<QRectF>("geometry");
        QTest::newRow("move") << QRectF(200, 150, 800, 500);
        QTest::newRow("resize") << QRectF(40, 60, 1000, 700);
    }
    void userInteractionCancelsPendingRestore() {
        QFETCH(QRectF, geometry);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.Cancel." + QUuid::createUuid().toString());
        const auto key = Material::windowStateKey(&window);
        QVERIFY(Material::writeWindowState(key, Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        const auto count = immediateRequests.size();
        immediateAvailable = false;
        Material::configureWindowState(&decoration, true);
        window.map();
        window.startInteraction();
        window.setProperty("frameGeometry", geometry);
        window.finishInteraction();
        immediateAvailable = true;
        QTest::qWait(600);
        QCOMPARE(immediateRequests.size(), count);
        QCOMPARE(window.property("frameGeometry").toRectF(), geometry);
        // Cancellation affects only restoration, not saving the user's choice.
        window.close();
        const auto saved = Material::readWindowState(key).value("position").toMap();
        QCOMPARE(saved.value("x").toDouble(), geometry.x());
        QCOMPARE(saved.value("y").toDouble(), geometry.y());
    }
    void applicationGeometryChangeDoesNotCancelPendingRestore() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.AppMove." + QUuid::createUuid().toString());
        QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        const auto count = immediateRequests.size();
        immediateAvailable = false;
        Material::configureWindowState(&decoration, true);
        window.map();
        window.setProperty("frameGeometry", QRectF(100, 100, 900, 600));
        immediateAvailable = true;
        QTRY_COMPARE(immediateRequests.size(), count + 1);
    }
    void doesNotRetryExecutionFailure() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.Failure." + QUuid::createUuid().toString());
        QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        const auto count = immediateRequests.size();
        immediateAvailable = true;
        immediateSucceeds = false;
        Material::configureWindowState(&decoration, true);
        window.map();
        immediateSucceeds = true;
        QCOMPARE(immediateRequests.size(), count + 1);
        QTest::qWait(600);
        QCOMPARE(immediateRequests.size(), count + 1);
    }
    void preservesPreviousStateWhenRestoreFails_data() {
        QTest::addColumn<bool>("available");
        QTest::addColumn<bool>("manual");
        QTest::addColumn<bool>("snapshot");
        for (bool available : {false, true}) {
            for (bool manual : {false, true}) {
                for (bool snapshot : {false, true}) {
                    const auto name = QStringLiteral("%1-%2-%3").arg(available ? "execution-failed" : "unavailable",
                        manual ? "user-moved" : "untouched", snapshot ? "snapshot" : "close");
                    QTest::newRow(qPrintable(name)) << available << manual << snapshot;
                }
            }
        }
    }
    void preservesPreviousStateWhenRestoreFails() {
        QFETCH(bool, available);
        QFETCH(bool, manual);
        QFETCH(bool, snapshot);
        QScopedValueRollback restoreAvailability(immediateAvailable, available);
        QScopedValueRollback restoreSuccess(immediateSucceeds, false);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.ProtectSaved." + QUuid::createUuid().toString());
        const auto key = Material::windowStateKey(&window);
        const auto scope = Material::WindowStateScope::self()->current();
        QVERIFY(Material::writeWindowState(key, Material::captureWindowState(&window), scope));
        const auto saved = Material::readWindowState(key, scope);
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true);
        // KWin/app startup placement is not a manual user decision.
        window.setProperty("frameGeometry", QRectF(300, 200, 800, 500));
        window.map();
        if (manual) {
            window.startInteraction();
            window.setProperty("frameGeometry", QRectF(500, 400, 900, 650));
            window.finishInteraction();
        }
        if (snapshot) Q_EMIT Material::WindowStateScope::self()->snapshotRequested(scope, {});
        else window.close();
        if (manual) {
            QCOMPARE(Material::readWindowState(key, scope).value("position").toMap().value("x").toInt(), 500);
            QCOMPARE(Material::readWindowState(key, scope).value("position").toMap().value("y").toInt(), 400);
        } else {
            QCOMPARE(Material::readWindowState(key, scope), saved);
        }
    }
    void newWindowStillSavesWithoutScript() {
        QScopedValueRollback restoreAvailability(immediateAvailable, false);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.NewWithoutScript." + QUuid::createUuid().toString());
        const auto key = Material::windowStateKey(&window);
        const auto scope = Material::WindowStateScope::self()->current();
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true);
        window.map();
        Q_EMIT Material::WindowStateScope::self()->snapshotRequested(scope, {});
        QVERIFY(!Material::readWindowState(key, scope).isEmpty());
        // A snapshot created by this same new window must not prevent its
        // later close from recording the final placement.
        window.setProperty("frameGeometry", QRectF(300, 200, 800, 500));
        window.close();
        QCOMPARE(Material::readWindowState(key, scope).value("position").toMap().value("x").toInt(), 300);
    }
    void successfulRestoreStillSavesCurrentState() {
        QScopedValueRollback restoreAvailability(immediateAvailable, true);
        QScopedValueRollback restoreSuccess(immediateSucceeds, true);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.SaveAfterRestore." + QUuid::createUuid().toString());
        const auto key = Material::windowStateKey(&window);
        const auto scope = Material::WindowStateScope::self()->current();
        QVERIFY(Material::writeWindowState(key, Material::captureWindowState(&window), scope));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true);
        window.map();
        window.setProperty("frameGeometry", QRectF(600, 450, 800, 500));
        Q_EMIT Material::WindowStateScope::self()->snapshotRequested(scope, {});
        QCOMPARE(Material::readWindowState(key, scope).value("position").toMap().value("x").toInt(), 600);
        window.setProperty("frameGeometry", QRectF(620, 470, 800, 500));
        window.close();
        QCOMPARE(Material::readWindowState(key, scope).value("position").toMap().value("x").toInt(), 620);
    }
    void restoresBeforeMappingSignalReturns() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.Immediate." + QUuid::createUuid().toString());
        QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true);
        const auto immediate = immediateRequests.size();
        immediateAvailable = true;
        window.map();
        // No processEvents/QTRY: complete before the compositor can render.
        QCOMPARE(immediateRequests.size(), immediate + 1);
        QCoreApplication::processEvents();
        QCOMPARE(immediateRequests.size(), immediate + 1);
    }
    void preparesDesktopBeforeMappingAndReleasesGuard() {
        for (bool initiallyMinimized : {false, true}) {
            KWin::WaylandWindow window;
            setup(window, "org.example.StateTest.Prepare." + QUuid::createUuid().toString());
            window.setProperty("minimized", initiallyMinimized);
            QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
            MaterialTest::Bridge bridge;
            StateDecoration decoration(&window, &bridge);
            prepareAvailable = true;
            prepareMinimizes = true;
            immediateAvailable = true;
            const auto count = prepareRequests.size();
            Material::configureWindowState(&decoration, true);
            QCOMPARE(prepareRequests.size(), count + 1); // Already prepared, no event loop.
            QVERIFY(window.property("minimized").toBool());
            window.map();
            QCOMPARE(window.property("minimized").toBool(), initiallyMinimized);
            QCoreApplication::processEvents();
            QCOMPARE(prepareRequests.size(), count + 1); // No late preparation after map.
            prepareAvailable = prepareMinimizes = immediateAvailable = false;
        }
    }
    void disablingBeforeMappingCancelsPreparation() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.CancelPrepare." + QUuid::createUuid().toString());
        QObject oldDesktop, newDesktop;
        const auto original = QVariant::fromValue(QList<QObject *>{&oldDesktop});
        window.setProperty("desktops", original);
        window.setProperty("minimized", false);
        QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        preparedDesktops = QVariant::fromValue(QList<QObject *>{&newDesktop});
        prepareAvailable = prepareMinimizes = true;
        Material::configureWindowState(&decoration, true);
        QVERIFY(window.property("minimized").toBool());
        QCOMPARE(window.property("desktops"), preparedDesktops);
        Material::configureWindowState(&decoration, false);
        QVERIFY(!window.property("minimized").toBool());
        QCOMPARE(window.property("desktops"), original);
        const auto count = immediateRequests.size();
        window.map();
        QCoreApplication::processEvents();
        QCOMPARE(immediateRequests.size(), count);
        prepareAvailable = prepareMinimizes = false;
        preparedDesktops.clear();
    }
    void retriesPreparationWhenScriptStartsBeforeMapping() {
        QScopedValueRollback available(prepareAvailable, false);
        QScopedValueRollback minimizes(prepareMinimizes, true);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.LatePrepare." + QUuid::createUuid().toString());
        window.setProperty("minimized", false);
        QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true);
        QCoreApplication::processEvents();
        const auto count = prepareCalls;
        prepareAvailable = true;
        QTRY_VERIFY_WITH_TIMEOUT(window.property("minimized").toBool(), 500);
        QCOMPARE(prepareCalls, count + 1);
        QTest::qWait(120);
        QCOMPARE(prepareCalls, count + 1);
        window.map();
        QVERIFY(!window.property("minimized").toBool());
    }
    void preparationStopsAtLifecycleBoundary() {
        for (int action = 0; action < 4; ++action) {
            QScopedValueRollback available(prepareAvailable, false);
            KWin::WaylandWindow window;
            setup(window, "org.example.StateTest.StopPrepare." + QUuid::createUuid().toString());
            QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
            MaterialTest::Bridge bridge;
            StateDecoration decoration(&window, &bridge);
            Material::configureWindowState(&decoration, true);
            QCoreApplication::processEvents();
            const auto count = prepareCalls;
            if (action == 0) window.map();
            if (action == 1) Material::configureWindowState(&decoration, false);
            if (action == 2) window.startInteraction();
            if (action == 3) window.close();
            prepareAvailable = true;
            QTest::qWait(120);
            QCOMPARE(prepareCalls, count);
        }
    }
    void preparationFailureIsNotRetried() {
        QScopedValueRollback available(prepareAvailable, true);
        QScopedValueRollback succeeds(prepareSucceeds, false);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.FailedPrepare." + QUuid::createUuid().toString());
        QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        const auto count = prepareCalls;
        Material::configureWindowState(&decoration, true);
        QTest::qWait(120);
        QCOMPARE(prepareCalls, count + 1);
    }
    void immediatePreparationIsReassertedOnce() {
        QScopedValueRollback available(prepareAvailable, true);
        QScopedValueRollback minimizes(prepareMinimizes, true);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.ReassertPrepare." + QUuid::createUuid().toString());
        window.setProperty("minimized", false);
        QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        const auto count = prepareCalls;
        Material::configureWindowState(&decoration, true);
        QCOMPARE(prepareCalls, count + 1);
        window.setProperty("minimized", false); // Simulate KWin applying initial client state.
        QTest::qWait(120);
        QCOMPARE(prepareCalls, count + 2);
        QVERIFY(window.property("minimized").toBool());
        window.map();
        QVERIFY(!window.property("minimized").toBool());
    }
    void preparationRetriesHaveABoundedBudget() {
        QScopedValueRollback available(prepareAvailable, false);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.PrepareBudget." + QUuid::createUuid().toString());
        QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        const auto count = prepareCalls;
        Material::configureWindowState(&decoration, true);
        QTRY_COMPARE_WITH_TIMEOUT(prepareCalls, count + 100, 6500);
        prepareAvailable = true;
        QTest::qWait(120);
        QCOMPARE(prepareCalls, count + 100);
    }
    void configurationIdentityAndScopedStorage() {
        ScopeWorkspace workspace;
        QObject a, b, desktop;
        a.setProperty("name", "A"); a.setProperty("geometry", QRect(0, 0, 1280, 800));
        b.setProperty("name", "B"); b.setProperty("geometry", QRect(1280, 0, 1280, 800));
        desktop.setProperty("id", "desktop-1");
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a, &b}));
        workspace.setProperty("desktops", QVariant::fromValue(QList<QObject *>{&desktop}));
        workspace.setProperty("desktopGridWidth", 1);
        workspace.setProperty("desktopGridHeight", 1);
        const auto ab = Material::windowConfigurationScope(&workspace);
        QVERIFY(!ab.isEmpty());
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&b, &a}));
        QCOMPARE(Material::windowConfigurationScope(&workspace), ab);
        workspace.setProperty("currentDesktop", QVariant::fromValue(&desktop));
        desktop.setProperty("name", "renamed");
        QCOMPARE(Material::windowConfigurationScope(&workspace), ab);
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a}));
        const auto onlyA = Material::windowConfigurationScope(&workspace);
        QVERIFY(onlyA != ab);
        workspace.setProperty("desktopGridHeight", 2);
        QVERIFY(Material::windowConfigurationScope(&workspace) != onlyA);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.Scopes." + QUuid::createUuid().toString());
        const auto key = Material::windowStateKey(&window);
        const auto original = Material::captureWindowState(&window);
        QVERIFY(Material::writeWindowState(key, original, ab));
        QVERIFY(Material::readWindowState(key, onlyA).isEmpty());
        window.setProperty("frameGeometry", QRectF(200, 100, 900, 600));
        QVERIFY(Material::writeWindowState(key, Material::captureWindowState(&window), onlyA));
        QCOMPARE(Material::readWindowState(key, ab).value("position"), original.value("position"));
        QCOMPARE(Material::readWindowState(key, onlyA).value("position").toMap().value("x").toInt(), 200);
        const auto unscoped = key + "-unscoped";
        QVERIFY(Material::writeWindowState(unscoped, original));
        QVERIFY(Material::readWindowState(unscoped, ab).isEmpty());
    }
    void snapshotsBeforeOutputEvacuation() {
        ScopeWorkspace workspace;
        QObject interface(&workspace), a, b, desktop;
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerObject("/KWin", &interface));
        a.setProperty("name", "A"); a.setProperty("geometry", QRect(0, 0, 1280, 800));
        b.setProperty("name", "B"); b.setProperty("geometry", QRect(1280, 0, 1280, 800));
        desktop.setProperty("id", "desktop-1");
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a, &b}));
        workspace.setProperty("desktops", QVariant::fromValue(QList<QObject *>{&desktop}));
        testWorkspace = &workspace;
        KWin::WaylandWindow window;
        const QString app = "org.example.StateTest.Unplug." + QUuid::createUuid().toString();
        setup(window, app);
        window.setProperty("output", QVariant::fromValue(&b));
        window.setProperty("frameGeometry", QRectF(1390, 90, 640, 480));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true);
        window.map();
        const auto ab = Material::windowConfigurationScope(&workspace);
        const auto key = Material::windowStateKey(&window);
        QVERIFY(Material::readWindowState(key, ab).isEmpty());
        // KWin changes its output list, announces removal, then moves windows.
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a}));
        workspace.removeOutput(&b);
        window.setProperty("output", QVariant::fromValue(&a));
        window.setProperty("frameGeometry", QRectF(110, 90, 640, 480));
        workspace.finish();
        const auto onlyA = Material::windowConfigurationScope(&workspace);
        window.close();
        window.setProperty("deleted", true);
        QCOMPARE(Material::readWindowState(key, ab).value("outputName").toString(), "B");
        QCOMPARE(Material::readWindowState(key, ab).value("position").toMap().value("x").toInt(), 1390);
        QCOMPARE(Material::readWindowState(key, onlyA).value("outputName").toString(), "A");
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a, &b}));
        workspace.addOutput(&b);
        workspace.finish();
        KWin::WaylandWindow reopened;
        setup(reopened, app);
        MaterialTest::Bridge reopenedBridge;
        StateDecoration reopenedDecoration(&reopened, &reopenedBridge);
        immediateAvailable = true;
        Material::configureWindowState(&reopenedDecoration, true);
        reopened.map();
        QCOMPARE(immediateRequests.last().value("outputName").toString(), "B");
        QCOMPARE(immediateRequests.last().value("position").toMap().value("x").toInt(), 1390);
        testWorkspace = nullptr;
        bus.unregisterObject("/KWin");
    }
};

QTEST_MAIN(WindowStateTest)
#include "WindowStateTest.moc"
