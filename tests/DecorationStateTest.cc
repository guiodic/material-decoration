// SPDX-License-Identifier: GPL-2.0-or-later
#include "DecorationTestBackend.h"

#include <KConfigGroup>
#include <KDecoration3/DecorationSettings>
#include <KDecoration3/DecorationButton>
#include <KDecoration3/ScaleHelpers>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QApplication>
#include <QPainter>
#include <QHoverEvent>
#include <QPluginLoader>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QTest>

class DecorationStateTest : public QObject
{
    Q_OBJECT
    QPluginLoader m_loader;
    KPluginFactory *m_factory = nullptr;

    std::unique_ptr<KDecoration3::Decoration> create(MaterialTest::Bridge &bridge)
    {
        return std::unique_ptr<KDecoration3::Decoration>(m_factory->create<KDecoration3::Decoration>(nullptr,
            {QVariantMap{{QStringLiteral("bridge"), QVariant::fromValue<KDecoration3::DecorationBridge *>(&bridge)}}}));
    }

private slots:
    void initTestCase()
    {
        m_loader.setFileName(QString::fromUtf8(MATERIAL_DECORATION_PLUGIN));
        m_factory = qobject_cast<KPluginFactory *>(m_loader.instance());
        QVERIFY2(m_factory, qPrintable(m_loader.errorString()));
    }

    void init()
    {
        auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
        config->deleteGroup(QStringLiteral("Windeco"));
        QVERIFY(config->sync());
    }

    void resizeBorders_data()
    {
        QTest::addColumn<KDecoration3::BorderSize>("mode");
        QTest::addColumn<bool>("sides");
        QTest::addColumn<bool>("bottom");
        using KDecoration3::BorderSize;
        QTest::newRow("none") << BorderSize::None << true << true;
        QTest::newRow("no-sides") << BorderSize::NoSides << true << false;
        QTest::newRow("tiny") << BorderSize::Tiny << false << false;
        QTest::newRow("normal") << BorderSize::Normal << false << false;
        QTest::newRow("large") << BorderSize::Large << false << false;
    }

    void resizeBorders()
    {
        QFETCH(KDecoration3::BorderSize, mode);
        QFETCH(bool, sides);
        QFETCH(bool, bottom);
        MaterialTest::Bridge bridge;
        auto decoration = create(bridge);
        QVERIFY(decoration);
        decoration->create();
        decoration->setSettings(std::make_shared<KDecoration3::DecorationSettings>(&bridge));
        QVERIFY(decoration->init());
        Q_EMIT decoration->settings()->reconfigured();
        bridge.options->borderMode = mode;
        Q_EMIT decoration->settings()->borderSizeChanged(mode);
        auto *client = bridge.client;
        auto *window = decoration->window();
        for (qreal scale : {1.0, 1.25, 1.3}) {
            client->outputScale = scale;
            Q_EMIT window->nextScaleChanged();
            const qreal extent = KDecoration3::snapToPixelGrid(decoration->settings()->largeSpacing(), scale);
            const qreal side = sides ? extent : 0;
            const qreal lower = bottom ? extent : 0;
            const QMarginsF expected(side, extent, side, lower);
            QCOMPARE(decoration->resizeOnlyBorders(), expected);
            client->screenEdges = Qt::LeftEdge | Qt::TopEdge | Qt::RightEdge | Qt::BottomEdge;
            Q_EMIT window->adjacentScreenEdgesChanged(client->screenEdges);
            QCOMPARE(decoration->resizeOnlyBorders(), expected);
            client->maximizedHorizontal = true;
            Q_EMIT window->maximizedHorizontallyChanged(true);
            QCOMPARE(decoration->resizeOnlyBorders(), QMarginsF(0, extent, 0, lower));
            client->maximizedVertical = true;
            Q_EMIT window->maximizedVerticallyChanged(true);
            QCOMPARE(decoration->resizeOnlyBorders(), QMarginsF());
            client->maximizedHorizontal = false;
            Q_EMIT window->maximizedHorizontallyChanged(false);
            QCOMPARE(decoration->resizeOnlyBorders(), QMarginsF(side, 0, side, 0));
            client->maximizedVertical = false;
            Q_EMIT window->maximizedVerticallyChanged(false);
            client->setResizeable(false);
            QCOMPARE(decoration->resizeOnlyBorders(), QMarginsF());
            client->setResizeable(true);
            QCOMPARE(decoration->resizeOnlyBorders(), expected);
            // KWin also uses maximizeableChanged when fixed-size constraints change.
            client->resizeable = false;
            Q_EMIT window->maximizeableChanged(false);
            QCOMPARE(decoration->resizeOnlyBorders(), QMarginsF());
            client->resizeable = true;
            Q_EMIT window->maximizeableChanged(true);
            QCOMPARE(decoration->resizeOnlyBorders(), expected);
        }
    }

    void buttonRepaints_data()
    {
        QTest::addColumn<qreal>("scale");
        QTest::addColumn<bool>("rtl");
        QTest::addColumn<bool>("gtk");
        for (qreal scale : {1.0, 1.25, 1.5, 2.0}) {
            for (bool rtl : {false, true}) {
                for (bool gtk : {false, true}) {
                    const auto name = QStringLiteral("%1-%2-%3")
                        .arg(scale).arg(rtl ? "rtl" : "ltr").arg(gtk ? "gtk" : "native");
                    QTest::newRow(qPrintable(name)) << scale << rtl << gtk;
                }
            }
        }
    }

    void buttonRepaints()
    {
        QFETCH(qreal, scale);
        QFETCH(bool, rtl);
        QFETCH(bool, gtk);
        auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
        config->group(QStringLiteral("Windeco")).writeEntry("AnimationsEnabled", false);
        QVERIFY(config->sync());

        const auto oldName = QCoreApplication::applicationName();
        const auto oldDirection = QApplication::layoutDirection();
        const auto restoreApplication = qScopeGuard([&] {
            QCoreApplication::setApplicationName(oldName);
            QApplication::setLayoutDirection(oldDirection);
        });
        QCoreApplication::setApplicationName(gtk ? QStringLiteral("kded6") : oldName);
        QApplication::setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
        MaterialTest::Bridge bridge;
        auto decoration = create(bridge);
        QVERIFY(decoration);
        bridge.client->outputScale = scale;
        decoration->create();
        decoration->setSettings(std::make_shared<KDecoration3::DecorationSettings>(&bridge));
        bridge.options->borderMode = KDecoration3::BorderSize::None;
        bridge.options->rightButtons = {KDecoration3::DecorationButtonType::Minimize,
            KDecoration3::DecorationButtonType::Maximize, KDecoration3::DecorationButtonType::Close};
        QVERIFY(decoration->init());
        Q_EMIT decoration->settings()->reconfigured();
        decoration->apply(decoration->nextState()->clone());
        QCoreApplication::processEvents();
        decoration->apply(decoration->nextState()->clone());
        QCoreApplication::processEvents();

        const auto buttons = decoration->findChildren<KDecoration3::DecorationButton *>();
        QCOMPARE(buttons.size(), 3);
        QImage untouched(qCeil(decoration->rect().width() * scale),
                         qCeil(decoration->rect().height() * scale), QImage::Format_ARGB32_Premultiplied);
        untouched.setDevicePixelRatio(scale);
        untouched.fill(QColor(25, 51, 77));
        const QRectF fullRegion = decoration->rect();
        const QRectF outside(-10000, -10000, 1, 1);
        for (auto *button : buttons) {
            const auto geometry = button->geometry();
            QHoverEvent hover(QEvent::HoverEnter, geometry.center(), geometry.center(), QPointF(-1, -1));
            QCoreApplication::sendEvent(button, &hover);
            QVERIFY(button->isHovered());
            const auto render = [&](const QRectF &region, bool clip = false) {
                QImage image = untouched;
                QPainter painter(&image);
                if (clip) painter.setClipRect(region);
                button->paint(&painter, region);
                return image;
            };
            const auto full = render(fullRegion);
            QVERIFY(full != untouched);
            QCOMPARE(render(QRectF()), full);
            QCOMPARE(render(outside), gtk ? full : untouched);
            QVERIFY(button->setProperty("opacity", 0.0));
            QCOMPARE(render(fullRegion), untouched);
            QVERIFY(button->setProperty("opacity", 1.0));

            // Partial damage, including fractional strips at both button edges.
            for (const QRectF &damage : {geometry.adjusted(1, 1, -1, -1),
                     QRectF(geometry.left() - 0.75 / scale, geometry.top(), 1.0 / scale, geometry.height()),
                     QRectF(geometry.right() - 0.25 / scale, geometry.top(), 1.0 / scale, geometry.height())}) {
                QImage expected = untouched;
                {
                    QPainter painter(&expected);
                    painter.setClipRect(damage);
                    painter.setCompositionMode(QPainter::CompositionMode_Source);
                    painter.drawImage(QPointF(0, 0), full);
                }
                QCOMPARE(render(damage, true), expected);
            }
        }
    }

    void opaqueState()
    {
        MaterialTest::Bridge bridge;
        auto decoration = create(bridge);
        QVERIFY(decoration);
        decoration->create();
        decoration->setSettings(std::make_shared<KDecoration3::DecorationSettings>(&bridge));
        QVERIFY(decoration->init());
        Q_EMIT decoration->settings()->reconfigured();
        const auto apply = [&] { decoration->apply(decoration->nextState()->clone()); };
        auto *client = bridge.client;
        auto *window = decoration->window();
        const auto setting = [&](const char *key, const QVariant &value) {
            auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
            config->group(QStringLiteral("Windeco")).writeEntry(key, value);
            QVERIFY(config->sync());
            Q_EMIT decoration->settings()->reconfigured();
        };
        apply();
        QVERIFY(!decoration->isOpaque());
        client->maximizedHorizontal = client->maximizedVertical = true;
        Q_EMIT window->maximizedChanged(true);
        QVERIFY(!decoration->isOpaque());
        apply();
        QVERIFY(decoration->isOpaque());
        for (qreal scale : {1.0, 1.25, 1.5, 2.0}) {
            client->outputScale = scale;
            Q_EMIT window->nextScaleChanged();
            apply();
            QVERIFY(decoration->isOpaque());
            QImage image(qRound(decoration->rect().width() * scale),
                         qRound(decoration->borders().top() * scale), QImage::Format_ARGB32_Premultiplied);
            QVERIFY(!image.isNull());
            image.setDevicePixelRatio(scale);
            image.fill(Qt::transparent);
            {
                QPainter painter(&image);
                decoration->paint(&painter, QRectF(0, 0, image.width() / scale, image.height() / scale));
            }
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    QCOMPARE(qAlpha(image.pixel(x, y)), 255);
                }
            }
        }
        setting("ActiveOpacity", 0.6);
        QVERIFY(!decoration->isOpaque());
        setting("ActiveOpacity", 1.0);
        QVERIFY(decoration->isOpaque());
        setting("InactiveOpacity", 0.5);
        client->active = false;
        Q_EMIT window->activeChanged(false);
        QVERIFY(!decoration->isOpaque());
        client->active = true;
        Q_EMIT window->activeChanged(true);
        QVERIFY(decoration->isOpaque());
        setting("HideTitleBar", true);
        apply();
        QVERIFY(!decoration->isOpaque());
        setting("HideTitleBar", false);
        apply();
        QVERIFY(decoration->isOpaque());
        client->shaded = true;
        Q_EMIT window->shadedChanged(true);
        QVERIFY(!decoration->isOpaque());
        client->shaded = false;
        Q_EMIT window->shadedChanged(false);
        apply();
        QVERIFY(decoration->isOpaque());
        client->maximizedHorizontal = client->maximizedVertical = false;
        Q_EMIT window->maximizedChanged(false);
        QVERIFY(!decoration->isOpaque());
        apply();
        QVERIFY(!decoration->isOpaque());
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir config;
    if (!config.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QApplication app(argc, argv);
    DecorationStateTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "DecorationStateTest.moc"
