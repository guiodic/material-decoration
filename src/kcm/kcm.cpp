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
#include <QIcon>

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

    m_ui->longPressInfoButton->setIcon(QIcon::fromTheme(QStringLiteral("dialog-information")));

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
    connect(m_ui->kcfg_UseCustomBorderColors, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ActiveBorderColor, &KColorButton::changed, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_InactiveBorderColor, &KColorButton::changed, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_MenuAlwaysShow, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_SearchEnabled, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_SearchEnabled, &QCheckBox::toggled, m_ui->kcfg_ShowDisabledActions, &QCheckBox::setEnabled);

    const auto updateBorderStuff = [this] {
        const bool useCustom = m_ui->kcfg_UseCustomBorderColors->isChecked();
        m_ui->kcfg_ActiveBorderColor->setEnabled(useCustom);
        m_ui->kcfg_InactiveBorderColor->setEnabled(useCustom);
    };
    connect(m_ui->kcfg_UseCustomBorderColors, &QCheckBox::toggled, this, updateBorderStuff);

    connect(m_ui->kcfg_HamburgerMenu, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ShowDisabledActions, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_MenuButtonHorzPadding, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_UseSystemMenuFont, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ShadowSize, &QComboBox::currentIndexChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ShadowColor, &KColorButton::changed, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ShadowStrength, &QSlider::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_AnimationsEnabled, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_AnimationsDuration, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_BottomCorners, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_HideCaptionWhenLimitedSpace, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_HideCaptionWhenLimitedSpace, &QCheckBox::toggled, m_ui->kcfg_MinWidthForCaption, &QSpinBox::setEnabled);
    connect(m_ui->kcfg_HideCaptionWhenLimitedSpace, &QCheckBox::toggled, m_ui->label_minWidthForCaption, &QLabel::setEnabled);
    connect(m_ui->kcfg_MinWidthForCaption, qOverload<int>(&QSpinBox::valueChanged), this, &MaterialDecorationKCM::updateChanged);

    connect(m_ui->kcfg_LongPressEnabled, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_LongPressEnabled, &QCheckBox::toggled, m_ui->kcfg_LongPressDuration, &QSpinBox::setEnabled);
    connect(m_ui->kcfg_LongPressDuration, qOverload<int>(&QSpinBox::valueChanged), this, &MaterialDecorationKCM::updateChanged);
}

void MaterialDecorationKCM::load()
{
    m_ui->kcfg_TitleAlignment->setCurrentIndex(m_settings->titleAlignment());
    m_ui->kcfg_ButtonSize->setCurrentIndex(m_settings->buttonSize());
    m_ui->kcfg_ActiveOpacity->setValue(qRound(m_settings->activeOpacity() * 100));
    m_ui->kcfg_InactiveOpacity->setValue(qRound(m_settings->inactiveOpacity() * 100));
    m_ui->kcfg_CornerRadius->setValue(m_settings->cornerRadius());
    m_ui->kcfg_UseCustomBorderColors->setChecked(m_settings->useCustomBorderColors());
    m_ui->kcfg_ActiveBorderColor->setColor(m_settings->activeBorderColor());
    m_ui->kcfg_InactiveBorderColor->setColor(m_settings->inactiveBorderColor());
    const bool useCustom = m_ui->kcfg_UseCustomBorderColors->isChecked();
    m_ui->kcfg_ActiveBorderColor->setEnabled(useCustom);
    m_ui->kcfg_InactiveBorderColor->setEnabled(useCustom);    
    m_ui->kcfg_MenuAlwaysShow->setChecked(m_settings->menuAlwaysShow());
    m_ui->kcfg_SearchEnabled->setChecked(m_settings->searchEnabled());
    m_ui->kcfg_ShowDisabledActions->setEnabled(m_settings->searchEnabled());
    m_ui->kcfg_HamburgerMenu->setChecked(m_settings->hamburgerMenu());
    m_ui->kcfg_ShowDisabledActions->setChecked(m_settings->showDisabledActions());
    m_ui->kcfg_MenuButtonHorzPadding->setValue(m_settings->menuButtonHorzPadding());
    m_ui->kcfg_UseSystemMenuFont->setChecked(m_settings->useSystemMenuFont());
    m_ui->kcfg_ShadowSize->setCurrentIndex(m_settings->shadowSize());
    m_ui->kcfg_ShadowColor->setColor(m_settings->shadowColor());
    m_ui->kcfg_ShadowStrength->setValue(m_settings->shadowStrength());
    m_ui->kcfg_AnimationsEnabled->setChecked(m_settings->animationsEnabled());
    m_ui->kcfg_AnimationsDuration->setValue(m_settings->animationsDuration());
    m_ui->kcfg_BottomCorners->setChecked(m_settings->bottomCornerRadiusFlag());
    m_ui->kcfg_HideCaptionWhenLimitedSpace->setChecked(m_settings->hideCaptionWhenLimitedSpace());
    m_ui->kcfg_MinWidthForCaption->setValue(m_settings->minWidthForCaption());
    m_ui->kcfg_MinWidthForCaption->setEnabled(m_settings->hideCaptionWhenLimitedSpace());
    m_ui->label_minWidthForCaption->setEnabled(m_settings->hideCaptionWhenLimitedSpace());

    m_ui->kcfg_LongPressEnabled->setChecked(m_settings->longPressEnabled());
    m_ui->kcfg_LongPressDuration->setValue(m_settings->longPressDuration());
    m_ui->kcfg_LongPressDuration->setEnabled(m_settings->longPressEnabled());
}

void MaterialDecorationKCM::save()
{
    m_settings->setTitleAlignment(m_ui->kcfg_TitleAlignment->currentIndex());
    m_settings->setButtonSize(m_ui->kcfg_ButtonSize->currentIndex());
    m_settings->setActiveOpacity(static_cast<double>(m_ui->kcfg_ActiveOpacity->value()) / 100.0);
    m_settings->setInactiveOpacity(static_cast<double>(m_ui->kcfg_InactiveOpacity->value()) / 100.0);
    m_settings->setCornerRadius(m_ui->kcfg_CornerRadius->value());
    m_settings->setUseCustomBorderColors(m_ui->kcfg_UseCustomBorderColors->isChecked());
    m_settings->setActiveBorderColor(m_ui->kcfg_ActiveBorderColor->color());
    m_settings->setInactiveBorderColor(m_ui->kcfg_InactiveBorderColor->color());
    m_settings->setMenuAlwaysShow(m_ui->kcfg_MenuAlwaysShow->isChecked());
    m_settings->setSearchEnabled(m_ui->kcfg_SearchEnabled->isChecked());
    m_settings->setHamburgerMenu(m_ui->kcfg_HamburgerMenu->isChecked());
    m_settings->setShowDisabledActions(m_ui->kcfg_ShowDisabledActions->isChecked());
    m_settings->setMenuButtonHorzPadding(m_ui->kcfg_MenuButtonHorzPadding->value());
    m_settings->setUseSystemMenuFont(m_ui->kcfg_UseSystemMenuFont->isChecked());
    m_settings->setShadowSize(m_ui->kcfg_ShadowSize->currentIndex());
    m_settings->setShadowColor(m_ui->kcfg_ShadowColor->color());
    m_settings->setShadowStrength(m_ui->kcfg_ShadowStrength->value());
    m_settings->setAnimationsEnabled(m_ui->kcfg_AnimationsEnabled->isChecked());
    m_settings->setAnimationsDuration(m_ui->kcfg_AnimationsDuration->value());
    m_settings->setBottomCornerRadiusFlag(m_ui->kcfg_BottomCorners->isChecked());
    m_settings->setHideCaptionWhenLimitedSpace(m_ui->kcfg_HideCaptionWhenLimitedSpace->isChecked());
    m_settings->setMinWidthForCaption(m_ui->kcfg_MinWidthForCaption->value());

    m_settings->setLongPressEnabled(m_ui->kcfg_LongPressEnabled->isChecked());
    m_settings->setLongPressDuration(m_ui->kcfg_LongPressDuration->value());

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
    m_ui->kcfg_UseCustomBorderColors->setChecked(m_settings->useCustomBorderColors());
    m_ui->kcfg_ActiveBorderColor->setColor(m_settings->activeBorderColor());
    m_ui->kcfg_InactiveBorderColor->setColor(m_settings->inactiveBorderColor());
    const bool useCustom = m_ui->kcfg_UseCustomBorderColors->isChecked();
    m_ui->kcfg_ActiveBorderColor->setEnabled(useCustom);
    m_ui->kcfg_InactiveBorderColor->setEnabled(useCustom);
    m_ui->kcfg_MenuAlwaysShow->setChecked(m_settings->menuAlwaysShow());
    m_ui->kcfg_SearchEnabled->setChecked(m_settings->searchEnabled());
    m_ui->kcfg_HamburgerMenu->setChecked(m_settings->hamburgerMenu());
    m_ui->kcfg_ShowDisabledActions->setChecked(m_settings->showDisabledActions());
    m_ui->kcfg_MenuButtonHorzPadding->setValue(m_settings->menuButtonHorzPadding());
    m_ui->kcfg_UseSystemMenuFont->setChecked(m_settings->useSystemMenuFont());
    m_ui->kcfg_ShadowSize->setCurrentIndex(m_settings->shadowSize());
    m_ui->kcfg_ShadowColor->setColor(m_settings->shadowColor());
    m_ui->kcfg_ShadowStrength->setValue(m_settings->shadowStrength());
    m_ui->kcfg_AnimationsEnabled->setChecked(m_settings->animationsEnabled());
    m_ui->kcfg_AnimationsDuration->setValue(m_settings->animationsDuration());
    m_ui->kcfg_BottomCorners->setChecked(m_settings->bottomCornerRadiusFlag());
    m_ui->kcfg_HideCaptionWhenLimitedSpace->setChecked(m_settings->hideCaptionWhenLimitedSpace());
    m_ui->kcfg_MinWidthForCaption->setValue(m_settings->minWidthForCaption());
    m_ui->kcfg_MinWidthForCaption->setEnabled(m_settings->hideCaptionWhenLimitedSpace());
    m_ui->label_minWidthForCaption->setEnabled(m_settings->hideCaptionWhenLimitedSpace());

    m_ui->kcfg_LongPressEnabled->setChecked(m_settings->longPressEnabled());
    m_ui->kcfg_LongPressDuration->setValue(m_settings->longPressDuration());
    m_ui->kcfg_LongPressDuration->setEnabled(m_settings->longPressEnabled());

    markAsChanged();
}

void MaterialDecorationKCM::updateChanged()
{
    markAsChanged();
}

#include "kcm.moc"
