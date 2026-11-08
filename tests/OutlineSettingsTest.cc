// SPDX-License-Identifier: GPL-2.0-or-later
#include "ExceptionList.h"
#include "SettingsProvider.h"

#include <KConfigGroup>
#include <QTemporaryDir>
#include <QTest>

class OutlineSettingsTest : public QObject
{
    Q_OBJECT
private slots:
    void defaultsAndInheritance()
    {
        auto defaults = Material::InternalSettingsPtr::create();
        defaults->setDefaults();
        QVERIFY(!defaults->activeBorderUsesAccent());
        auto exception = Material::InternalSettingsPtr::create();
        exception->setMask(Material::ExceptionMask::OutlineActive);
        exception->setOutlineActive(true);
        Material::SettingsProvider provider;
        for (bool enabled : {false, true}) {
            defaults->setActiveBorderUsesAccent(enabled);
            exception->setActiveBorderUsesAccent(!enabled);
            QCOMPARE(Material::cloneInternalSettings(defaults)->activeBorderUsesAccent(), enabled);
            const auto merged = provider.createMergedSettings(defaults, exception);
            QVERIFY(merged->outlineActive());
            QCOMPARE(merged->activeBorderUsesAccent(), enabled);
        }
    }

    void persistence()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto config = KSharedConfig::openConfig(directory.filePath(QStringLiteral("outlinerc")), KConfig::SimpleConfig);
        for (bool enabled : {true, false}) {
            Material::InternalSettings written;
            written.setSharedConfig(config);
            written.load();
            written.setActiveBorderUsesAccent(enabled);
            QVERIFY(written.save());
            QCOMPARE(config->group(QStringLiteral("Windeco")).readEntry("ActiveBorderUsesAccent", false), enabled);
            Material::InternalSettings loaded;
            loaded.setSharedConfig(config);
            loaded.load();
            QCOMPARE(loaded.activeBorderUsesAccent(), enabled);
        }
    }
};
QTEST_MAIN(OutlineSettingsTest)
#include "OutlineSettingsTest.moc"
