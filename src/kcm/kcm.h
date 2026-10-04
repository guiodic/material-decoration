#pragma once

#include <KCModule>
#include <memory>

class KConfig;
class QWidget;
class QObject;
class KPluginMetaData;

namespace Ui
{
class Config;
}

namespace Material
{
class InternalSettings;
}

/**
 * @brief KDE Control Module (KCM) plugin for configuring Material window decoration settings.
 *
 * Implements KCModule to present UI configuration pages for title bar height, button padding,
 * corner rounding, shadow toggle, app menu search, color scheme, and window exception rules.
 */
class MaterialDecorationKCM : public KCModule
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a MaterialDecorationKCM instance.
     * @param parent Parent object.
     * @param data Plugin metadata.
     */
    explicit MaterialDecorationKCM(QObject *parent, const KPluginMetaData &data);

    /**
     * @brief Destructor.
     */
    ~MaterialDecorationKCM() override;

    /**
     * @brief Loads configuration settings from disk into UI widgets.
     */
    void load() override;

    /**
     * @brief Saves UI widget settings to configuration file on disk.
     */
    void save() override;

    /**
     * @brief Resets UI widget controls to default values.
     */
    void defaults() override;

private:
    /**
     * @brief Checks UI control state against saved configuration and updates KCModule changed status.
     */
    void updateChanged();

private:
    std::unique_ptr<Ui::Config> m_ui;
    std::unique_ptr<Material::InternalSettings> m_settings;

    void setupConnections();
    bool isChanged() const;
    void updateUI();
};
