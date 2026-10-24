// SPDX-License-Identifier: GPL-2.0-or-later
#include "LongPressAction.h"
#include <QDBusConnection>
#include <QTest>

class ShortcutService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kglobalaccel.Component")
public:
    QStringList names;
public slots:
    QStringList shortcutNames() { return names; }
};

class ScriptService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.Scripting")
public:
    QString loaded;
public slots:
    bool isScriptLoaded(const QString &name) { return name == loaded; }
};

class LongPressActionTest : public QObject
{
    Q_OBJECT
    ShortcutService shortcuts;
    ScriptService scripts;
private slots:
    void initTestCase()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerService(QStringLiteral("org.kde.kglobalaccel")));
        QVERIFY(bus.registerService(QStringLiteral("org.kde.KWin")));
        QVERIFY(bus.registerObject(QStringLiteral("/component/kwin"), &shortcuts, QDBusConnection::ExportAllSlots));
        QVERIFY(bus.registerObject(QStringLiteral("/Scripting"), &scripts, QDBusConnection::ExportAllSlots));
    }
    void availability_data()
    {
        QTest::addColumn<bool>("registered");
        QTest::addColumn<bool>("requiresScript");
        QTest::addColumn<bool>("loaded");
        QTest::addColumn<bool>("expected");
        QTest::newRow("missing-native-action") << false << false << false << false;
        QTest::newRow("available-native-action") << true << false << false << true;
        QTest::newRow("stale-script-shortcut") << true << true << false << false;
        QTest::newRow("loaded-but-no-shortcut") << false << true << true << false;
        QTest::newRow("available-script") << true << true << true << true;
    }
    void availability()
    {
        QFETCH(bool, registered);
        QFETCH(bool, requiresScript);
        QFETCH(bool, loaded);
        QFETCH(bool, expected);
        shortcuts.names = registered ? QStringList{QStringLiteral("action")} : QStringList{};
        scripts.loaded = loaded ? QStringLiteral("script") : QString();
        bool finished = false;
        bool available = !expected;
        Material::checkLongPressAction(this, QStringLiteral("action"), requiresScript ? QStringLiteral("script") : QString(),
                                      [&](bool result) { available = result; finished = true; });
        QTRY_VERIFY(finished);
        QCOMPARE(available, expected);
    }
    void destroyedButtonDoesNotReceiveCallback()
    {
        auto *button = new QObject;
        bool called = false;
        Material::checkLongPressAction(button, QStringLiteral("action"), QStringLiteral("script"), [&](bool) { called = true; });
        delete button;
        QTest::qWait(50);
        QVERIFY(!called);
    }
    void unavailableService()
    {
        QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/component/kwin"));
        bool finished = false;
        bool available = true;
        Material::checkLongPressAction(this, QStringLiteral("action"), {}, [&](bool result) { available = result; finished = true; });
        QTRY_VERIFY(finished);
        QVERIFY(!available);
    }
};

QTEST_GUILESS_MAIN(LongPressActionTest)
#include "LongPressActionTest.moc"
