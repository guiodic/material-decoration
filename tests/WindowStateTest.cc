// SPDX-License-Identifier: GPL-2.0-or-later
#include "WindowState.h"
#include "WindowStateScope.h"
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
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <functional>

// Match the runtime base-class name without including any private KWin header.
namespace KWin {
class QuickRootTile : public QObject { Q_OBJECT public: using QObject::QObject; };
class WaylandWindow : public QObject {
    Q_OBJECT
public:
    void close() { Q_EMIT closed(); }
    void map() { Q_EMIT readyForPaintingChanged(); }
    void changedTile() { Q_EMIT tileChanged(); }
signals:
    void closed();
    void readyForPaintingChanged();
    void tileChanged();
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

static QList<QVariantMap> restoreRequests;
static QList<QVariantMap> immediateRequests;
static bool immediateAvailable = false;
static QList<QVariantMap> prepareRequests;
static bool prepareAvailable = false;
static bool prepareMinimizes = false;
static QVariant preparedDesktops;
static QObject *testWorkspace = nullptr;
namespace Material {
QObject *windowStateWorkspace() { return testWorkspace; }
bool prepareWindowStateImmediately(KDecoration3::Decoration *decoration, const QVariantMap &state)
{
    if (!prepareAvailable) return false;
    prepareRequests.append(state);
    if (prepareMinimizes) decoration->parent()->setProperty("minimized", true);
    if (preparedDesktops.isValid()) decoration->parent()->setProperty("desktops", preparedDesktops);
    return true;
}
bool restoreWindowStateImmediately(KDecoration3::Decoration *, const QVariantMap &state)
{
    if (!immediateAvailable) return false;
    immediateRequests.append(state);
    return true;
}
void restoreWindowState(KDecoration3::Decoration *, const QVariantMap &state, std::function<void(bool)> callback)
{
    restoreRequests.append(state);
    callback(true);
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
        const auto count = restoreRequests.size();
        Material::configureWindowState(&decoration, false);
        window.map();
        QCoreApplication::processEvents();
        QCOMPARE(restoreRequests.size(), count);
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
        const auto count = restoreRequests.size();
        Material::configureWindowState(&decoration, true);
        QCoreApplication::processEvents();
        QCOMPARE(restoreRequests.size(), count); // Wait for the first committed buffer.
        window.map();
        QTRY_COMPARE(restoreRequests.size(), count + 1);
        Material::configureWindowState(&decoration, true);
        QCoreApplication::processEvents();
        QCOMPARE(restoreRequests.size(), count + 1);
        const auto saved = Material::readWindowState(key);
        Material::configureWindowState(&decoration, false);
        window.setProperty("frameGeometry", QRectF(200, 200, 1000, 700));
        window.close();
        QCOMPARE(Material::readWindowState(key), saved);
    }
    void restoresBeforeMappingSignalReturns() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.Immediate." + QUuid::createUuid().toString());
        QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true);
        const auto queued = restoreRequests.size();
        const auto immediate = immediateRequests.size();
        immediateAvailable = true;
        window.map();
        // No processEvents/QTRY: complete before the compositor can render.
        QCOMPARE(immediateRequests.size(), immediate + 1);
        QCOMPARE(restoreRequests.size(), queued);
        QCoreApplication::processEvents();
        QCOMPARE(restoreRequests.size(), queued);
        immediateAvailable = false;
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
        const auto count = restoreRequests.size();
        window.map();
        QCoreApplication::processEvents();
        QCOMPARE(restoreRequests.size(), count);
        prepareAvailable = prepareMinimizes = false;
        preparedDesktops.clear();
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
        immediateAvailable = false;
        testWorkspace = nullptr;
        bus.unregisterObject("/KWin");
    }
};

QTEST_MAIN(WindowStateTest)
#include "WindowStateTest.moc"
