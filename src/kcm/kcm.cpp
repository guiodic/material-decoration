#include "kcm.h"

#include <KPluginMetaData>
#include <QObject>

#include "../InternalSettings.h"

#include <ui_config.h>

#include <KPluginFactory>
#include <KSharedConfig>
#include <kconfiggroup.h>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

K_PLUGIN_CLASS_WITH_JSON(MaterialDecorationKCM, "materialdecoration_kcm.json")

MaterialDecorationKCM::MaterialDecorationKCM(QObject *parent, const KPluginMetaData &data)
    : KCModule(qobject_cast<QWidget*>(parent), data)
{
    m_ui = new Ui::Config();
    m_ui->setupUi(widget());

    // Populate combo boxes
    m_ui->kcfg_TitleAlignment->addItems({QStringLiteral("Left"),
                                         QStringLiteral("Center"),
                                         QStringLiteral("Center (Full Width)"),
                                         QStringLiteral("Right"),
                                         QStringLiteral("Hidden")});

    m_ui->kcfg_ButtonSize->addItems({QStringLiteral("Tiny"),
                                     QStringLiteral("Small"),
                                     QStringLiteral("Default"),
                                     QStringLiteral("Large"),
                                     QStringLiteral("Very Large")});

    m_ui->kcfg_ShadowSize->addItems({QStringLiteral("None"),
                                     QStringLiteral("Small"),
                                     QStringLiteral("Medium"),
                                     QStringLiteral("Large"),
                                     QStringLiteral("Very Large")});
    
    connect(m_ui->kcfg_ShadowStrength, &QSlider::valueChanged, m_ui->spinShadowStrength, &QSpinBox::setValue);
    connect(m_ui->spinShadowStrength, qOverload<int>(&QSpinBox::valueChanged), m_ui->kcfg_ShadowStrength, &QSlider::setValue);

    setupConnections();
}

MaterialDecorationKCM::~MaterialDecorationKCM()
{
    delete m_ui;
}

void MaterialDecorationKCM::setupConnections()
{
    connect(m_ui->kcfg_TitleAlignment, &QComboBox::currentIndexChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ButtonSize, &QComboBox::currentIndexChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ActiveOpacity, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_InactiveOpacity, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_MenuAlwaysShow, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_SearchEnabled, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_HamburgerMenu, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ShowDisabledActions, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_MenuButtonHorzPadding, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ShadowSize, &QComboBox::currentIndexChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ShadowColor, &KColorButton::changed, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ShadowStrength, &QSlider::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_AnimationsEnabled, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_AnimationsDuration, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
}

void MaterialDecorationKCM::load()
{
    KCModule::load();
    Material::InternalSettings settings;
    m_ui->kcfg_TitleAlignment->setCurrentIndex(settings.titleAlignment());
    m_ui->kcfg_ButtonSize->setCurrentIndex(settings.buttonSize());
    m_ui->kcfg_ActiveOpacity->setValue(qRound(settings.activeOpacity() * 100));
    m_ui->kcfg_InactiveOpacity->setValue(qRound(settings.inactiveOpacity() * 100));
    m_ui->kcfg_MenuAlwaysShow->setChecked(settings.menuAlwaysShow());
    m_ui->kcfg_SearchEnabled->setChecked(settings.searchEnabled());
    m_ui->kcfg_HamburgerMenu->setChecked(settings.hamburgerMenu());
    m_ui->kcfg_ShowDisabledActions->setChecked(settings.showDisabledActions());
    m_ui->kcfg_MenuButtonHorzPadding->setValue(settings.menuButtonHorzPadding());
    m_ui->kcfg_ShadowSize->setCurrentIndex(settings.shadowSize());
    m_ui->kcfg_ShadowColor->setColor(settings.shadowColor());
    m_ui->kcfg_ShadowStrength->setValue(settings.shadowStrength());
    m_ui->kcfg_AnimationsEnabled->setChecked(settings.animationsEnabled());
    m_ui->kcfg_AnimationsDuration->setValue(settings.animationsDuration());
}

void MaterialDecorationKCM::save()
{
    KCModule::save();
    Material::InternalSettings settings;
    settings.setTitleAlignment(m_ui->kcfg_TitleAlignment->currentIndex());
    settings.setButtonSize(m_ui->kcfg_ButtonSize->currentIndex());
    settings.setActiveOpacity(static_cast<double>(m_ui->kcfg_ActiveOpacity->value()) / 100.0);
    settings.setInactiveOpacity(static_cast<double>(m_ui->kcfg_InactiveOpacity->value()) / 100.0);
    settings.setMenuAlwaysShow(m_ui->kcfg_MenuAlwaysShow->isChecked());
    settings.setSearchEnabled(m_ui->kcfg_SearchEnabled->isChecked());
    settings.setHamburgerMenu(m_ui->kcfg_HamburgerMenu->isChecked());
    settings.setShowDisabledActions(m_ui->kcfg_ShowDisabledActions->isChecked());
    settings.setMenuButtonHorzPadding(m_ui->kcfg_MenuButtonHorzPadding->value());
    settings.setShadowSize(m_ui->kcfg_ShadowSize->currentIndex());
    settings.setShadowColor(m_ui->kcfg_ShadowColor->color());
    settings.setShadowStrength(m_ui->kcfg_ShadowStrength->value());
    settings.setAnimationsEnabled(m_ui->kcfg_AnimationsEnabled->isChecked());
    settings.setAnimationsDuration(m_ui->kcfg_AnimationsDuration->value());
}

void MaterialDecorationKCM::defaults()
{
    KCModule::defaults();
    Material::InternalSettings s;
    m_ui->kcfg_TitleAlignment->setCurrentIndex(s.titleAlignment());
    m_ui->kcfg_ButtonSize->setCurrentIndex(s.buttonSize());
    m_ui->kcfg_ActiveOpacity->setValue(qRound(s.activeOpacity() * 100));
    m_ui->kcfg_InactiveOpacity->setValue(qRound(s.inactiveOpacity() * 100));
    m_ui->kcfg_MenuAlwaysShow->setChecked(s.menuAlwaysShow());
    m_ui->kcfg_SearchEnabled->setChecked(s.searchEnabled());
    m_ui->kcfg_HamburgerMenu->setChecked(s.hamburgerMenu());
    m_ui->kcfg_ShowDisabledActions->setChecked(s.showDisabledActions());
    m_ui->kcfg_MenuButtonHorzPadding->setValue(s.menuButtonHorzPadding());
    m_ui->kcfg_ShadowSize->setCurrentIndex(s.shadowSize());
    m_ui->kcfg_ShadowColor->setColor(s.shadowColor());
    m_ui->kcfg_ShadowStrength->setValue(s.shadowStrength());
    m_ui->kcfg_AnimationsEnabled->setChecked(s.animationsEnabled());
    m_ui->kcfg_AnimationsDuration->setValue(s.animationsDuration());
    markAsChanged();
}

void MaterialDecorationKCM::updateChanged()
{
    markAsChanged();
}

#include "kcm.moc"
