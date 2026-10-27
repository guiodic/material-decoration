// SPDX-License-Identifier: GPL-2.0-or-later
#include "ExceptionList.h"
#include "SettingsProvider.h"

#include <KConfigGroup>
#include <QTemporaryDir>
#include <QTest>

class MenuColorsSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAndOverrides()
    {
        auto defaults = Material::InternalSettingsPtr::create();
        defaults->setDefaults();
        QVERIFY(!defaults->menusFollowDecorationColors());
        auto exception = Material::InternalSettingsPtr::create();
        Material::SettingsProvider provider;

        for (bool enabled : {false, true}) {
            defaults->setMenusFollowDecorationColors(enabled);
            exception->setMask(Material::ExceptionMask::None);
            QCOMPARE(provider.createMergedSettings(defaults, exception)->menusFollowDecorationColors(), enabled);
            QCOMPARE(Material::cloneInternalSettings(defaults)->menusFollowDecorationColors(), enabled);

            exception->setMask(Material::ExceptionMask::MenusFollowDecorationColors);
            exception->setMenusFollowDecorationColors(!enabled);
            QCOMPARE(provider.createMergedSettings(defaults, exception)->menusFollowDecorationColors(), !enabled);
        }
    }

    void exceptionRoundTrip()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto config = KSharedConfig::openConfig(directory.filePath(QStringLiteral("exceptionsrc")), KConfig::SimpleConfig);
        for (bool enabled : {false, true}) {
            auto exception = Material::InternalSettingsPtr::create();
            exception->setExceptionPattern(QStringLiteral("test-window"));
            exception->setMask(Material::ExceptionMask::MenusFollowDecorationColors);
            exception->setMenusFollowDecorationColors(enabled);
            Material::ExceptionList written;
            written.setExceptions({exception});
            written.writeConfig(config);

            Material::ExceptionList loaded;
            loaded.readConfig(config);
            QCOMPARE(loaded.exceptions().size(), 1);
            QVERIFY(loaded.exceptions().first()->mask() & Material::ExceptionMask::MenusFollowDecorationColors);
            QCOMPARE(loaded.exceptions().first()->menusFollowDecorationColors(), enabled);
        }
    }

    void existingExceptionsInherit()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto config = KSharedConfig::openConfig(directory.filePath(QStringLiteral("exceptionsrc")), KConfig::SimpleConfig);
        config->group(QStringLiteral("Windeco Exception 0")).writeEntry("ExceptionPattern", "test-window");
        config->sync();
        Material::ExceptionList loaded;
        loaded.readConfig(config);
        QCOMPARE(loaded.exceptions().size(), 1);
        QVERIFY(!(loaded.exceptions().first()->mask() & Material::ExceptionMask::MenusFollowDecorationColors));
        auto defaults = Material::InternalSettingsPtr::create();
        defaults->setMenusFollowDecorationColors(true);
        Material::SettingsProvider provider;
        QVERIFY(provider.createMergedSettings(defaults, loaded.exceptions().first())->menusFollowDecorationColors());
    }
};

QTEST_MAIN(MenuColorsSettingsTest)
#include "MenuColorsSettingsTest.moc"
