#include "kcm.h"

#include <KPluginMetaData>
#include <QObject>

#include "../InternalSettings.h"

#include <ui_config.h>

#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <kconfiggroup.h>

#include <QDBusConnection>
#include <QDBusMessage>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

K_PLUGIN_CLASS_WITH_JSON(MaterialDecorationKCM, "materialdecoration_kcm.json")

MaterialDecorationKCM::MaterialDecorationKCM(QObject *parent, const KPluginMetaData &data)
    : KCModule(qobject_cast<QWidget*>(parent), data)
{
    m_settings = new Material::InternalSettings();
    m_ui = new Ui::Config();
    m_ui->setupUi(widget());

    // Populate combo boxes
    m_ui->kcfg_TitleAlignment->addItems({i18n("Left"),
                                         i18n("Center"),
                                         i18n("Center (Full Width)"),
                                         i18n("Right"),
                                         i18n("Hidden")});

    m_ui->kcfg_ButtonSize->addItems({i18n("Tiny"),
                                     i18n("Small"),
                                     i18n("Default"),
                                     i18n("Large"),
                                     i18n("Very Large")});

    m_ui->kcfg_ShadowSize->addItems({i18n("None"),
                                     i18n("Small"),
                                     i18n("Medium"),
                                     i18n("Large"),
                                     i18n("Very Large")});
    
    connect(m_ui->kcfg_ShadowStrength, &QSlider::valueChanged, m_ui->spinShadowStrength, &QSpinBox::setValue);
    connect(m_ui->spinShadowStrength, qOverload<int>(&QSpinBox::valueChanged), m_ui->kcfg_ShadowStrength, &QSlider::setValue);

    setupConnections();
}

MaterialDecorationKCM::~MaterialDecorationKCM()
{
    delete m_settings;
    delete m_ui;
}

void MaterialDecorationKCM::setupConnections()
{
    connect(m_ui->kcfg_TitleAlignment, &QComboBox::currentIndexChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ButtonSize, &QComboBox::currentIndexChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ActiveOpacity, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_InactiveOpacity, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_CornerRadius, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_MenuAlwaysShow, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_SearchEnabled, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_SearchEnabled, &QCheckBox::toggled, m_ui->kcfg_ShowDisabledActions, &QCheckBox::setEnabled);
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
    m_ui->kcfg_TitleAlignment->setCurrentIndex(m_settings->titleAlignment());
    m_ui->kcfg_ButtonSize->setCurrentIndex(m_settings->buttonSize());
    m_ui->kcfg_ActiveOpacity->setValue(qRound(m_settings->activeOpacity() * 100));
    m_ui->kcfg_InactiveOpacity->setValue(qRound(m_settings->inactiveOpacity() * 100));
    m_ui->kcfg_CornerRadius->setValue(m_settings->cornerRadius());
    m_ui->kcfg_MenuAlwaysShow->setChecked(m_settings->menuAlwaysShow());
    m_ui->kcfg_SearchEnabled->setChecked(m_settings->searchEnabled());
    m_ui->kcfg_ShowDisabledActions->setEnabled(m_settings->searchEnabled());
    m_ui->kcfg_HamburgerMenu->setChecked(m_settings->hamburgerMenu());
    m_ui->kcfg_ShowDisabledActions->setChecked(m_settings->showDisabledActions());
    m_ui->kcfg_MenuButtonHorzPadding->setValue(m_settings->menuButtonHorzPadding());
    m_ui->kcfg_ShadowSize->setCurrentIndex(m_settings->shadowSize());
    m_ui->kcfg_ShadowColor->setColor(m_settings->shadowColor());
    m_ui->kcfg_ShadowStrength->setValue(m_settings->shadowStrength());
    m_ui->kcfg_AnimationsEnabled->setChecked(m_settings->animationsEnabled());
    m_ui->kcfg_AnimationsDuration->setValue(m_settings->animationsDuration());
}

void MaterialDecorationKCM::save()
{
    m_settings->setTitleAlignment(m_ui->kcfg_TitleAlignment->currentIndex());
    m_settings->setButtonSize(m_ui->kcfg_ButtonSize->currentIndex());
    m_settings->setActiveOpacity(static_cast<double>(m_ui->kcfg_ActiveOpacity->value()) / 100.0);
    m_settings->setInactiveOpacity(static_cast<double>(m_ui->kcfg_InactiveOpacity->value()) / 100.0);
    m_settings->setCornerRadius(m_ui->kcfg_CornerRadius->value());
    m_settings->setMenuAlwaysShow(m_ui->kcfg_MenuAlwaysShow->isChecked());
    m_settings->setSearchEnabled(m_ui->kcfg_SearchEnabled->isChecked());
    m_settings->setHamburgerMenu(m_ui->kcfg_HamburgerMenu->isChecked());
    m_settings->setShowDisabledActions(m_ui->kcfg_ShowDisabledActions->isChecked());
    m_settings->setMenuButtonHorzPadding(m_ui->kcfg_MenuButtonHorzPadding->value());
    m_settings->setShadowSize(m_ui->kcfg_ShadowSize->currentIndex());
    m_settings->setShadowColor(m_ui->kcfg_ShadowColor->color());
    m_settings->setShadowStrength(m_ui->kcfg_ShadowStrength->value());
    m_settings->setAnimationsEnabled(m_ui->kcfg_AnimationsEnabled->isChecked());
    m_settings->setAnimationsDuration(m_ui->kcfg_AnimationsDuration->value());

    m_settings->save();
    QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                                     QStringLiteral("/KWin"),
                                                                     QStringLiteral("org.kde.KWin"),
                                                                     QStringLiteral("reconfigure")));
}

void MaterialDecorationKCM::defaults()
{
    m_settings->setDefaults();
    m_ui->kcfg_TitleAlignment->setCurrentIndex(m_settings->titleAlignment());
    m_ui->kcfg_ButtonSize->setCurrentIndex(m_settings->buttonSize());
    m_ui->kcfg_ActiveOpacity->setValue(qRound(m_settings->activeOpacity() * 100));
    m_ui->kcfg_InactiveOpacity->setValue(qRound(m_settings->inactiveOpacity() * 100));
    m_ui->kcfg_CornerRadius->setValue(m_settings->cornerRadius());
    m_ui->kcfg_MenuAlwaysShow->setChecked(m_settings->menuAlwaysShow());
    m_ui->kcfg_SearchEnabled->setChecked(m_settings->searchEnabled());
    m_ui->kcfg_HamburgerMenu->setChecked(m_settings->hamburgerMenu());
    m_ui->kcfg_ShowDisabledActions->setChecked(m_settings->showDisabledActions());
    m_ui->kcfg_MenuButtonHorzPadding->setValue(m_settings->menuButtonHorzPadding());
    m_ui->kcfg_ShadowSize->setCurrentIndex(m_settings->shadowSize());
    m_ui->kcfg_ShadowColor->setColor(m_settings->shadowColor());
    m_ui->kcfg_ShadowStrength->setValue(m_settings->shadowStrength());
    m_ui->kcfg_AnimationsEnabled->setChecked(m_settings->animationsEnabled());
    m_ui->kcfg_AnimationsDuration->setValue(m_settings->animationsDuration());
    markAsChanged();
}

void MaterialDecorationKCM::updateChanged()
{
    markAsChanged();
}

#include "kcm.moc"
