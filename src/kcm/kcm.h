#pragma once

#include <KCModule>

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

private Q_SLOTS:
    void updateChanged();

private:
    Ui::Config *m_ui = nullptr;
    Material::InternalSettings *m_settings = nullptr;

    void setupConnections();
    bool isChanged() const;
    void updateUI();
};
