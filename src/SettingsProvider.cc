#include "SettingsProvider.h"
#include "Decoration.h"
#include <KDecoration3/DecoratedWindow>
#include <QRegularExpression>
#include <utility>

namespace Material
{

SettingsProvider *SettingsProvider::s_self = nullptr;

SettingsProvider::SettingsProvider()
    : m_config(KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc")))
{
    reconfigure();
}

SettingsProvider::~SettingsProvider()
{
    s_self = nullptr;
}

SettingsProvider *SettingsProvider::self()
{
    if (!s_self) {
        s_self = new SettingsProvider();
    }
    return s_self;
}

void SettingsProvider::reconfigure()
{
    if (!m_defaultSettings) {
        m_defaultSettings = InternalSettingsPtr(new InternalSettings());
        m_defaultSettings->setCurrentGroup(QStringLiteral("Windeco"));
    }

    m_config->reparseConfiguration();
    m_defaultSettings->load();

    ExceptionList exceptions;
    exceptions.readConfig(m_config);
    m_exceptions = exceptions.get();
}

InternalSettingsPtr SettingsProvider::internalSettings(Decoration *decoration) const
{
    if (!decoration || !decoration->window()) {
        return m_defaultSettings;
    }

    QString windowTitle;
    QString windowClass;

    for (const auto &internalSettings : std::as_const(m_exceptions)) {
        if (!internalSettings->enabled()) {
            continue;
        }

        if (internalSettings->exceptionPattern().isEmpty()) {
            continue;
        }

        QString value;
        switch (internalSettings->exceptionType()) {
        case InternalSettings::ExceptionWindowTitle: {
            value = windowTitle.isEmpty() ? (windowTitle = decoration->window()->caption()) : windowTitle;
            break;
        }
        default:
        case InternalSettings::ExceptionWindowClassName: {
            value = windowClass.isEmpty() ? (windowClass = decoration->window()->windowClass()) : windowClass;
            break;
        }
        }

        QRegularExpression rx(internalSettings->exceptionPattern());
        if (rx.match(value).hasMatch()) {
            return internalSettings;
        }
    }

    return m_defaultSettings;
}

} // namespace Material
