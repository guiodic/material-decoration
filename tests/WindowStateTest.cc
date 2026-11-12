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
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <functional>
#include <limits>

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
    void stepInteraction() { Q_EMIT interactiveMoveResizeStepped(property("frameGeometry").toRectF()); }
    void changeIdentity(const QByteArray &name, const QString &value) {
        setProperty(name.constData(), value);
        if (name == "desktopFileName") Q_EMIT desktopFileNameChanged();
        else if (name == "resourceClass") Q_EMIT windowClassChanged();
        else if (name == "windowRole") Q_EMIT windowRoleChanged();
        else if (name == "tag") Q_EMIT tagChanged();
    }
signals:
    void desktopFileNameChanged();
    void windowClassChanged();
    void windowRoleChanged();
    void tagChanged();
    void closed();
    void readyForPaintingChanged();
    void tileChanged(KWin::Tile *tile);
    void requestedTileChanged();
    void frameGeometryAboutToChange();
    void frameGeometryChanged();
    void maximizedAboutToChange(int mode);
    void maximizedChanged();
    void fullScreenChanged();
    void interactiveMoveResizeStepped(const QRectF &geometry);
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
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        // Direct controller tests simulate a globally enabled feature.
        // The schema defaults are checked separately below.
        const auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
        config->group("Windeco").writeEntry("RememberWindowState", true);
        QVERIFY(config->sync());
        Material::SettingsProvider::self()->reconfigure();
    }
    void disabledByDefault() {
        Material::InternalSettings settings;
        settings.setDefaults();
        QVERIFY(!settings.rememberWindowState());
        QVERIFY(settings.rememberWindowScreen());
        QVERIFY(!settings.rememberWindowDesktop());
        auto source = Material::InternalSettingsPtr::create();
        auto copy = Material::InternalSettingsPtr::create();
        source->setRememberWindowState(true);
        source->setRememberWindowScreen(false);
        source->setRememberWindowDesktop(true);
        Material::copyInternalSettings(source, copy);
        QVERIFY(copy->rememberWindowState());
        QVERIFY(!copy->rememberWindowScreen());
        QVERIFY(copy->rememberWindowDesktop());
    }
    void defaultPolicyKeepsTheOpeningDesktop() {
        QScopedValueRollback prepareAvailability(prepareAvailable, true);
        QScopedValueRollback restoreAvailability(immediateAvailable, true);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.DefaultPolicy." + QUuid::createUuid().toString());
        QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        const auto prepared = prepareRequests.size();
        Material::configureWindowState(&decoration, true);
        window.map();
        QCOMPARE(prepareRequests.size(), prepared);
        QVERIFY(immediateRequests.last().value("rememberScreen").toBool());
        QVERIFY(!immediateRequests.last().value("rememberDesktop").toBool());
    }
    void restoresWithCurrentPolicy_data() {
        QTest::addColumn<bool>("screen");
        QTest::addColumn<bool>("desktop");
        for (bool screen : {false, true}) {
            for (bool desktop : {false, true}) {
                const auto name = QStringLiteral("screen-%1-desktop-%2").arg(screen).arg(desktop);
                QTest::newRow(qPrintable(name)) << screen << desktop;
            }
        }
    }
    void restoresWithCurrentPolicy() {
        QFETCH(bool, screen);
        QFETCH(bool, desktop);
        QScopedValueRollback prepareAvailability(prepareAvailable, true);
        QScopedValueRollback restoreAvailability(immediateAvailable, true);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.Policy." + QUuid::createUuid().toString());
        const auto key = Material::windowStateKey(&window);
        QVERIFY(Material::writeWindowState(key, Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        const auto prepared = prepareRequests.size();
        Material::configureWindowState(&decoration, true, screen, desktop);
        QCOMPARE(prepareRequests.size(), prepared + int(desktop));
        if (desktop) {
            QCOMPARE(prepareRequests.last().value("rememberScreen").toBool(), screen);
            QVERIFY(prepareRequests.last().value("rememberDesktop").toBool());
        }
        window.map();
        QCOMPARE(immediateRequests.last().value("rememberScreen").toBool(), screen);
        QCOMPARE(immediateRequests.last().value("rememberDesktop").toBool(), desktop);
        window.close();
        const auto saved = Material::readWindowState(key);
        QVERIFY(!saved.contains("rememberScreen"));
        QVERIFY(!saved.contains("rememberDesktop"));
    }
    void turningOffDesktopBeforeMappingReleasesPreparation() {
        QScopedValueRollback available(prepareAvailable, true);
        QScopedValueRollback minimizes(prepareMinimizes, true);
        QScopedValueRollback restoreAvailability(immediateAvailable, true);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.DisableDesktop." + QUuid::createUuid().toString());
        QObject originalDesktop, savedDesktop;
        const auto original = QVariant::fromValue(QList<QObject *>{&originalDesktop});
        QScopedValueRollback desktops(preparedDesktops, QVariant::fromValue(QList<QObject *>{&savedDesktop}));
        window.setProperty("desktops", original);
        window.setProperty("minimized", false);
        QVERIFY(Material::writeWindowState(Material::windowStateKey(&window), Material::captureWindowState(&window)));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true, true, true);
        QVERIFY(window.property("minimized").toBool());
        Material::configureWindowState(&decoration, true, true, false);
        QVERIFY(!window.property("minimized").toBool());
        QCOMPARE(window.property("desktops"), original);
        const auto prepared = prepareRequests.size();
        QCoreApplication::processEvents();
        window.map();
        QCOMPARE(prepareRequests.size(), prepared);
        QVERIFY(!immediateRequests.last().value("rememberDesktop").toBool());
    }
    void policySettingsRoundTripAndInheritThroughExceptions() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto config = KSharedConfig::openConfig(directory.filePath("settingsrc"), KConfig::SimpleConfig);
        Material::InternalSettings settings;
        settings.setSharedConfig(config);
        settings.load();
        settings.setDefaults();
        settings.setRememberWindowState(true);
        settings.setRememberWindowScreen(false);
        settings.setRememberWindowDesktop(true);
        settings.save();
        Material::InternalSettings loaded;
        loaded.setSharedConfig(config);
        loaded.load();
        QVERIFY(loaded.rememberWindowState());
        QVERIFY(!loaded.rememberWindowScreen());
        QVERIFY(loaded.rememberWindowDesktop());
        Material::SettingsProvider provider;
        auto defaults = Material::InternalSettingsPtr::create();
        defaults->setRememberWindowState(true);
        defaults->setRememberWindowScreen(false);
        defaults->setRememberWindowDesktop(true);
        auto exception = Material::InternalSettingsPtr::create();
        exception->setMask(Material::ExceptionMask::ExcludeWindowPosition);
        exception->setExcludeWindowPosition(true);
        const auto merged = provider.createMergedSettings(defaults, exception);
        QVERIFY(!merged->rememberWindowState());
        QVERIFY(!merged->rememberWindowScreen());
        QVERIFY(merged->rememberWindowDesktop());
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
    void identityChangesReevaluateExclusions_data() {
        QTest::addColumn<bool>("excludedAfterChange");
        QTest::addColumn<bool>("mappedBeforeChange");
        QTest::addColumn<bool>("withoutDecoration");
        QTest::addColumn<bool>("snapshot");
        QTest::addColumn<bool>("desktopFileSignal");
        for (bool excluded : {false, true}) {
            for (bool mapped : {false, true}) {
                for (bool withoutDecoration : {false, true}) {
                    for (bool snapshot : {false, true}) {
                        for (bool desktopFile : {false, true}) {
                            const auto name = QStringLiteral("%1-%2-%3-%4-%5").arg(excluded ? "excluded" : "allowed",
                                mapped ? "mapped" : "unmapped", withoutDecoration ? "no-decoration" : "decorated",
                                snapshot ? "snapshot" : "close", desktopFile ? "desktop-file" : "class");
                            QTest::newRow(qPrintable(name)) << excluded << mapped << withoutDecoration << snapshot << desktopFile;
                        }
                    }
                }
            }
        }
    }
    void identityChangesReevaluateExclusions() {
        QFETCH(bool, excludedAfterChange);
        QFETCH(bool, mappedBeforeChange);
        QFETCH(bool, withoutDecoration);
        QFETCH(bool, snapshot);
        QFETCH(bool, desktopFileSignal);
        QScopedValueRollback available(immediateAvailable, false);
        QScopedValueRollback prepareAvailability(prepareAvailable, false);
        const auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
        const auto cleanup = qScopeGuard([config] {
            config->deleteGroup("Windeco Exception 0");
            config->sync();
            Material::SettingsProvider::self()->reconfigure();
        });
        const QString app = "org.example.StateTest.ClassExclusion." + QUuid::createUuid().toString();
        const QString allowed = app + ".Allowed", excluded = app + ".Excluded";
        auto exception = Material::InternalSettingsPtr::create();
        exception->setEnabled(true);
        exception->setExceptionPattern(excluded);
        exception->setExceptionType(int(Material::ExceptionType::WindowClass));
        exception->setMatchingMode(int(Material::MatchingMode::ExactMatch));
        exception->setMask(Material::ExceptionMask::ExcludeWindowPosition);
        exception->setExcludeWindowPosition(true);
        Material::ExceptionList list;
        list.setExceptions({exception});
        list.writeConfig(config);
        QVERIFY(config->sync());
        Material::SettingsProvider::self()->reconfigure();
        KWin::WaylandWindow window;
        const QString initialClass = excludedAfterChange ? allowed : excluded;
        const QString nextClass = excludedAfterChange ? excluded : allowed;
        setup(window, initialClass);
        window.setProperty("resourceName", "material-state-test");
        window.setProperty("resourceClass", initialClass);
        window.setProperty("caption", "Unchanged title");
        const auto scope = Material::WindowStateScope::self()->current();
        const auto originalKey = Material::windowStateKey(&window);
        QVERIFY(Material::writeWindowState(originalKey, Material::captureWindowState(&window), scope));
        const auto originalState = Material::readWindowState(originalKey, scope);
        MaterialTest::Bridge bridge;
        auto decoration = std::make_unique<StateDecoration>(&window, &bridge);
        Material::configureWindowState(decoration.get(), excludedAfterChange);
        if (mappedBeforeChange) window.map();
        if (withoutDecoration) decoration.reset();
        // Update both properties as KWin does, without a caption notification.
        // Exercise either notification independently.
        window.setProperty("resourceClass", nextClass);
        window.setProperty("desktopFileName", nextClass);
        if (desktopFileSignal) Q_EMIT window.desktopFileNameChanged();
        else Q_EMIT window.windowClassChanged();
        const auto key = Material::windowStateKey(&window);
        QVERIFY(key != originalKey);
        immediateAvailable = true;
        if (!mappedBeforeChange) window.map();
        if (excludedAfterChange && mappedBeforeChange) {
            const auto count = immediateRequests.size();
            QTest::qWait(300); // A previously queued restore must not run.
            QCOMPARE(immediateRequests.size(), count);
        }
        window.setProperty("frameGeometry", QRectF(400, 250, 800, 500));
        if (snapshot) Q_EMIT Material::WindowStateScope::self()->snapshotRequested(scope, {});
        else window.close();
        const auto saved = Material::readWindowState(key, scope);
        if (excludedAfterChange) QVERIFY(saved.isEmpty());
        else QCOMPARE(saved.value("position").toMap().value("x").toInt(), 400);
        QCOMPARE(Material::readWindowState(originalKey, scope), originalState);
        QCOMPARE(window.property("caption").toString(), "Unchanged title");
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
            Material::configureWindowState(&decoration, true, true, true);
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
    void persistsQuickTileProportions_data() {
        QTest::addColumn<int>("edges");
        QTest::addColumn<QRectF>("region");
        QTest::newRow("left") << 2 << QRectF(0, 0, 0.65, 1);
        QTest::newRow("right") << 4 << QRectF(0.65, 0, 0.35, 1);
        QTest::newRow("top") << 1 << QRectF(0, 0, 1, 0.4);
        QTest::newRow("bottom") << 8 << QRectF(0, 0.4, 1, 0.6);
        QTest::newRow("top-left") << 3 << QRectF(0, 0, 0.65, 0.4);
        QTest::newRow("top-right") << 5 << QRectF(0.65, 0, 0.35, 0.4);
        QTest::newRow("bottom-left") << 10 << QRectF(0, 0.4, 0.65, 0.6);
        QTest::newRow("bottom-right") << 12 << QRectF(0.65, 0.4, 0.35, 0.6);
    }
    void persistsQuickTileProportions() {
        QFETCH(int, edges);
        QFETCH(QRectF, region);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.TileProportions." + QUuid::createUuid().toString());
        KWin::QuickRootTile root;
        KWin::Tile tile;
        tile.setProperty("parent", QVariant::fromValue<QObject *>(&root));
        tile.setProperty("relativeGeometry", region);
        window.setProperty("tile", QVariant::fromValue<QObject *>(&tile));
        const auto state = Material::captureWindowState(&window);
        const auto key = Material::windowStateKey(&window);
        QVERIFY(Material::writeWindowState(key, state));
        const auto savedTile = Material::readWindowState(key).value("tile").toMap();
        QCOMPARE(savedTile.value("edges").toInt(), edges);
        const auto saved = savedTile.value("region").toMap();
        QCOMPARE(saved.value("x").toDouble(), region.x());
        QCOMPARE(saved.value("y").toDouble(), region.y());
        QCOMPARE(saved.value("width").toDouble(), region.width());
        QCOMPARE(saved.value("height").toDouble(), region.height());
    }
    void preservesQuickTileModeWhenAnOuterEdgeIsResized() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.TileMode." + QUuid::createUuid().toString());
        KWin::QuickRootTile root;
        KWin::Tile tiles[8];
        QVariantList siblings;
        for (auto &tile : tiles) siblings.append(QVariant::fromValue<QObject *>(&tile));
        root.setProperty("tiles", siblings);
        tiles[0].setProperty("parent", QVariant::fromValue<QObject *>(&root));
        tiles[0].setProperty("relativeGeometry", QRectF(0, 0, 0.65, 0.85));
        window.setProperty("tile", QVariant::fromValue<QObject *>(&tiles[0]));
        const auto key = Material::windowStateKey(&window);
        QVERIFY(Material::writeWindowState(key, Material::captureWindowState(&window)));
        const auto saved = Material::readWindowState(key).value("tile").toMap();
        QCOMPARE(saved.value("edges").toInt(), 2); // Still left, not top-left.
        QCOMPARE(saved.value("region").toMap().value("height").toDouble(), 0.85);
    }
    void rejectsInvalidQuickTileProportions() {
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.InvalidProportions." + QUuid::createUuid().toString());
        const auto key = Material::windowStateKey(&window);
        auto state = Material::captureWindowState(&window);
        for (const auto &region : QList<QVariantMap>{ {}, {{"x", 0}, {"y", 0}, {"width", 0.6}},
                {{"x", -0.1}, {"y", 0}, {"width", 0.7}, {"height", 1}},
                {{"x", 0}, {"y", 0}, {"width", 1.2}, {"height", 1}},
                {{"x", 0}, {"y", 0}, {"width", 0}, {"height", 1}},
                {{"x", 0}, {"y", 0}, {"width", 0.6}, {"height", 1.2}},
                {{"x", 0}, {"y", 0}, {"width", std::numeric_limits<double>::infinity()}, {"height", 1}}}) {
            state.insert("tile", QVariantMap{{"kind", "quick"}, {"edges", 2}, {"region", region}});
            QVERIFY(Material::writeWindowState(key, state));
            const auto saved = Material::readWindowState(key).value("tile").toMap();
            QCOMPARE(saved.value("kind").toString(), "quick");
            QCOMPARE(saved.value("edges").toInt(), 2);
            QVERIFY(!saved.contains("region"));
        }
    }
    void remembersResizedTileBeforeCloseCleanup_data() {
        QTest::addColumn<bool>("interactiveFinish");
        QTest::newRow("interactive-finish-before-buffer") << true;
        QTest::newRow("frame-committed") << false;
    }
    void remembersResizedTileBeforeCloseCleanup() {
        QFETCH(bool, interactiveFinish);
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.ResizedTileCleanup." + QUuid::createUuid().toString());
        window.setProperty("frameGeometry", QRectF(100, 100, 900, 650));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true);
        window.map();
        KWin::QuickRootTile root;
        KWin::Tile tile;
        tile.setProperty("parent", QVariant::fromValue<QObject *>(&root));
        tile.setProperty("relativeGeometry", QRectF(0, 0, 0.5, 1));
        window.setProperty("tile", QVariant::fromValue<QObject *>(&tile));
        Q_EMIT window.requestedTileChanged();
        window.changedTile();
        tile.setProperty("relativeGeometry", QRectF(0, 0, 0.65, 1));
        window.setProperty("frameGeometry", QRectF(0, 0, 832, 800));
        if (interactiveFinish) window.finishInteraction();
        else {
            Q_EMIT window.frameGeometryChanged();
            QCoreApplication::processEvents();
        }
        const auto key = Material::windowStateKey(&window);
        const auto scope = Material::WindowStateScope::self()->current();
        QVERIFY(Material::readWindowState(key, scope).isEmpty());
        window.setProperty("tile", QVariant::fromValue<QObject *>(nullptr));
        window.close();
        const auto saved = Material::readWindowState(key, scope).value("tile").toMap();
        QCOMPARE(saved.value("region").toMap().value("width").toDouble(), 0.65);
        QCOMPARE(saved.value("normalSize").toMap().value("width").toDouble(), 900);
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
        Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&sourceDecoration, true, true, true);
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
            Material::configureWindowState(&extraDecoration, true, true, true);
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
        Material::configureWindowState(&targetDecoration, true, true, true);
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
        Material::configureWindowState(decoration, true, true, true);
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
        Material::configureWindowState(&decoration, true, true, true);
        QCoreApplication::processEvents();
        QCOMPARE(immediateRequests.size(), count); // Wait for the first committed buffer.
        window.map();
        QTRY_COMPARE(immediateRequests.size(), count + 1);
        Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&decoration, true, true, true);
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
        auto state = Material::captureWindowState(&window);
        state.insert("tile", QVariantMap{{"kind", "quick"}, {"edges", 2},
            {"normalSize", QVariantMap{{"width", 900}, {"height", 650}}}});
        QVERIFY(Material::writeWindowState(key, state, scope));
        const auto saved = Material::readWindowState(key, scope);
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&decoration, true, true, true);
        window.map();
        Q_EMIT Material::WindowStateScope::self()->snapshotRequested(scope, {});
        QVERIFY(!Material::readWindowState(key, scope).isEmpty());
        // A snapshot created by this same new window must not prevent its
        // later close from recording the final placement.
        window.setProperty("frameGeometry", QRectF(300, 200, 800, 500));
        window.close();
        QCOMPARE(Material::readWindowState(key, scope).value("position").toMap().value("x").toInt(), 300);
    }
    void siblingWindowsSaveInClosingOrder_data() {
        QTest::addColumn<bool>("available");
        QTest::addColumn<bool>("savedBeforeOpening");
        QTest::addColumn<bool>("snapshot");
        for (bool available : {false, true}) {
            for (bool saved : {false, true}) {
                for (bool snapshot : {false, true}) {
                    const auto name = QStringLiteral("%1-%2-%3").arg(available ? "script" : "no-script",
                        saved ? "existing" : "new", snapshot ? "snapshot" : "close");
                    QTest::newRow(qPrintable(name)) << available << saved << snapshot;
                }
            }
        }
    }
    void siblingWindowsSaveInClosingOrder() {
        QFETCH(bool, available);
        QFETCH(bool, savedBeforeOpening);
        QFETCH(bool, snapshot);
        QScopedValueRollback restoreAvailability(immediateAvailable, available);
        QScopedValueRollback restoreSuccess(immediateSucceeds, true);
        KWin::WaylandWindow first, last;
        const QString app = "org.example.StateTest.SiblingSave." + QUuid::createUuid().toString();
        setup(first, app);
        setup(last, app);
        const auto key = Material::windowStateKey(&first);
        const auto scope = Material::WindowStateScope::self()->current();
        if (savedBeforeOpening) QVERIFY(Material::writeWindowState(key, Material::captureWindowState(&first), scope));
        const auto initial = Material::readWindowState(key, scope);
        MaterialTest::Bridge firstBridge, lastBridge;
        StateDecoration firstDecoration(&first, &firstBridge), lastDecoration(&last, &lastBridge);
        Material::configureWindowState(&firstDecoration, true, true, true);
        Material::configureWindowState(&lastDecoration, true, true, true);
        first.map();
        last.map();
        // App/KWin placement without a manual move or resize.
        first.setProperty("frameGeometry", QRectF(300, 200, 800, 500));
        last.setProperty("frameGeometry", QRectF(500, 400, 800, 500));
        first.close();
        first.setProperty("deleted", true);
        if (snapshot) Q_EMIT Material::WindowStateScope::self()->snapshotRequested(scope, {});
        last.close();
        const auto saved = Material::readWindowState(key, scope);
        if (savedBeforeOpening && !available) QCOMPARE(saved, initial);
        else QCOMPARE(saved.value("position").toMap().value("x").toInt(), 500);
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
        Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&decoration, true, true, true);
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
            Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&decoration, true, true, true);
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
            Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&decoration, true, true, true);
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
    void reloadsStateWhenOpeningConfigurationChanges_data() {
        QTest::addColumn<int>("phase");
        QTest::addColumn<bool>("hasState");
        QTest::addColumn<bool>("removeDesktop");
        for (int phase = 0; phase < 3; ++phase) {
            for (bool hasState : {false, true}) {
                for (bool removeDesktop : {false, true}) {
                    const QByteArray name = QByteArray::number(phase) + (hasState ? "-saved" : "-missing")
                        + (removeDesktop ? "-removed-desktop" : "-same-desktops");
                    QTest::newRow(name.constData()) << phase << hasState << removeDesktop;
                }
            }
        }
    }
    void reloadsStateWhenOpeningConfigurationChanges() {
        QFETCH(int, phase); // Reassertion, first frame, or full-restore retry.
        QFETCH(bool, hasState);
        QFETCH(bool, removeDesktop);
        ScopeWorkspace workspace;
        QObject interface(&workspace), a, b, originalDesktop, savedDesktop;
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerObject("/KWin", &interface));
        QScopedValueRollback wrapper(testWorkspace, static_cast<QObject *>(&workspace));
        QScopedValueRollback available(prepareAvailable, true);
        QScopedValueRollback minimizes(prepareMinimizes, true);
        QScopedValueRollback restoreAvailable(immediateAvailable, phase != 2);
        QScopedValueRollback desktops(preparedDesktops, QVariant::fromValue(QList<QObject *>{&savedDesktop}));
        a.setProperty("name", "A"); a.setProperty("geometry", QRect(0, 0, 1280, 800));
        b.setProperty("name", "B"); b.setProperty("geometry", QRect(1280, 0, 1280, 800));
        originalDesktop.setProperty("id", "original");
        savedDesktop.setProperty("id", "saved");
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a, &b}));
        workspace.setProperty("desktops", QVariant::fromValue(QList<QObject *>{&originalDesktop, &savedDesktop}));
        workspace.setProperty("currentDesktop", QVariant::fromValue(&originalDesktop));
        const auto before = Material::WindowStateScope::self()->current();
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.ChangeWhileOpening." + QUuid::createUuid().toString());
        const auto original = QVariant::fromValue(QList<QObject *>{&originalDesktop});
        window.setProperty("desktops", original);
        window.setProperty("minimized", false);
        const auto key = Material::windowStateKey(&window);
        auto state = Material::captureWindowState(&window);
        state.insert("position", QVariantMap{{"x", 110}, {"y", 90}});
        QVERIFY(Material::writeWindowState(key, state, before));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true, true, true);
        QVERIFY(window.property("minimized").toBool());
        QCOMPARE(window.property("desktops"), preparedDesktops);
        if (phase == 2) window.map();
        const auto prepareCount = prepareRequests.size();
        const auto restoreCount = immediateRequests.size();
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a}));
        if (removeDesktop) {
            workspace.setProperty("desktops", QVariant::fromValue(QList<QObject *>{&savedDesktop}));
            workspace.setProperty("currentDesktop", QVariant::fromValue(&savedDesktop));
        }
        workspace.finish();
        const auto after = Material::WindowStateScope::self()->current();
        QVERIFY(before != after);
        if (hasState) {
            state.insert("position", QVariantMap{{"x", 330}, {"y", 190}});
            QVERIFY(Material::writeWindowState(key, state, after));
        }
        immediateAvailable = true;
        if (phase == 0) QCoreApplication::processEvents();
        if (phase != 2) window.map();
        if (phase == 2) QTest::qWait(350);
        QCOMPARE(prepareRequests.size(), prepareCount + (phase == 0 && hasState ? 1 : 0));
        QCOMPARE(immediateRequests.size(), restoreCount + (hasState ? 1 : 0));
        if (hasState) {
            QCOMPARE(immediateRequests.last().value("position").toMap().value("x").toInt(), 330);
            if (phase == 0) QCOMPARE(prepareRequests.last().value("position").toMap().value("x").toInt(), 330);
        } else {
            QCOMPARE(window.property("desktops"), removeDesktop ? preparedDesktops : original);
        }
        QVERIFY(!window.property("minimized").toBool());
        // Merely looking up a different scope must not modify either state.
        QCOMPARE(Material::readWindowState(key, before).value("position").toMap().value("x").toInt(), 110);
        QCOMPARE(Material::readWindowState(key, after).isEmpty(), !hasState);
        bus.unregisterObject("/KWin");
    }
    void siblingWindowsSaveAfterConfigurationChange_data() {
        QTest::addColumn<bool>("screenChange");
        QTest::addColumn<bool>("savedInNewScope");
        QTest::addColumn<bool>("available");
        for (bool screen : {false, true}) {
            for (bool saved : {false, true}) {
                for (bool available : {false, true}) {
                    const auto name = QStringLiteral("%1-%2-%3").arg(screen ? "screens" : "desktops",
                        saved ? "existing" : "empty", available ? "script" : "no-script");
                    QTest::newRow(qPrintable(name)) << screen << saved << available;
                }
            }
        }
    }
    void siblingWindowsSaveAfterConfigurationChange() {
        QFETCH(bool, screenChange);
        QFETCH(bool, savedInNewScope);
        QFETCH(bool, available);
        ScopeWorkspace workspace;
        QObject interface(&workspace), a, b, desktop, otherDesktop;
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerObject("/KWin", &interface));
        QScopedValueRollback wrapper(testWorkspace, static_cast<QObject *>(&workspace));
        QScopedValueRollback restoreAvailability(immediateAvailable, available);
        a.setProperty("name", "A"); a.setProperty("geometry", QRect(0, 0, 1280, 800));
        b.setProperty("name", "B"); b.setProperty("geometry", QRect(1280, 0, 1280, 800));
        desktop.setProperty("id", "desktop");
        otherDesktop.setProperty("id", "other");
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a, &b}));
        workspace.setProperty("desktops", QVariant::fromValue(QList<QObject *>{&desktop}));
        KWin::WaylandWindow first, last;
        const QString app = "org.example.StateTest.SiblingScope." + QUuid::createUuid().toString();
        setup(first, app);
        setup(last, app);
        const auto key = Material::windowStateKey(&first);
        const auto before = Material::WindowStateScope::self()->current();
        MaterialTest::Bridge firstBridge, lastBridge;
        StateDecoration firstDecoration(&first, &firstBridge), lastDecoration(&last, &lastBridge);
        Material::configureWindowState(&firstDecoration, true, true, true);
        Material::configureWindowState(&lastDecoration, true, true, true);
        first.map();
        last.map();
        if (screenChange) workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a}));
        else workspace.setProperty("desktops", QVariant::fromValue(QList<QObject *>{&desktop, &otherDesktop}));
        const auto after = Material::windowConfigurationScope(&workspace);
        QVERIFY(before != after);
        if (savedInNewScope) QVERIFY(Material::writeWindowState(key, Material::captureWindowState(&first), after));
        const auto protectedState = Material::readWindowState(key, after);
        if (screenChange) {
            workspace.removeOutput(&b);
            workspace.finish();
        } else {
            Q_EMIT workspace.desktopsChanged();
        }
        QCOMPARE(Material::WindowStateScope::self()->current(), after);
        first.setProperty("frameGeometry", QRectF(300, 200, 800, 500));
        last.setProperty("frameGeometry", QRectF(500, 400, 800, 500));
        first.close();
        first.setProperty("deleted", true);
        if (!savedInNewScope) QCOMPARE(Material::readWindowState(key, after).value("position").toMap().value("x").toInt(), 300);
        last.close();
        const auto saved = Material::readWindowState(key, after);
        if (savedInNewScope) QCOMPARE(saved, protectedState);
        else QCOMPARE(saved.value("position").toMap().value("x").toInt(), 500);
        bus.unregisterObject("/KWin");
    }
    void manualPlacementAfterOutputRemoval_data() {
        QTest::addColumn<QString>("action");
        QTest::addColumn<bool>("anotherChange");
        QTest::addColumn<bool>("failedInitialRestore");
        for (const auto &action : {"untouched", "application", "no-step", "move", "resize", "both", "before-removal"}) {
            for (bool anotherChange : {false, true}) {
                for (bool failed : {false, true}) {
                    const QByteArray name = QByteArray(action) + (anotherChange ? "-then-topology" : "-then-close")
                        + (failed ? "-failed-initial-restore" : "-new");
                    QTest::newRow(name.constData()) << QString::fromLatin1(action) << anotherChange << failed;
                }
            }
        }
    }
    void manualPlacementAfterOutputRemoval() {
        QFETCH(QString, action);
        QFETCH(bool, anotherChange);
        QFETCH(bool, failedInitialRestore);
        ScopeWorkspace workspace;
        QObject interface(&workspace), a, b, desktop;
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerObject("/KWin", &interface));
        QScopedValueRollback wrapper(testWorkspace, static_cast<QObject *>(&workspace));
        QScopedValueRollback available(immediateAvailable, !failedInitialRestore);
        a.setProperty("name", "A"); a.setProperty("geometry", QRect(1920, 0, 1920, 1080));
        b.setProperty("name", "B"); b.setProperty("geometry", QRect(0, 0, 1920, 1080));
        desktop.setProperty("id", "desktop");
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a, &b}));
        workspace.setProperty("desktops", QVariant::fromValue(QList<QObject *>{&desktop}));
        workspace.setProperty("desktopGridWidth", 1);
        workspace.setProperty("desktopGridHeight", 1);
        KWin::WaylandWindow window;
        const QString app = "org.example.StateTest.ManualAfterRemoval." + QUuid::createUuid().toString();
        setup(window, app);
        window.setProperty("output", QVariant::fromValue(&b));
        window.setProperty("frameGeometry", QRectF(110, 90, 640, 480));
        const auto key = Material::windowStateKey(&window);
        const auto ab = Material::WindowStateScope::self()->current();
        if (failedInitialRestore) QVERIFY(Material::writeWindowState(key, Material::captureWindowState(&window), ab));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true, true, true);
        window.map();
        if (action == "before-removal") {
            window.startInteraction();
            window.setProperty("frameGeometry", QRectF(150, 90, 640, 480));
            window.stepInteraction();
            window.finishInteraction();
        }
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a}));
        workspace.removeOutput(&b);
        const auto onB = Material::readWindowState(key, ab);
        QCOMPARE(onB.value("outputName").toString(), "B");
        a.setProperty("geometry", QRect(0, 0, 1920, 1080));
        window.setProperty("output", QVariant::fromValue(&a));
        window.setProperty("frameGeometry", QRectF(110, 90, 640, 480));
        workspace.finish();
        const auto onlyA = Material::WindowStateScope::self()->current();
        QVERIFY(ab != onlyA);
        const bool manual = action == "move" || action == "resize" || action == "both";
        if (manual || action == "no-step") window.startInteraction();
        if (manual || action == "application") {
            const bool move = action != "resize";
            const bool resize = action != "move";
            window.setProperty("frameGeometry", QRectF(move ? 315 : 110, move ? 210 : 90,
                resize ? 900 : 640, resize ? 650 : 480));
            Q_EMIT window.frameGeometryChanged();
        }
        if (manual) window.stepInteraction();
        if (manual || action == "no-step") window.finishInteraction();
        QCOMPARE(Material::readWindowState(key, ab), onB); // No writes while interacting.
        if (anotherChange) {
            Q_EMIT workspace.desktopLayoutChanged();
            workspace.setProperty("desktopGridWidth", 2);
            QCoreApplication::processEvents();
        }
        window.close();
        window.setProperty("deleted", true);
        const auto saved = Material::readWindowState(key, ab);
        if (manual) {
            QCOMPARE(saved.value("outputName").toString(), "A");
            QCOMPARE(saved.value("position").toMap().value("x").toInt(), action == "resize" ? 110 : 315);
            QCOMPARE(saved.value("outputPosition").toMap().value("x").toInt(), 0);
            QVERIFY(!saved.contains("normalSize"));
            QVERIFY(!saved.contains("size"));
        } else QCOMPARE(saved, onB);
        // Reconnect with the original layout and check which output is restored.
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a, &b}));
        a.setProperty("geometry", QRect(1920, 0, 1920, 1080));
        workspace.setProperty("desktopGridWidth", 1);
        workspace.addOutput(&b);
        workspace.finish();
        QCOMPARE(Material::WindowStateScope::self()->current(), ab);
        KWin::WaylandWindow reopened;
        setup(reopened, app);
        MaterialTest::Bridge reopenedBridge;
        StateDecoration reopenedDecoration(&reopened, &reopenedBridge);
        const auto restores = immediateRequests.size();
        immediateAvailable = true;
        Material::configureWindowState(&reopenedDecoration, true, true, true);
        reopened.map();
        QCOMPARE(immediateRequests.size(), restores + 1);
        QCOMPARE(immediateRequests.last().value("outputName").toString(), manual ? "A" : "B");
        bus.unregisterObject("/KWin");
    }
    void saveAuthorizationIsScoped_data() {
        QTest::addColumn<bool>("restored");
        QTest::addColumn<bool>("manualInNewScope");
        QTest::addColumn<bool>("snapshot");
        QTest::addColumn<bool>("screenChange");
        for (bool restored : {false, true}) {
            for (bool manual : {false, true}) {
                for (bool snapshot : {false, true}) {
                    for (bool screen : {false, true}) {
                        const auto name = QStringLiteral("%1-%2-%3-%4").arg(restored ? "restored" : "user-moved",
                            manual ? "new-user-move" : "automatic", snapshot ? "snapshot" : "close", screen ? "screens" : "desktops");
                        QTest::newRow(qPrintable(name)) << restored << manual << snapshot << screen;
                    }
                }
            }
        }
    }
    void saveAuthorizationIsScoped() {
        QFETCH(bool, restored);
        QFETCH(bool, manualInNewScope);
        QFETCH(bool, snapshot);
        QFETCH(bool, screenChange);
        ScopeWorkspace workspace;
        QObject interface(&workspace), a, b, desktop, otherDesktop;
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerObject("/KWin", &interface));
        QScopedValueRollback wrapper(testWorkspace, static_cast<QObject *>(&workspace));
        QScopedValueRollback available(immediateAvailable, restored);
        QScopedValueRollback succeeds(immediateSucceeds, true);
        a.setProperty("name", "A"); a.setProperty("geometry", QRect(0, 0, 1280, 800));
        b.setProperty("name", "B"); b.setProperty("geometry", QRect(1280, 0, 1280, 800));
        desktop.setProperty("id", "desktop");
        otherDesktop.setProperty("id", "other");
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a}));
        workspace.setProperty("desktops", QVariant::fromValue(QList<QObject *>{&desktop}));
        const auto before = Material::WindowStateScope::self()->current();
        KWin::WaylandWindow window;
        setup(window, "org.example.StateTest.ScopePermission." + QUuid::createUuid().toString());
        window.setProperty("output", QVariant::fromValue(&a));
        window.setProperty("frameGeometry", QRectF(100, 80, 640, 480));
        const auto key = Material::windowStateKey(&window);
        QVERIFY(Material::writeWindowState(key, Material::captureWindowState(&window), before));
        // A separate, pre-existing state belongs to the future configuration.
        if (screenChange) workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a, &b}));
        else workspace.setProperty("desktops", QVariant::fromValue(QList<QObject *>{&desktop, &otherDesktop}));
        const auto after = Material::windowConfigurationScope(&workspace);
        QVERIFY(before != after);
        auto protectedState = Material::captureWindowState(&window);
        protectedState.insert("position", QVariantMap{{"x", screenChange ? 1500 : 500}, {"y", 180}});
        protectedState.insert("outputName", screenChange ? "B" : "A");
        protectedState.insert("outputPosition", QVariantMap{{"x", screenChange ? 1280 : 0}, {"y", 0}});
        protectedState.insert("desktopIds", QStringList{screenChange ? "desktop" : "other"});
        QVERIFY(Material::writeWindowState(key, protectedState, after));
        protectedState = Material::readWindowState(key, after);
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a}));
        workspace.setProperty("desktops", QVariant::fromValue(QList<QObject *>{&desktop}));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true);
        window.map();
        if (!restored) {
            window.startInteraction();
            window.setProperty("frameGeometry", QRectF(300, 200, 640, 480));
            window.stepInteraction();
            window.finishInteraction();
        }
        if (screenChange) {
            workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a, &b}));
            workspace.addOutput(&b);
            workspace.finish();
        } else {
            workspace.setProperty("desktops", QVariant::fromValue(QList<QObject *>{&desktop, &otherDesktop}));
            Q_EMIT workspace.desktopLayoutChanged();
            Q_EMIT workspace.desktopsChanged();
        }
        QCOMPARE(Material::WindowStateScope::self()->current(), after);
        // The original scope remains writable after its restore/user action.
        QCOMPARE(Material::readWindowState(key, before).value("position").toMap().value("x").toInt(), restored ? 100 : 300);
        window.setProperty("frameGeometry", QRectF(400, 250, 640, 480));
        if (manualInNewScope) {
            window.startInteraction();
            window.setProperty("frameGeometry", QRectF(500, 350, 640, 480));
            window.stepInteraction();
            window.finishInteraction();
        }
        if (snapshot) Q_EMIT Material::WindowStateScope::self()->snapshotRequested(after, {});
        else window.close();
        const auto saved = Material::readWindowState(key, after);
        if (manualInNewScope) QCOMPARE(saved.value("position").toMap().value("x").toInt(), 500);
        else QCOMPARE(saved, protectedState);
        window.setProperty("deleted", true);
        bus.unregisterObject("/KWin");
    }
    void saveAuthorizationFollowsWindowIdentity_data() {
        QTest::addColumn<QByteArray>("property");
        QTest::addColumn<QString>("authorization");
        QTest::addColumn<bool>("manualAfterChange");
        QTest::addColumn<bool>("snapshot");
        for (const auto &property : {"desktopFileName", "resourceClass", "windowRole", "tag"}) {
            for (const auto &authorization : {"restored", "user-moved", "saved"}) {
                for (bool manual : {false, true}) {
                    for (bool snapshot : {false, true}) {
                        const QByteArray name = QByteArray(property) + "-" + authorization
                            + (manual ? "-new-user-move" : "-automatic") + (snapshot ? "-snapshot" : "-close");
                        QTest::newRow(name.constData()) << QByteArray(property) << QString::fromLatin1(authorization) << manual << snapshot;
                    }
                }
            }
        }
    }
    void saveAuthorizationFollowsWindowIdentity() {
        QFETCH(QByteArray, property);
        QFETCH(QString, authorization);
        QFETCH(bool, manualAfterChange);
        QFETCH(bool, snapshot);
        QScopedValueRollback available(immediateAvailable, authorization == "restored");
        QScopedValueRollback succeeds(immediateSucceeds, true);
        QScopedValueRollback prepareAvailability(prepareAvailable, false);
        KWin::WaylandWindow window, future;
        const QString app = "org.example.StateTest.IdentityPermission." + QUuid::createUuid().toString();
        for (auto *w : {&window, &future}) {
            setup(*w, app);
            if (property == "resourceClass") {
                w->setProperty("desktopFileName", "");
                w->setProperty("resourceClass", app);
            }
        }
        future.setProperty(property.constData(), QString(app + ".new"));
        const auto key = Material::windowStateKey(&window);
        const auto newKey = Material::windowStateKey(&future);
        QVERIFY(key != newKey);
        const auto scope = Material::WindowStateScope::self()->current();
        future.setProperty("frameGeometry", QRectF(880, 450, 800, 500));
        QVERIFY(Material::writeWindowState(newKey, Material::captureWindowState(&future), scope));
        const auto protectedState = Material::readWindowState(newKey, scope);
        if (authorization != "saved") QVERIFY(Material::writeWindowState(key, Material::captureWindowState(&window), scope));
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true);
        window.map();
        if (authorization == "user-moved") {
            window.startInteraction();
            window.setProperty("frameGeometry", QRectF(300, 200, 800, 500));
            window.stepInteraction();
            window.finishInteraction();
        } else if (authorization == "saved") {
            Q_EMIT Material::WindowStateScope::self()->snapshotRequested(scope, {});
        }
        const auto originalState = Material::readWindowState(key, scope);
        window.changeIdentity(property, app + ".new");
        QCOMPARE(Material::windowStateKey(&window), newKey);
        window.setProperty("frameGeometry", QRectF(400, 250, 800, 500));
        if (manualAfterChange) {
            window.startInteraction();
            window.setProperty("frameGeometry", QRectF(500, 350, 800, 500));
            window.stepInteraction();
            window.finishInteraction();
        }
        if (snapshot) Q_EMIT Material::WindowStateScope::self()->snapshotRequested(scope, {});
        else window.close();
        const auto saved = Material::readWindowState(newKey, scope);
        if (manualAfterChange) QCOMPARE(saved.value("position").toMap().value("x").toInt(), 500);
        else QCOMPARE(saved, protectedState);
        QCOMPARE(Material::readWindowState(key, scope), originalState);
    }
    void siblingWindowsEnteringNewIdentitySaveInClosingOrder() {
        QScopedValueRollback available(immediateAvailable, false);
        KWin::WaylandWindow first, last;
        const QString app = "org.example.StateTest.SharedNewIdentity." + QUuid::createUuid().toString();
        setup(first, app + ".first");
        setup(last, app + ".last");
        MaterialTest::Bridge firstBridge, lastBridge;
        StateDecoration firstDecoration(&first, &firstBridge), lastDecoration(&last, &lastBridge);
        Material::configureWindowState(&firstDecoration, true);
        Material::configureWindowState(&lastDecoration, true);
        first.map();
        last.map();
        first.changeIdentity("desktopFileName", app);
        last.changeIdentity("desktopFileName", app);
        const auto key = Material::windowStateKey(&first);
        const auto scope = Material::WindowStateScope::self()->current();
        QCOMPARE(Material::windowStateKey(&last), key);
        QVERIFY(Material::readWindowState(key, scope).isEmpty());
        first.setProperty("frameGeometry", QRectF(300, 200, 800, 500));
        last.setProperty("frameGeometry", QRectF(500, 350, 800, 500));
        first.close();
        last.close();
        QCOMPARE(Material::readWindowState(key, scope).value("position").toMap().value("x").toInt(), 500);
    }
    void previousConfigurationPlacementFollowsIdentity_data() {
        QTest::addColumn<bool>("changeBeforeMove");
        QTest::newRow("new-identity-before-user-move") << true;
        QTest::newRow("new-identity-after-user-move") << false;
    }
    void previousConfigurationPlacementFollowsIdentity() {
        QFETCH(bool, changeBeforeMove);
        ScopeWorkspace workspace;
        QObject interface(&workspace), a, b, desktop;
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerObject("/KWin", &interface));
        QScopedValueRollback wrapper(testWorkspace, static_cast<QObject *>(&workspace));
        QScopedValueRollback available(immediateAvailable, true);
        a.setProperty("name", "A"); a.setProperty("geometry", QRect(0, 0, 1280, 800));
        b.setProperty("name", "B"); b.setProperty("geometry", QRect(1280, 0, 1280, 800));
        desktop.setProperty("id", "desktop");
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a, &b}));
        workspace.setProperty("desktops", QVariant::fromValue(QList<QObject *>{&desktop}));
        KWin::WaylandWindow window, future;
        const QString app = "org.example.StateTest.PreviousIdentity." + QUuid::createUuid().toString();
        setup(window, app);
        setup(future, app + ".new");
        window.setProperty("output", QVariant::fromValue(&b));
        window.setProperty("frameGeometry", QRectF(1400, 90, 800, 500));
        future.setProperty("output", QVariant::fromValue(&b));
        future.setProperty("frameGeometry", QRectF(1500, 100, 800, 500));
        const auto key = Material::windowStateKey(&window);
        const auto newKey = Material::windowStateKey(&future);
        const auto ab = Material::WindowStateScope::self()->current();
        QVERIFY(Material::writeWindowState(newKey, Material::captureWindowState(&future), ab));
        const auto protectedState = Material::readWindowState(newKey, ab);
        MaterialTest::Bridge bridge;
        StateDecoration decoration(&window, &bridge);
        Material::configureWindowState(&decoration, true);
        window.map();
        workspace.setProperty("screens", QVariant::fromValue(QList<QObject *>{&a}));
        workspace.removeOutput(&b);
        window.setProperty("output", QVariant::fromValue(&a));
        window.setProperty("frameGeometry", QRectF(120, 90, 800, 500));
        workspace.finish();
        if (changeBeforeMove) window.changeIdentity("desktopFileName", app + ".new");
        window.startInteraction();
        window.setProperty("frameGeometry", QRectF(300, 200, 800, 500));
        window.stepInteraction();
        window.finishInteraction();
        if (!changeBeforeMove) window.changeIdentity("desktopFileName", app + ".new");
        window.close();
        const auto saved = Material::readWindowState(newKey, ab);
        if (changeBeforeMove) {
            QCOMPARE(saved.value("outputName").toString(), "A");
            QCOMPARE(saved.value("position").toMap().value("x").toInt(), 300);
        } else QCOMPARE(saved, protectedState);
        QCOMPARE(Material::readWindowState(key, ab).value("outputName").toString(), "B");
        bus.unregisterObject("/KWin");
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
        Material::configureWindowState(&decoration, true, true, true);
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
        Material::configureWindowState(&reopenedDecoration, true, true, true);
        reopened.map();
        QCOMPARE(immediateRequests.last().value("outputName").toString(), "B");
        QCOMPARE(immediateRequests.last().value("position").toMap().value("x").toInt(), 1390);
        testWorkspace = nullptr;
        bus.unregisterObject("/KWin");
    }
};

QTEST_MAIN(WindowStateTest)
#include "WindowStateTest.moc"
