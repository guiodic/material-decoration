// SPDX-License-Identifier: GPL-2.0-or-later
#include "WindowState.h"
#include "InternalSettings.h"
#include "ExceptionList.h"
#include "DecorationTestBackend.h"
#include <KDecoration3/Decoration>
#include <QRectF>
#include <QStandardPaths>
#include <QTest>
#include <QUuid>
#include <functional>

// Match the runtime base-class name without including any private KWin header.
namespace KWin {
class WaylandWindow : public QObject {
    Q_OBJECT
public:
    void close() { Q_EMIT closed(); }
    void map() { Q_EMIT readyForPaintingChanged(); }
signals:
    void closed();
    void readyForPaintingChanged();
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
namespace Material {
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
        QCOMPARE(state.value("geometry").toMap().value("width").toDouble(), 800);
        QCOMPARE(state.value("desktopIds").toStringList(), QStringList{"desktop-uuid"});
        QCOMPARE(state.value("outputGeometry").toMap().value("x").toDouble(), 1920);
        const auto key = Material::windowStateKey(&window);
        QVERIFY(Material::writeWindowState(key, state));
        const auto saved = Material::readWindowState(key);
        QCOMPARE(saved.value("outputUuid").toString(), "monitor-uuid");
        QCOMPARE(saved.value("maximizeMode").toInt(), 3);
        QVERIFY(saved.value("fullScreen").toBool());
        QVERIFY(!Material::writeWindowState(key, {{"version", 1}, {"geometry", QVariantMap{{"width", -1}}}}));
        QCOMPARE(Material::readWindowState(key), saved);
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
        QCOMPARE(Material::readWindowState(key).value("geometry").toMap().value("x").toDouble(), 120);
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
};

QTEST_MAIN(WindowStateTest)
#include "WindowStateTest.moc"
