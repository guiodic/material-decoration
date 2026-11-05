// SPDX-License-Identifier: GPL-2.0-or-later
#include "DecorationTestBackend.h"
#include <KDecoration3/DecorationSettings>
#include <KPluginFactory>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QPluginLoader>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QDBusVirtualObject>
#include <QDBusVariant>

class FakeKWin : public QDBusVirtualObject {
public:
    explicit FakeKWin(QDBusConnection connection) : bus(connection) {}
    QDBusConnection bus;
    QDBusMessage pending;
    bool received = false;
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &) override {
        if (message.interface() == QStringLiteral("org.freedesktop.DBus.Properties") &&
            message.member() == QStringLiteral("Get") && message.arguments().size() == 2 &&
            message.arguments()[1].toString() == QStringLiteral("tabletMode")) {
            pending = message;
            received = true;
            return true; // Deliberately hold the reply until the test releases it.
        }
        bus.send(message.createErrorReply(QDBusError::UnknownMethod, QStringLiteral("Unexpected method")));
        return true;
    }
    void reply(bool value) {
        if (!bus.send(pending.createReply({QVariant::fromValue(QDBusVariant(value))}))) qFatal("Reply failed");
    }
    void signal(bool value) {
        auto message = QDBusMessage::createSignal(QStringLiteral("/org/kde/KWin"),
            QStringLiteral("org.kde.KWin.TabletModeManager"), QStringLiteral("tabletModeChanged"));
        message << value;
        if (!bus.send(message)) qFatal("Signal failed");
    }
};

class Observer : public QObject {
    Q_OBJECT
public slots:
    void receive(bool value) { Q_EMIT observed(value); }
signals:
    void observed(bool value);
};

class TabletReplyTest : public QObject {
    Q_OBJECT
    QPluginLoader plugin;
    KPluginFactory *factory = nullptr;
    QDBusConnection server = QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("fake-kwin"));
    FakeKWin kwin{server};
private slots:
    void initTestCase() {
        QCOMPARE(QGuiApplication::platformName(), QStringLiteral("wayland"));
        QVERIFY(server.registerService(QStringLiteral("org.kde.KWin")));
        QVERIFY(server.registerVirtualObject(QStringLiteral("/org/kde/KWin"), &kwin));
        plugin.setFileName(QString::fromUtf8(MATERIAL_DECORATION_PLUGIN));
        factory = qobject_cast<KPluginFactory *>(plugin.instance());
        QVERIFY2(factory, qPrintable(plugin.errorString()));
    }
    void ordering_data() {
        QTest::addColumn<bool>("signalFirst");
        QTest::addColumn<bool>("signalValue");
        QTest::newRow("reply-true-then-signal-false") << false << false;
        QTest::newRow("reply-false-then-signal-true") << false << true;
        QTest::newRow("signal-true-then-stale-reply-false") << true << true;
        QTest::newRow("signal-false-matches-default-then-stale-reply-true") << true << false;
    }
    void ordering() {
        QFETCH(bool, signalFirst);
        QFETCH(bool, signalValue);
        kwin.received = false;
        MaterialTest::Bridge bridge;
        std::unique_ptr<KDecoration3::Decoration> deco(factory->create<KDecoration3::Decoration>(nullptr,
            {QVariantMap{{QStringLiteral("bridge"), QVariant::fromValue<KDecoration3::DecorationBridge *>(&bridge)}}}));
        QVERIFY(deco);
        deco->create();
        deco->setSettings(std::make_shared<KDecoration3::DecorationSettings>(&bridge));
        QVERIFY(deco->init());
        deco->apply(deco->nextState()->clone());
        QTRY_VERIFY(kwin.received);
        auto *watcher = deco->findChild<QDBusPendingCallWatcher *>();
        QVERIFY(watcher);
        QSignalSpy replyHandled(watcher, &QDBusPendingCallWatcher::finished);
        Observer observer;
        QSignalSpy signalSpy(&observer, &Observer::observed);
        QVERIFY(QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.KWin"), QStringLiteral("/org/kde/KWin"),
            QStringLiteral("org.kde.KWin.TabletModeManager"), QStringLiteral("tabletModeChanged"),
            &observer, SLOT(receive(bool))));
        const qreal ext = deco->settings()->largeSpacing();
        const auto expected = [ext](bool mode) {
            const qreal size = ext * (mode ? 2 : 1);
            return mode ? QMarginsF(size,size,size,size) : QMarginsF(0,size,0,0);
        };
        if (signalFirst) {
            kwin.signal(signalValue);
            QTRY_COMPARE(signalSpy.count(), 1);
            QCOMPARE(deco->resizeOnlyBorders(), expected(signalValue));
            kwin.reply(!signalValue);
            QTRY_COMPARE(replyHandled.count(), 1);
        } else {
            kwin.reply(!signalValue);
            QTRY_COMPARE(replyHandled.count(), 1);
            QCOMPARE(deco->resizeOnlyBorders(), expected(!signalValue));
            kwin.signal(signalValue);
            QTRY_COMPARE(signalSpy.count(), 1);
        }
        QCOMPARE(deco->resizeOnlyBorders(), expected(signalValue));
    }
};
int main(int argc, char **argv)
{
    const auto display = qgetenv("MATERIAL_TEST_WAYLAND_DISPLAY");
    if (display.isEmpty()) {
        qInfo("Set MATERIAL_TEST_WAYLAND_DISPLAY to run this test with a Wayland compositor.");
        return 77;
    }
    QTemporaryDir config;
    if (!config.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    qputenv("WAYLAND_DISPLAY", display);
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);
    TabletReplyTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "TabletReplyTest.moc"
