// SPDX-License-Identifier: GPL-2.0-or-later
#include "DecorationTestBackend.h"

#include <KConfigGroup>
#include <KDecoration3/DecorationButton>
#include <KDecoration3/DecorationSettings>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QApplication>
#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QPluginLoader>
#include <QSvgGenerator>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QTest>

class ButtonSvgTest : public QObject
{
    Q_OBJECT
    QPluginLoader m_loader;
    KPluginFactory *m_factory = nullptr;

private slots:
    void initTestCase()
    {
        auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
        config->group(QStringLiteral("Windeco")).writeEntry("AnimationsEnabled", false);
        QVERIFY(config->sync());
        m_loader.setFileName(QString::fromUtf8(MATERIAL_DECORATION_PLUGIN));
        m_factory = qobject_cast<KPluginFactory *>(m_loader.instance());
        QVERIFY2(m_factory, qPrintable(m_loader.errorString()));
    }

    void alignedStrokes_data()
    {
        QTest::addColumn<KDecoration3::DecorationButtonType>("type");
        QTest::addColumn<bool>("checked");
        QTest::addColumn<qreal>("buttonWidth");
        using Type = KDecoration3::DecorationButtonType;
        for (qreal width : {50.0, 48.0}) {
            QTest::addRow("minimize-%g", width) << Type::Minimize << false << width;
            QTest::addRow("maximize-%g", width) << Type::Maximize << false << width;
            QTest::addRow("restore-%g", width) << Type::Maximize << true << width;
        }
    }

    void alignedStrokes()
    {
        QFETCH(KDecoration3::DecorationButtonType, type);
        QFETCH(bool, checked);
        QFETCH(qreal, buttonWidth);
        MaterialTest::Bridge bridge;
        std::unique_ptr<KDecoration3::Decoration> decoration(m_factory->create<KDecoration3::Decoration>(nullptr,
            {QVariantMap{{QStringLiteral("bridge"), QVariant::fromValue<KDecoration3::DecorationBridge *>(&bridge)}}}));
        QVERIFY(decoration);
        decoration->create();
        decoration->setSettings(std::make_shared<KDecoration3::DecorationSettings>(&bridge));
        bridge.options->borderMode = KDecoration3::BorderSize::None;
        QVERIFY(decoration->init());

        // Match the KDE exporter: create the button through the plugin factory.
        std::unique_ptr<KDecoration3::DecorationButton> button(m_factory->create<KDecoration3::DecorationButton>(
            decoration.get(), {QVariant::fromValue(type), QVariant::fromValue(decoration.get())}));
        QVERIFY(button);
        button->setGeometry(QRectF(0, 0, buttonWidth, buttonWidth));
        button->setCheckable(true);
        button->setChecked(checked);

        QByteArray svg;
        QBuffer buffer(&svg);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QSvgGenerator generator;
        generator.setOutputDevice(&buffer);
        // The export canvas stays 50×50 even for a differently sized button.
        generator.setViewBox(QRect(0, 0, 50, 50));
        {
            QPainter painter(&generator);
            button->paint(&painter, QRectF(0, 0, 50, 50));
        }
        QSvgRenderer renderer(svg);
        QVERIFY(renderer.isValid());
        QImage image(18, 18, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        {
            QPainter painter(&image);
            renderer.render(&painter, QRectF(image.rect()));
        }
        int paintedPixels = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const int alpha = qAlpha(image.pixel(x, y));
                // Allow two alpha levels for SVG numeric serialization.
                QVERIFY2(alpha <= 2 || alpha >= 253,
                    qPrintable(QStringLiteral("SVG stroke misses pixel grid: (%1,%2), alpha=%3")
                        .arg(x).arg(y).arg(alpha)));
                paintedPixels += alpha >= 253;
            }
        }
        QVERIFY(paintedPixels > 0);
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir config;
    if (!config.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kded6"));
    ButtonSvgTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "ButtonSvgTest.moc"
