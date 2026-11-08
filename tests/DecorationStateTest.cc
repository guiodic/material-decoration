// SPDX-License-Identifier: GPL-2.0-or-later
#include "DecorationTestBackend.h"

#include <array>

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
#include <QSignalSpy>
#include <QTest>
#include <QMenu>

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

    void shadowReuse_data()
    {
        QTest::addColumn<qreal>("scale");
        QTest::addColumn<int>("radius");
        for (qreal scale : {1.0, 1.25}) {
            for (int radius : {0, 6, 16}) {
                QTest::newRow(qPrintable(QStringLiteral("scale-%1-radius-%2").arg(scale).arg(radius))) << scale << radius;
            }
        }
    }

    void shadowReuse()
    {
        QFETCH(qreal, scale);
        QFETCH(int, radius);
        auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
        auto group = config->group(QStringLiteral("Windeco"));
        group.writeEntry("CornerRadius", radius);
        group.writeEntry("SquareCornersTiled", true);
        group.writeEntry("ShadowSize", 2);
        auto exception = config->group(QStringLiteral("Windeco Exception 0"));
        exception.writeEntry("ExceptionPattern", "Square window");
        exception.writeEntry("ExceptionType", 0);
        exception.writeEntry("Mask", 1 << 4); // SquareCorners
        exception.writeEntry("SquareCorners", true);
        QVERIFY(config->sync());

        MaterialTest::Bridge bridge;
        auto decoration = create(bridge);
        QVERIFY(decoration);
        bridge.client->outputScale = scale;
        decoration->create();
        decoration->setSettings(std::make_shared<KDecoration3::DecorationSettings>(&bridge));
        QVERIFY(decoration->init());
        Q_EMIT decoration->settings()->reconfigured();
        decoration->apply(decoration->nextState()->clone());
        const auto shadow = decoration->shadow();
        QVERIFY(shadow);
        const auto rounded = decoration->currentState()->borderRadius();
        std::shared_ptr<KDecoration3::DecorationShadow> squareShadow;
        const auto checkSquare = [&] {
            decoration->apply(decoration->nextState()->clone());
            QVERIFY(decoration->currentState()->borderRadius() == KDecoration3::BorderRadius());
            QVERIFY(decoration->shadow());
            if (squareShadow) {
                QCOMPARE(decoration->shadow(), squareShadow);
            } else {
                squareShadow = decoration->shadow();
            }
            QCOMPARE(squareShadow == shadow, radius == 0);
        };
        bridge.client->screenEdges = Qt::LeftEdge | Qt::TopEdge | Qt::BottomEdge;
        Q_EMIT decoration->window()->adjacentScreenEdgesChanged(bridge.client->screenEdges);
        checkSquare();
        bridge.client->screenEdges = {};
        Q_EMIT decoration->window()->adjacentScreenEdgesChanged({});
        QCOMPARE(decoration->shadow(), shadow);
        bridge.client->maximizedHorizontal = true;
        bridge.client->maximizedVertical = true;
        Q_EMIT decoration->window()->maximizedChanged(true);
        checkSquare();
        bridge.client->maximizedHorizontal = false;
        bridge.client->maximizedVertical = false;
        Q_EMIT decoration->window()->maximizedChanged(false);
        QCOMPARE(decoration->shadow(), shadow);
        bridge.client->captionText = QStringLiteral("Square window");
        Q_EMIT decoration->window()->captionChanged(bridge.client->captionText);
        checkSquare();
        bridge.client->captionText.clear();
        Q_EMIT decoration->window()->captionChanged(bridge.client->captionText);
        decoration->apply(decoration->nextState()->clone());
        QVERIFY(decoration->currentState()->borderRadius() == rounded);
        QCOMPARE(decoration->shadow(), shadow);
        group.writeEntry("CornerRadius", radius == 16 ? 6 : 16);
        QVERIFY(config->sync());
        Q_EMIT decoration->settings()->reconfigured();
        QVERIFY(decoration->shadow() != shadow);
        group.writeEntry("CornerRadius", radius);
        QVERIFY(config->sync());
        Q_EMIT decoration->settings()->reconfigured();
        QCOMPARE(decoration->shadow(), shadow);
        config->deleteGroup(QStringLiteral("Windeco Exception 0"));
        QVERIFY(config->sync());
    }

    void shadowCacheEviction()
    {
        auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
        auto group = config->group(QStringLiteral("Windeco"));
        MaterialTest::Bridge bridge;
        auto decoration = create(bridge);
        QVERIFY(decoration);
        decoration->create();
        decoration->setSettings(std::make_shared<KDecoration3::DecorationSettings>(&bridge));
        QVERIFY(decoration->init());
        const auto select = [&](int variant, int preset = 2) {
            group.writeEntry("ShadowSize", preset);
            group.writeEntry("ShadowColor", QColor(40 + variant, 50, 60));
            QVERIFY(config->sync());
            Q_EMIT decoration->settings()->reconfigured();
        };
        std::array<std::shared_ptr<KDecoration3::DecorationShadow>, 8> shadows;
        for (int i = 0; i < int(shadows.size()); ++i) {
            select(i);
            shadows[i] = decoration->shadow();
            QVERIFY(shadows[i]);
        }

        // Disabled shadows must not occupy entries or evict real textures.
        for (int i = 0; i < 9; ++i) {
            select(i, 0);
            QVERIFY(!decoration->shadow());
        }
        select(0);
        QCOMPARE(decoration->shadow(), shadows[0]);

        // Reusing the oldest entry makes variant 1 the eviction candidate.
        select(8);
        QVERIFY(decoration->shadow());
        select(0);
        QCOMPARE(decoration->shadow(), shadows[0]);
        for (int i = 2; i < int(shadows.size()); ++i) {
            select(i);
            QCOMPARE(decoration->shadow(), shadows[i]);
        }
        select(1);
        QVERIFY(decoration->shadow());
        QVERIFY(decoration->shadow() != shadows[1]);
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

    void queuedButtonLayout()
    {
        MaterialTest::Bridge bridge;
        auto decoration = create(bridge);
        QVERIFY(decoration);
        decoration->create();
        decoration->setSettings(std::make_shared<KDecoration3::DecorationSettings>(&bridge));
        bridge.options->rightButtons = {KDecoration3::DecorationButtonType::Minimize,
            KDecoration3::DecorationButtonType::Maximize, KDecoration3::DecorationButtonType::Close};
        QVERIFY(decoration->init());
        decoration->apply(decoration->nextState()->clone());
        for (int i = 0; i < 3; ++i) QCoreApplication::processEvents();

        const auto buttons = decoration->findChildren<KDecoration3::DecorationButton *>();
        QCOMPARE(buttons.size(), 3);
        QSignalSpy damage(decoration.get(), &KDecoration3::Decoration::damaged);
        const auto notifyGeometry = [&] {
            for (int i = 0; i < 20; ++i) {
                auto *button = buttons.at(i % buttons.size());
                Q_EMIT button->geometryChanged(button->geometry());
            }
            damage.clear(); // Ignore each button's immediate damage notification.
        };
        notifyGeometry();
        QCoreApplication::processEvents();
        QCOMPARE(damage.count(), 1);
        QCoreApplication::processEvents();
        QCOMPARE(damage.count(), 1);

        notifyGeometry();
        // A synchronous layout must consume the pending request as well.
        Q_EMIT decoration->window()->widthChanged(decoration->window()->width());
        QCOMPARE(damage.count(), 1);
        QCoreApplication::processEvents();
        QCOMPARE(damage.count(), 1);

        const auto resizeWindow = [&](qreal width) {
            bridge.client->windowSize.setWidth(width);
            Q_EMIT decoration->window()->sizeChanged(bridge.client->windowSize);
            Q_EMIT decoration->window()->widthChanged(width);
        };
        const auto previousGeometry = buttons.first()->geometry();
        resizeWindow(800);
        QVERIFY(buttons.first()->geometry() != previousGeometry);
        const auto damageAfterResize = damage.count();
        QCoreApplication::processEvents();
        QCOMPARE(damage.count(), damageAfterResize);

        // A size change arriving during layout must still request a follow-up.
        auto *target = buttons.last();
        const QSizeF originalSize = target->size();
        bool changedSize = false;
        const auto connection = connect(buttons.first(), &KDecoration3::DecorationButton::geometryChanged,
            decoration.get(), [&](const QRectF &) {
                if (!changedSize) {
                    changedSize = true;
                    auto geometry = target->geometry();
                    geometry.setWidth(geometry.width() + 5);
                    target->setGeometry(geometry);
                }
            });
        resizeWindow(900);
        QVERIFY(changedSize);
        QTRY_COMPARE(target->size(), originalSize);
        disconnect(connection);
        for (int i = 0; i < 3; ++i) QCoreApplication::processEvents();

        notifyGeometry();
        decoration.reset();
        QCoreApplication::processEvents(); // Destruction cancels queued layout work.
    }

    void hamburgerIcon()
    {
        auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
        auto group = config->group(QStringLiteral("Windeco"));
        group.writeEntry("HamburgerMenu", true);
        group.writeEntry("MenuAlwaysShow", true);
        group.writeEntry("SearchEnabled", false);
        group.writeEntry("AnimationsEnabled", false);
        QVERIFY(config->sync());
        MaterialTest::Bridge bridge;
        auto decoration = create(bridge);
        QVERIFY(decoration);
        bridge.client->applicationMenuAvailable = true;
        decoration->create();
        decoration->setSettings(std::make_shared<KDecoration3::DecorationSettings>(&bridge));
        QVERIFY(decoration->init());
        Q_EMIT decoration->settings()->reconfigured();
        decoration->apply(decoration->nextState()->clone());

        QObject *model = nullptr;
        for (auto *object : decoration->findChildren<QObject *>()) {
            if (qstrcmp(object->metaObject()->className(), "Material::AppMenuModel") == 0) model = object;
        }
        QVERIFY(model);
        QMenu menu;
        for (const auto &title : {"File", "Edit", "View", "Tools", "Settings", "Help"}) {
            menu.addMenu(QString::fromLatin1(title))->addAction(QStringLiteral("Action"));
        }
        // Supply a menu to the real model without requiring an external D-Bus exporter.
        QVERIFY(QMetaObject::invokeMethod(model, "onMenuUpdated", Qt::DirectConnection, Q_ARG(QMenu *, &menu)));
        const auto findOverflow = [&]() -> KDecoration3::DecorationButton * {
            for (auto *button : decoration->findChildren<KDecoration3::DecorationButton *>()) {
                if (qstrcmp(button->metaObject()->className(), "Material::MenuOverflowButton") == 0) return button;
            }
            return nullptr;
        };
        QTRY_VERIFY(findOverflow());
        auto *overflow = findOverflow();
        QTRY_VERIFY(overflow->isVisible());
        QVERIFY(overflow->setProperty("opacity", 1.0));
        for (int i = 0; i < 3; ++i) QCoreApplication::processEvents();

        QImage image(qCeil(decoration->rect().width()), qCeil(decoration->rect().height()),
                     QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        {
            QPainter painter(&image);
            painter.setPen(Qt::NoPen);
            overflow->paint(&painter, decoration->rect());
            QCOMPARE(painter.pen().style(), Qt::NoPen);
        }
        // An idle hamburger has no background: its three strokes must be visible.
        int paintedRows = 0;
        for (int y = 0; y < image.height(); ++y) {
            bool painted = false;
            for (int x = 0; x < image.width(); ++x) painted |= qAlpha(image.pixel(x, y)) > 0;
            paintedRows += painted;
        }
        QVERIFY(paintedRows >= 3);

        // The overflow button must still react to real menu-layout changes.
        group.writeEntry("HamburgerMenu", false);
        QVERIFY(config->sync());
        Q_EMIT decoration->settings()->reconfigured();
        const auto resizeWindow = [&](qreal width) {
            bridge.client->windowSize.setWidth(width);
            Q_EMIT decoration->window()->sizeChanged(bridge.client->windowSize);
            Q_EMIT decoration->window()->widthChanged(width);
        };
        resizeWindow(150);
        QTRY_VERIFY(overflow->isVisible());
        resizeWindow(900);
        QTRY_VERIFY(!overflow->isVisible());
        resizeWindow(150);
        QTRY_VERIFY(overflow->isVisible());
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
