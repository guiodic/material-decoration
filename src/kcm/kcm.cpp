#include "kcm.h"

#include <KPluginMetaData>
#include <QObject>

#include "../InternalSettings.h"
#include "../SettingsMigration.h"

#include <ui_config.h>

#include <KLocalizedString>
#include <KPluginFactory>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QIcon>
#include <QLabel>
#include <QTabBar>


K_PLUGIN_CLASS_WITH_JSON(MaterialDecorationKCM, "materialdecoration_kcm.json")

MaterialDecorationKCM::MaterialDecorationKCM(QObject *parent, const KPluginMetaData &data)
    : KCModule(qobject_cast<QWidget*>(parent), data)
    , m_ui(std::make_unique<Ui::Config>())
    , m_settings(std::make_unique<Material::InternalSettings>())
{
    m_ui->setupUi(widget());

    if (auto *bar = m_ui->tabWidget->findChild<QTabBar *>()) {
        bar->setExpanding(true);
    }

    // Populate combo boxes
    m_ui->kcfg_TitleAlignment->addItems({i18n("Left"),
                                         i18n("Center"),
                                         i18n("Center (Full Width)"),
                                         i18n("Right"),
                                         i18n("Hidden")});

    m_ui->kcfg_ButtonSize->addItems({i18n("Very Tiny"),
                                     i18n("Tiny"),
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

MaterialDecorationKCM::~MaterialDecorationKCM() = default;

void MaterialDecorationKCM::setupConnections()
{
    connect(m_ui->kcfg_TitleAlignment, &QComboBox::currentIndexChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ButtonSize, &QComboBox::currentIndexChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ActiveOpacity, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_InactiveOpacity, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_CornerRadius, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_OutlineActive, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_UseSystemColors, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_UseCustomBorderColors, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ActiveBorderColor, &KColorButton::changed, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_InactiveBorderColor, &KColorButton::changed, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_MenuAlwaysShow, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_MenuAlwaysShow, &QCheckBox::toggled, this, [this](bool checked) {
        if (!checked) {
            m_ui->kcfg_ShowCaptionOnHover->setChecked(false);
            m_ui->kcfg_ShowCaptionOnHover->setEnabled(false);
        } else {
            m_ui->kcfg_ShowCaptionOnHover->setEnabled(true);
        }
    });
    connect(m_ui->kcfg_SearchEnabled, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_SearchEnabled, &QCheckBox::toggled, m_ui->kcfg_ShowDisabledActions, &QWidget::setEnabled);
    connect(m_ui->kcfg_SearchEnabled, &QCheckBox::toggled, m_ui->kcfg_SearchIgnoreTopLevel, &QWidget::setEnabled);
    connect(m_ui->kcfg_SearchEnabled, &QCheckBox::toggled, m_ui->kcfg_SearchIgnoreSubMenus, &QWidget::setEnabled);
    connect(m_ui->kcfg_SearchEnabled, &QCheckBox::toggled, m_ui->kcfg_SearchFuzzyMatching, &QWidget::setEnabled);

    connect(m_ui->kcfg_SearchIgnoreSubMenus, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            m_ui->kcfg_SearchIgnoreTopLevel->setChecked(true);
        }
        updateChanged();
    });
    connect(m_ui->kcfg_SearchIgnoreTopLevel, &QCheckBox::toggled, this, [this](bool checked) {
        if (!checked) {
            m_ui->kcfg_SearchIgnoreSubMenus->setChecked(false);
        }
        updateChanged();
    });

    const auto updateBorderStuff = [this] {
        const bool useCustom = m_ui->kcfg_UseCustomBorderColors->isChecked();
        m_ui->kcfg_ActiveBorderColor->setEnabled(useCustom);
        m_ui->kcfg_InactiveBorderColor->setEnabled(useCustom);
    };
    connect(m_ui->kcfg_UseCustomBorderColors, &QCheckBox::toggled, this, updateBorderStuff);

    connect(m_ui->kcfg_HamburgerMenu, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ShowDisabledActions, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_SearchFuzzyMatching, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_MenuButtonHorzPadding, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_UseSystemMenuFont, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ShadowSize, &QComboBox::currentIndexChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ShadowColor, &KColorButton::changed, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ShadowStrength, &QSlider::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_AnimationsEnabled, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_AnimationsDuration, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_BottomCorners, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_SquareCornersTiled, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_HideCaptionWhenLimitedSpace, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_HideCaptionWhenLimitedSpace, &QCheckBox::toggled, m_ui->kcfg_MinWidthForCaption, &QWidget::setEnabled);
    connect(m_ui->kcfg_HideCaptionWhenLimitedSpace, &QCheckBox::toggled, m_ui->label_minWidthForCaption, &QWidget::setEnabled);
    connect(m_ui->kcfg_MinWidthForCaption, qOverload<int>(&QSpinBox::valueChanged), this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_ShowCaptionOnHover, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);

    connect(m_ui->kcfg_LongPressEnabled, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);
    connect(m_ui->kcfg_LongPressEnabled, &QCheckBox::toggled, m_ui->kcfg_LongPressDuration, &QWidget::setEnabled);
    connect(m_ui->kcfg_LongPressDuration, qOverload<int>(&QSpinBox::valueChanged), this, &MaterialDecorationKCM::updateChanged);

    connect(m_ui->kcfg_TilingPopupOpacity, &QSpinBox::valueChanged, this, &MaterialDecorationKCM::updateChanged);

    connect(m_ui->kcfg_DragFromButtonsEnabled, &QCheckBox::toggled, this, &MaterialDecorationKCM::updateChanged);

    connect(m_ui->exceptionsWidget, &Material::ExceptionListWidget::changed, this, &MaterialDecorationKCM::updateChanged);
}

void MaterialDecorationKCM::load()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
    config->reparseConfiguration();
    Material::migrateTilingPopupOpacity(*config);
    m_settings->load();
    m_ui->exceptionsWidget->load();
    updateUI();
    updateChanged();
}

void MaterialDecorationKCM::loadUIFromSettings(const Material::InternalSettings &settings)
{
    m_ui->kcfg_TitleAlignment->setCurrentIndex(settings.titleAlignment());
    m_ui->kcfg_ButtonSize->setCurrentIndex(settings.buttonSize());
    m_ui->kcfg_ActiveOpacity->setValue(qRound(settings.activeOpacity() * 100));
    m_ui->kcfg_InactiveOpacity->setValue(qRound(settings.inactiveOpacity() * 100));
    m_ui->kcfg_CornerRadius->setValue(settings.cornerRadius());
    m_ui->kcfg_UseSystemColors->setChecked(settings.useSystemColors());
    m_ui->kcfg_UseCustomBorderColors->setChecked(settings.useCustomBorderColors());
    m_ui->kcfg_ActiveBorderColor->setColor(settings.activeBorderColor());
    m_ui->kcfg_InactiveBorderColor->setColor(settings.inactiveBorderColor());
    const bool useCustom = m_ui->kcfg_UseCustomBorderColors->isChecked();
    m_ui->kcfg_ActiveBorderColor->setEnabled(useCustom);
    m_ui->kcfg_InactiveBorderColor->setEnabled(useCustom);
    m_ui->kcfg_MenuAlwaysShow->setChecked(settings.menuAlwaysShow());
    m_ui->kcfg_ShowCaptionOnHover->setChecked(settings.showCaptionOnHover());
    m_ui->kcfg_ShowCaptionOnHover->setEnabled(settings.menuAlwaysShow());
    m_ui->kcfg_SearchEnabled->setChecked(settings.searchEnabled());
    m_ui->kcfg_ShowDisabledActions->setEnabled(settings.searchEnabled());
    m_ui->kcfg_SearchIgnoreTopLevel->setEnabled(settings.searchEnabled());
    m_ui->kcfg_SearchIgnoreSubMenus->setEnabled(settings.searchEnabled());
    m_ui->kcfg_SearchFuzzyMatching->setEnabled(settings.searchEnabled());
    m_ui->kcfg_HamburgerMenu->setChecked(settings.hamburgerMenu());
    m_ui->kcfg_ShowDisabledActions->setChecked(settings.showDisabledActions());
    m_ui->kcfg_SearchIgnoreTopLevel->setChecked(settings.searchIgnoreTopLevel());
    m_ui->kcfg_SearchIgnoreSubMenus->setChecked(settings.searchIgnoreSubMenus());
    m_ui->kcfg_SearchFuzzyMatching->setChecked(settings.searchFuzzyMatching());
    m_ui->kcfg_MenuButtonHorzPadding->setValue(settings.menuButtonHorzPadding());
    m_ui->kcfg_UseSystemMenuFont->setChecked(settings.useSystemMenuFont());
    m_ui->kcfg_ShadowSize->setCurrentIndex(settings.shadowSize());
    m_ui->kcfg_ShadowColor->setColor(settings.shadowColor());
    m_ui->kcfg_ShadowStrength->setValue(settings.shadowStrength());
    m_ui->kcfg_AnimationsEnabled->setChecked(settings.animationsEnabled());
    m_ui->kcfg_AnimationsDuration->setValue(settings.animationsDuration());
    m_ui->kcfg_BottomCorners->setChecked(settings.bottomCornerRadiusFlag());
    m_ui->kcfg_SquareCornersTiled->setChecked(settings.squareCornersTiled());
    m_ui->kcfg_OutlineActive->setChecked(settings.outlineActive());
    m_ui->kcfg_HideCaptionWhenLimitedSpace->setChecked(settings.hideCaptionWhenLimitedSpace());
    m_ui->kcfg_MinWidthForCaption->setValue(settings.minWidthForCaption());
    m_ui->kcfg_MinWidthForCaption->setEnabled(settings.hideCaptionWhenLimitedSpace());
    m_ui->label_minWidthForCaption->setEnabled(settings.hideCaptionWhenLimitedSpace());

    m_ui->kcfg_LongPressEnabled->setChecked(settings.longPressEnabled());
    m_ui->kcfg_LongPressDuration->setValue(settings.longPressDuration());
    m_ui->kcfg_LongPressDuration->setEnabled(settings.longPressEnabled());

    m_ui->kcfg_TilingPopupOpacity->setValue(settings.tilingPopupOpacity());

    m_ui->kcfg_DragFromButtonsEnabled->setChecked(settings.dragFromButtonsEnabled());
}

void MaterialDecorationKCM::updateUI()
{
    loadUIFromSettings(*m_settings);
}

void MaterialDecorationKCM::save()
{
    m_ui->exceptionsWidget->save();

    m_settings->setTitleAlignment(m_ui->kcfg_TitleAlignment->currentIndex());
    m_settings->setButtonSize(m_ui->kcfg_ButtonSize->currentIndex());
    m_settings->setActiveOpacity(static_cast<double>(m_ui->kcfg_ActiveOpacity->value()) / 100.0);
    m_settings->setInactiveOpacity(static_cast<double>(m_ui->kcfg_InactiveOpacity->value()) / 100.0);
    m_settings->setCornerRadius(m_ui->kcfg_CornerRadius->value());
    m_settings->setUseSystemColors(m_ui->kcfg_UseSystemColors->isChecked());
    m_settings->setUseCustomBorderColors(m_ui->kcfg_UseCustomBorderColors->isChecked());
    m_settings->setActiveBorderColor(m_ui->kcfg_ActiveBorderColor->color());
    m_settings->setInactiveBorderColor(m_ui->kcfg_InactiveBorderColor->color());
    m_settings->setMenuAlwaysShow(m_ui->kcfg_MenuAlwaysShow->isChecked());
    m_settings->setShowCaptionOnHover(m_ui->kcfg_ShowCaptionOnHover->isChecked());
    m_settings->setSearchEnabled(m_ui->kcfg_SearchEnabled->isChecked());
    m_settings->setHamburgerMenu(m_ui->kcfg_HamburgerMenu->isChecked());
    m_settings->setShowDisabledActions(m_ui->kcfg_ShowDisabledActions->isChecked());
    m_settings->setSearchIgnoreTopLevel(m_ui->kcfg_SearchIgnoreTopLevel->isChecked());
    m_settings->setSearchIgnoreSubMenus(m_ui->kcfg_SearchIgnoreSubMenus->isChecked());
    m_settings->setSearchFuzzyMatching(m_ui->kcfg_SearchFuzzyMatching->isChecked());
    m_settings->setMenuButtonHorzPadding(m_ui->kcfg_MenuButtonHorzPadding->value());
    m_settings->setUseSystemMenuFont(m_ui->kcfg_UseSystemMenuFont->isChecked());
    m_settings->setShadowSize(m_ui->kcfg_ShadowSize->currentIndex());
    m_settings->setShadowColor(m_ui->kcfg_ShadowColor->color());
    m_settings->setShadowStrength(m_ui->kcfg_ShadowStrength->value());
    m_settings->setAnimationsEnabled(m_ui->kcfg_AnimationsEnabled->isChecked());
    m_settings->setAnimationsDuration(m_ui->kcfg_AnimationsDuration->value());
    m_settings->setBottomCornerRadiusFlag(m_ui->kcfg_BottomCorners->isChecked());
    m_settings->setSquareCornersTiled(m_ui->kcfg_SquareCornersTiled->isChecked());
    m_settings->setOutlineActive(m_ui->kcfg_OutlineActive->isChecked());
    m_settings->setHideCaptionWhenLimitedSpace(m_ui->kcfg_HideCaptionWhenLimitedSpace->isChecked());
    m_settings->setMinWidthForCaption(m_ui->kcfg_MinWidthForCaption->value());

    m_settings->setLongPressEnabled(m_ui->kcfg_LongPressEnabled->isChecked());
    m_settings->setLongPressDuration(m_ui->kcfg_LongPressDuration->value());

    m_settings->setTilingPopupOpacity(m_ui->kcfg_TilingPopupOpacity->value());

    m_settings->setDragFromButtonsEnabled(m_ui->kcfg_DragFromButtonsEnabled->isChecked());

    m_settings->save();
    QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                                     QStringLiteral("/KWin"),
                                                                     QStringLiteral("org.kde.KWin"),
                                                                     QStringLiteral("reconfigure")));
    updateChanged();
}

void MaterialDecorationKCM::defaults()
{
    m_ui->exceptionsWidget->defaults();

    Material::InternalSettings s;
    s.setDefaults();

    loadUIFromSettings(s);

    updateChanged();
}

bool MaterialDecorationKCM::isChanged() const
{
    if (m_ui->kcfg_TitleAlignment->currentIndex() != m_settings->titleAlignment()) return true;
    if (m_ui->kcfg_ButtonSize->currentIndex() != m_settings->buttonSize()) return true;
    if (m_ui->kcfg_ActiveOpacity->value() != qRound(m_settings->activeOpacity() * 100)) return true;
    if (m_ui->kcfg_InactiveOpacity->value() != qRound(m_settings->inactiveOpacity() * 100)) return true;
    if (m_ui->kcfg_CornerRadius->value() != m_settings->cornerRadius()) return true;
    if (m_ui->kcfg_UseSystemColors->isChecked() != m_settings->useSystemColors()) return true;
    if (m_ui->kcfg_UseCustomBorderColors->isChecked() != m_settings->useCustomBorderColors()) return true;
    if (m_ui->kcfg_ActiveBorderColor->color() != m_settings->activeBorderColor()) return true;
    if (m_ui->kcfg_InactiveBorderColor->color() != m_settings->inactiveBorderColor()) return true;
    if (m_ui->kcfg_MenuAlwaysShow->isChecked() != m_settings->menuAlwaysShow()) return true;
    if (m_ui->kcfg_ShowCaptionOnHover->isChecked() != m_settings->showCaptionOnHover()) return true;
    if (m_ui->kcfg_SearchEnabled->isChecked() != m_settings->searchEnabled()) return true;
    if (m_ui->kcfg_HamburgerMenu->isChecked() != m_settings->hamburgerMenu()) return true;
    if (m_ui->kcfg_ShowDisabledActions->isChecked() != m_settings->showDisabledActions()) return true;
    if (m_ui->kcfg_SearchIgnoreTopLevel->isChecked() != m_settings->searchIgnoreTopLevel()) return true;
    if (m_ui->kcfg_SearchIgnoreSubMenus->isChecked() != m_settings->searchIgnoreSubMenus()) return true;
    if (m_ui->kcfg_SearchFuzzyMatching->isChecked() != m_settings->searchFuzzyMatching()) return true;
    if (m_ui->kcfg_MenuButtonHorzPadding->value() != m_settings->menuButtonHorzPadding()) return true;
    if (m_ui->kcfg_UseSystemMenuFont->isChecked() != m_settings->useSystemMenuFont()) return true;
    if (m_ui->kcfg_ShadowSize->currentIndex() != m_settings->shadowSize()) return true;
    if (m_ui->kcfg_ShadowColor->color() != m_settings->shadowColor()) return true;
    if (m_ui->kcfg_ShadowStrength->value() != m_settings->shadowStrength()) return true;
    if (m_ui->kcfg_AnimationsEnabled->isChecked() != m_settings->animationsEnabled()) return true;
    if (m_ui->kcfg_AnimationsDuration->value() != m_settings->animationsDuration()) return true;
    if (m_ui->kcfg_BottomCorners->isChecked() != m_settings->bottomCornerRadiusFlag()) return true;
    if (m_ui->kcfg_SquareCornersTiled->isChecked() != m_settings->squareCornersTiled()) return true;
    if (m_ui->kcfg_OutlineActive->isChecked() != m_settings->outlineActive()) return true;
    if (m_ui->kcfg_HideCaptionWhenLimitedSpace->isChecked() != m_settings->hideCaptionWhenLimitedSpace()) return true;
    if (m_ui->kcfg_MinWidthForCaption->value() != m_settings->minWidthForCaption()) return true;
    if (m_ui->kcfg_LongPressEnabled->isChecked() != m_settings->longPressEnabled()) return true;
    if (m_ui->kcfg_LongPressDuration->value() != m_settings->longPressDuration()) return true;
    if (m_ui->kcfg_TilingPopupOpacity->value() != m_settings->tilingPopupOpacity()) return true;
    if (m_ui->kcfg_DragFromButtonsEnabled->isChecked() != m_settings->dragFromButtonsEnabled()) return true;

    if (m_ui->exceptionsWidget->isChanged()) return true;

    return false;
}

void MaterialDecorationKCM::updateChanged()
{
    setNeedsSave(isChanged());
}

#include "kcm.moc"
