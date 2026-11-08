// SPDX-License-Identifier: GPL-2.0-or-later
#include "DecorationTestBackend.h"

#include <KConfigGroup>
#include <KDecoration3/DecorationSettings>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QApplication>
#include <QImage>
#include <QPainter>
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
        QTest::addColumn<bool>("customBorder");
        QTest::addColumn<bool>("borderAccent");
        for (bool dark : {false, true}) {
            for (bool active : {false, true}) {
                for (bool accent : {false, true}) {
                    const auto name = QStringLiteral("%1-%2-%3")
                        .arg(dark ? "dark" : "light", active ? "active" : "inactive", accent ? "accent" : "frame");
                    for (int variant = 0; variant < 4; ++variant) {
                        const auto row = name + QStringLiteral("-border-%1").arg(variant);
                        QTest::newRow(qPrintable(row)) << dark << active << accent << bool(variant & 1) << bool(variant & 2);
                    }
                }
            }
        }
    }

    void colorSelection()
    {
        QFETCH(bool, dark);
        QFETCH(bool, active);
        QFETCH(bool, accent);
        QFETCH(bool, customBorder);
        QFETCH(bool, borderAccent);
        auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
        auto group = config->group(QStringLiteral("Windeco"));
        group.writeEntry("AnimationsEnabled", false);
        group.writeEntry("ActiveOpacity", 1.0);
        group.writeEntry("InactiveOpacity", 1.0);
        group.writeEntry("OutlineActive", true);
        group.writeEntry("ActiveOutlineUsesAccent", accent);
        group.writeEntry("UseCustomBorderColors", customBorder);
        group.writeEntry("ActiveBorderUsesAccent", borderAccent);
        group.writeEntry("ActiveBorderColor", QColor("#cc3344"));
        group.writeEntry("InactiveBorderColor", QColor("#667788"));
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
            const QColor frame = customBorder
                ? (bridge.client->active
                    ? (borderAccent ? palette.color(QPalette::Active, QPalette::Highlight) : QColor("#cc3344"))
                    : QColor("#667788"))
                : palette.color(bridge.client->active ? QPalette::Active : QPalette::Inactive, QPalette::Window);
            const QColor expected = accent && bridge.client->active
                ? palette.color(QPalette::Active, QPalette::Highlight)
                : frame;
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

        // Exercise the real paint path independently of the compositor outline.
        const auto checkPaintedBorder = [&] {
            decoration->apply(decoration->nextState()->clone());
            QVERIFY(decoration->borderLeft() >= 2);
            QImage image(decoration->size().toSize(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            decoration->paint(&painter, decoration->rect());
            painter.end();
            const QColor expected = customBorder
                ? (bridge.client->active
                    ? (borderAccent ? palette.color(QPalette::Active, QPalette::Highlight) : QColor("#cc3344"))
                    : QColor("#667788"))
                : palette.color(bridge.client->active ? QPalette::Active : QPalette::Inactive, QPalette::Window);
            QCOMPARE(image.pixelColor(int(decoration->borderLeft() / 2), image.height() / 2), expected);
        };
        checkPaintedBorder();
        bridge.client->active = !bridge.client->active;
        Q_EMIT decoration->window()->activeChanged(bridge.client->active);
        checkPaintedBorder();
        palette.setColor(QPalette::Active, QPalette::Highlight, QColor("#ee9900"));
        Q_EMIT decoration->window()->paletteChanged(palette);
        checkPaintedBorder();
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
