#include "ExceptionList.h"
#include <KConfigGroup>
#include <utility>

namespace Material
{

ExceptionList::ExceptionList(const InternalSettingsList &exceptions)
    : m_exceptions(exceptions)
{
}

void ExceptionList::readConfig(KSharedConfig::Ptr config)
{
    m_exceptions.clear();

    QString groupName;
    for (int index = 0; config->hasGroup(groupName = exceptionGroupName(index)); ++index) {
        InternalSettings exception;
        readConfig(&exception, config.data(), groupName);

        InternalSettingsPtr configuration(new InternalSettings());
        configuration->load();

        configuration->setEnabled(exception.enabled());
        configuration->setExceptionType(exception.exceptionType());
        configuration->setExceptionPattern(exception.exceptionPattern());
        configuration->setMask(exception.mask());
        configuration->setHideTitleBar(exception.hideTitleBar());

        if (exception.mask() & TitleAlignment) {
            configuration->setTitleAlignment(exception.titleAlignment());
        }
        if (exception.mask() & ButtonSize) {
            configuration->setButtonSize(exception.buttonSize());
        }
        if (exception.mask() & CornerRadius) {
            configuration->setCornerRadius(exception.cornerRadius());
        }
        if (exception.mask() & Opacity) {
            configuration->setActiveOpacity(exception.activeOpacity());
            configuration->setInactiveOpacity(exception.inactiveOpacity());
        }
        if (exception.mask() & OutlineActive) {
            configuration->setOutlineActive(exception.outlineActive());
        }
        if (exception.mask() & ShadowSize) {
            configuration->setShadowSize(exception.shadowSize());
        }
        if (exception.mask() & MenuAlwaysShow) {
            configuration->setMenuAlwaysShow(exception.menuAlwaysShow());
        }

        m_exceptions.append(configuration);
    }
}

void ExceptionList::writeConfig(KSharedConfig::Ptr config)
{
    QString groupName;
    for (int index = 0; config->hasGroup(groupName = exceptionGroupName(index)); ++index) {
        config->deleteGroup(groupName);
    }

    int index = 0;
    for (const InternalSettingsPtr &exception : std::as_const(m_exceptions)) {
        writeConfig(exception.data(), config.data(), exceptionGroupName(index));
        ++index;
    }
}

QString ExceptionList::exceptionGroupName(int index)
{
    return QStringLiteral("Windeco Exception %1").arg(index);
}

void ExceptionList::writeConfig(KCoreConfigSkeleton *skeleton, KConfig *config, const QString &groupName)
{
    const QStringList keys = {
        QStringLiteral("Enabled"),
        QStringLiteral("ExceptionPattern"),
        QStringLiteral("ExceptionType"),
        QStringLiteral("Mask"),
        QStringLiteral("HideTitleBar"),
        QStringLiteral("TitleAlignment"),
        QStringLiteral("ButtonSize"),
        QStringLiteral("CornerRadius"),
        QStringLiteral("ActiveOpacity"),
        QStringLiteral("InactiveOpacity"),
        QStringLiteral("OutlineActive"),
        QStringLiteral("ShadowSize"),
        QStringLiteral("MenuAlwaysShow")
    };

    for (const auto &key : keys) {
        KConfigSkeletonItem *item = skeleton->findItem(key);
        if (!item) {
            continue;
        }

        if (!groupName.isEmpty()) {
            item->setGroup(groupName);
        }
        KConfigGroup configGroup(config, item->group());
        configGroup.writeEntry(item->key(), item->property());
    }
}

void ExceptionList::readConfig(KCoreConfigSkeleton *skeleton, KConfig *config, const QString &groupName)
{
    const auto items = skeleton->items();
    for (KConfigSkeletonItem *item : items) {
        if (!groupName.isEmpty()) {
            item->setGroup(groupName);
        }
        item->readConfig(config);
    }
}

} // namespace Material
