// SPDX-License-Identifier: GPL-2.0-or-later
#include "DecorationTestBackend.h"

#include <KConfigGroup>
#include <KDecoration3/DecorationSettings>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QApplication>
#include <QPluginLoader>
#include <QTemporaryDir>
#include <QTest>

class OutlineColorTest : public QObject
{
    Q_OBJECT
    QPluginLoader m_loader;
    KPluginFactory *m_factory = nullptr;
private slots:
    void initTestCase()
    {
        m_loader.setFileName(QString::fromUtf8(MATERIAL_DECORATION_PLUGIN));
        m_factory = qobject_cast<KPluginFactory *>(m_loader.instance());
        QVERIFY2(m_factory, qPrintable(m_loader.errorString()));
    }

    void colorSelection_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<bool>("active");
        QTest::addColumn<bool>("accent");
        for (bool dark : {false, true}) {
            for (bool active : {false, true}) {
                for (bool accent : {false, true}) {
                    const auto name = QStringLiteral("%1-%2-%3")
                        .arg(dark ? "dark" : "light", active ? "active" : "inactive", accent ? "accent" : "frame");
                    QTest::newRow(qPrintable(name)) << dark << active << accent;
                }
            }
        }
    }

    void colorSelection()
    {
        QFETCH(bool, dark);
        QFETCH(bool, active);
        QFETCH(bool, accent);
        auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
        auto group = config->group(QStringLiteral("Windeco"));
        group.writeEntry("OutlineActive", true);
        group.writeEntry("ActiveOutlineUsesAccent", accent);
        group.writeEntry("UseCustomBorderColors", false);
        QVERIFY(config->sync());

        MaterialTest::Bridge bridge;
        std::unique_ptr<KDecoration3::Decoration> decoration(m_factory->create<KDecoration3::Decoration>(nullptr,
            {QVariantMap{{QStringLiteral("bridge"), QVariant::fromValue<KDecoration3::DecorationBridge *>(&bridge)}}}));
        QVERIFY(decoration);
        bridge.client->active = active;
        auto &palette = bridge.client->colors;
        palette.setColor(QPalette::Active, QPalette::Window, QColor(dark ? "#2a2e32" : "#eff0f1"));
        palette.setColor(QPalette::Inactive, QPalette::Window, QColor(dark ? "#232629" : "#dde0e4"));
        palette.setColor(QPalette::Active, QPalette::Highlight, QColor(dark ? "#a66eee" : "#3daee9"));
        palette.setColor(QPalette::Inactive, QPalette::Highlight, Qt::yellow);
        decoration->create();
        decoration->setSettings(std::make_shared<KDecoration3::DecorationSettings>(&bridge));
        QVERIFY(decoration->init());
        Q_EMIT decoration->settings()->reconfigured();

        const auto initial = decoration->nextState()->borderOutline();
        QVERIFY(!initial.isNull());
        const auto check = [&] {
            // The test compositor applies the state requested by the real plugin.
            decoration->apply(decoration->nextState()->clone());
            const auto outline = decoration->currentState()->borderOutline();
            const QColor expected = accent && bridge.client->active
                ? palette.color(QPalette::Active, QPalette::Highlight)
                : palette.color(bridge.client->active ? QPalette::Active : QPalette::Inactive, QPalette::Window);
            QCOMPARE(outline.color(), expected);
            QCOMPARE(outline.thickness(), initial.thickness());
            QVERIFY(outline.radius() == initial.radius());
        };
        check();
        bridge.client->active = !active;
        Q_EMIT decoration->window()->activeChanged(bridge.client->active);
        check();
        palette.setColor(QPalette::Active, QPalette::Highlight, Qt::green);
        Q_EMIT decoration->window()->paletteChanged(palette);
        check();
        accent = !accent;
        group.writeEntry("ActiveOutlineUsesAccent", accent);
        QVERIFY(config->sync());
        Q_EMIT decoration->settings()->reconfigured();
        check();
        group.writeEntry("OutlineActive", false);
        QVERIFY(config->sync());
        Q_EMIT decoration->settings()->reconfigured();
        QVERIFY(decoration->nextState()->borderOutline().isNull());
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir config;
    if (!config.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QApplication app(argc, argv);
    OutlineColorTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "OutlineColorTest.moc"
