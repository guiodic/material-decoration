#pragma once

#include "ExceptionList.h"
#include <KSharedConfig>
#include <QObject>

namespace Material
{

class Decoration;

class SettingsProvider : public QObject
{
    Q_OBJECT

public:
    ~SettingsProvider() override;

    static SettingsProvider *self();

    InternalSettingsPtr internalSettings(Decoration *decoration) const;

public Q_SLOTS:
    void reconfigure();

private:
    SettingsProvider();

    InternalSettingsPtr m_defaultSettings;
    InternalSettingsList m_exceptions;
    KSharedConfigPtr m_config;

    static SettingsProvider *s_self;
};

} // namespace Material
