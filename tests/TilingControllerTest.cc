// SPDX-License-Identifier: GPL-2.0-or-later
#include "TilingController.h"
#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QSet>
#include <QTest>
#include <QTimer>
#include <QUuid>
#include <QVariantMap>

class ScriptAction : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kglobalaccel.Component")
public:
    QList<QVariantMap> requests;
    bool respond = true;
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
            message << reply.value().value(QStringLiteral("id")).toString() << true << QString();
            QDBusConnection::sessionBus().asyncCall(message);
        });
    }
};

class TilingControllerTest : public QObject {
    Q_OBJECT
    ScriptAction action;
    QObject window;
private slots:
    void initTestCase() {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerService(QStringLiteral("org.kde.kglobalaccel")));
        QVERIFY(bus.registerObject(QStringLiteral("/component/kwin"), &action, QDBusConnection::ExportAllSlots));
        window.setProperty("internalId", QUuid::createUuid());
        window.setProperty("resizeable", true);
    }
    void identifiesOwnerAndRejectsInvalidTargets() {
        QObject valid(&window);
        QVERIFY(Material::canTileWindow(&valid));
        QObject unknown(nullptr);
        QVERIFY(!Material::canTileWindow(&unknown));
        QVERIFY(!Material::tileWindow(&valid, Qt::LeftEdge | Qt::RightEdge));
        QVERIFY(!Material::tileWindow(&unknown, Qt::LeftEdge));
        window.setProperty("resizeable", false);
        QVERIFY(!Material::canTileWindow(&valid));
        window.setProperty("resizeable", true);
    }
    void correlatesAndSerializesRequests() {
        QObject valid(&window);
        action.requests.clear();
        int replies = 0;
        bool allSucceeded = true;
        for (int i = 0; i < 3; ++i) {
            Material::checkTilingScript(&valid, [&](bool success) { ++replies; allSucceeded &= success; });
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
    void timesOutAndRecovers() {
        QObject valid(&window);
        action.respond = false;
        bool finished = false;
        bool succeeded = true;
        Material::checkTilingScript(&valid, [&](bool result) { succeeded = result; finished = true; });
        QTRY_VERIFY_WITH_TIMEOUT(finished, 3000);
        QVERIFY(!succeeded);
        action.respond = true;
        finished = false;
        Material::checkTilingScript(&valid, [&](bool result) { succeeded = result; finished = true; });
        QTRY_VERIFY(finished);
        QVERIFY(succeeded);
    }
    void deletedDecorationDoesNotReceiveCallback() {
        auto *decoration = new QObject(&window);
        bool called = false;
        Material::checkTilingScript(decoration, [&](bool) { called = true; });
        delete decoration;
        QTest::qWait(100);
        QVERIFY(!called);
    }
};
QTEST_MAIN(TilingControllerTest)
#include "TilingControllerTest.moc"
