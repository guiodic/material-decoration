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

class MaterialDecorationKCM : public KCModule
{
    Q_OBJECT
public:
    explicit MaterialDecorationKCM(QObject *parent, const KPluginMetaData &data);
    ~MaterialDecorationKCM() override;

    void load() override;
    void save() override;
    void defaults() override;

private:
    void updateChanged();

private:
    std::unique_ptr<Ui::Config> m_ui;
    std::unique_ptr<Material::InternalSettings> m_settings;

    void setupConnections();
    bool isChanged() const;
    void updateUI();
};
