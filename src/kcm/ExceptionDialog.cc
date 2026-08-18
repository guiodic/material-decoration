/*
 * Copyright (C) 2026 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ExceptionDialog.h"
#include "DetectDialog.h"
#include <ui_ExceptionDialog.h>
#include <KLocalizedString>
#include <QPushButton>

namespace Material
{

ExceptionDialog::ExceptionDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(std::make_unique<Ui::MaterialExceptionDialog>())
{
    m_ui->setupUi(this);

    // Populate combo box
    m_ui->titleAlignmentComboBox->addItems({i18n("Left"),
                                            i18n("Center"),
                                            i18n("Center (Full Width)"),
                                            i18n("Right"),
                                            i18n("Hidden")});

    m_checkboxes.insert(TitleAlignment, m_ui->titleAlignmentCheckBox);
    m_checkboxes.insert(CornerRadius, m_ui->cornerRadiusCheckBox);
    m_checkboxes.insert(Opacity, m_ui->opacityCheckBox);

    connect(m_ui->detectDialogButton, &QPushButton::clicked, this, &ExceptionDialog::selectWindowProperties);

    connect(m_ui->exceptionType, qOverload<int>(&QComboBox::currentIndexChanged), this, &ExceptionDialog::updateChanged);
    connect(m_ui->exceptionEditor, &QLineEdit::textChanged, this, &ExceptionDialog::updateChanged);

    connect(m_ui->titleAlignmentComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &ExceptionDialog::updateChanged);
    connect(m_ui->cornerRadiusSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &ExceptionDialog::updateChanged);
    connect(m_ui->activeOpacitySpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &ExceptionDialog::updateChanged);
    connect(m_ui->inactiveOpacitySpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &ExceptionDialog::updateChanged);
    connect(m_ui->outlineActiveVal, &QCheckBox::toggled, this, &ExceptionDialog::updateChanged);
    connect(m_ui->hideShadowVal, &QCheckBox::toggled, this, &ExceptionDialog::updateChanged);
    connect(m_ui->showMenuOnHoverVal, &QCheckBox::toggled, this, &ExceptionDialog::updateChanged);
    connect(m_ui->hamburgerMenuVal, &QCheckBox::toggled, this, &ExceptionDialog::updateChanged);

    for (auto it = m_checkboxes.begin(); it != m_checkboxes.end(); ++it) {
        connect(it.value(), &QCheckBox::toggled, this, &ExceptionDialog::updateChanged);
    }

    // Enable/disable sub-controls depending on checkboxes
    connect(m_ui->titleAlignmentCheckBox, &QCheckBox::toggled, m_ui->titleAlignmentComboBox, &QWidget::setEnabled);
    connect(m_ui->cornerRadiusCheckBox, &QCheckBox::toggled, m_ui->cornerRadiusSpinBox, &QWidget::setEnabled);
    connect(m_ui->opacityCheckBox, &QCheckBox::toggled, m_ui->activeOpacitySpinBox, &QWidget::setEnabled);
    connect(m_ui->opacityCheckBox, &QCheckBox::toggled, m_ui->inactiveOpacitySpinBox, &QWidget::setEnabled);
    connect(m_ui->opacityCheckBox, &QCheckBox::toggled, m_ui->labelActiveOpacity, &QWidget::setEnabled);
    connect(m_ui->opacityCheckBox, &QCheckBox::toggled, m_ui->labelInactiveOpacity, &QWidget::setEnabled);

    // Disable "Allineamento del titolo" if "Hide window title bar" is checked
    connect(m_ui->hideTitleBar, &QCheckBox::toggled, this, [this](bool hideChecked) {
        if (hideChecked) {
            m_ui->titleAlignmentCheckBox->setChecked(false);
            m_ui->titleAlignmentCheckBox->setEnabled(false);
            m_ui->titleAlignmentComboBox->setEnabled(false);
        } else {
            m_ui->titleAlignmentCheckBox->setEnabled(true);
            m_ui->titleAlignmentComboBox->setEnabled(m_ui->titleAlignmentCheckBox->isChecked());
        }
        updateChanged();
    });
}

ExceptionDialog::~ExceptionDialog() = default;

void ExceptionDialog::setException(InternalSettingsPtr exception)
{
    m_exception = exception;

    m_ui->exceptionType->setCurrentIndex(m_exception->exceptionType());
    m_ui->exceptionEditor->setText(m_exception->exceptionPattern());
    m_ui->hideTitleBar->setChecked(m_exception->hideTitleBar());

    m_ui->titleAlignmentComboBox->setCurrentIndex(m_exception->titleAlignment());
    m_ui->cornerRadiusSpinBox->setValue(m_exception->cornerRadius());
    m_ui->activeOpacitySpinBox->setValue(qRound(m_exception->activeOpacity() * 100));
    m_ui->inactiveOpacitySpinBox->setValue(qRound(m_exception->inactiveOpacity() * 100));

    m_ui->outlineActiveVal->setChecked((m_exception->mask() & OutlineActive) && m_exception->outlineActive());
    m_ui->hideShadowVal->setChecked((m_exception->mask() & ShadowSize) && m_exception->shadowSize() == InternalSettings::ShadowNone);
    m_ui->showMenuOnHoverVal->setChecked((m_exception->mask() & MenuAlwaysShow) && m_exception->menuAlwaysShow() == false);
    m_ui->hamburgerMenuVal->setChecked((m_exception->mask() & HamburgerMenu) && m_exception->hamburgerMenu());

    for (auto it = m_checkboxes.begin(); it != m_checkboxes.end(); ++it) {
        it.value()->setChecked(m_exception->mask() & it.key());
    }

    const bool hideChecked = m_ui->hideTitleBar->isChecked();
    if (hideChecked) {
        m_ui->titleAlignmentCheckBox->setChecked(false);
        m_ui->titleAlignmentCheckBox->setEnabled(false);
        m_ui->titleAlignmentComboBox->setEnabled(false);
    } else {
        m_ui->titleAlignmentCheckBox->setEnabled(true);
        m_ui->titleAlignmentComboBox->setEnabled(m_ui->titleAlignmentCheckBox->isChecked());
    }

    m_ui->cornerRadiusSpinBox->setEnabled(m_ui->cornerRadiusCheckBox->isChecked());
    m_ui->activeOpacitySpinBox->setEnabled(m_ui->opacityCheckBox->isChecked());
    m_ui->inactiveOpacitySpinBox->setEnabled(m_ui->opacityCheckBox->isChecked());
    m_ui->labelActiveOpacity->setEnabled(m_ui->opacityCheckBox->isChecked());
    m_ui->labelInactiveOpacity->setEnabled(m_ui->opacityCheckBox->isChecked());

    setChanged(false);
}

void ExceptionDialog::save()
{
    m_exception->setExceptionType(m_ui->exceptionType->currentIndex());
    m_exception->setExceptionPattern(m_ui->exceptionEditor->text());
    m_exception->setHideTitleBar(m_ui->hideTitleBar->isChecked());

    m_exception->setTitleAlignment(m_ui->titleAlignmentComboBox->currentIndex());
    m_exception->setCornerRadius(m_ui->cornerRadiusSpinBox->value());
    m_exception->setActiveOpacity(static_cast<double>(m_ui->activeOpacitySpinBox->value()) / 100.0);
    m_exception->setInactiveOpacity(static_cast<double>(m_ui->inactiveOpacitySpinBox->value()) / 100.0);

    unsigned int mask = None;
    for (auto it = m_checkboxes.begin(); it != m_checkboxes.end(); ++it) {
        if (it.value()->isChecked()) {
            mask |= it.key();
        }
    }

    if (m_ui->outlineActiveVal->isChecked()) {
        mask |= OutlineActive;
        m_exception->setOutlineActive(true);
    } else {
        m_exception->setOutlineActive(false);
    }

    if (m_ui->hideShadowVal->isChecked()) {
        mask |= ShadowSize;
        m_exception->setShadowSize(InternalSettings::ShadowNone);
    } else {
        m_exception->setShadowSize(InternalSettings::ShadowVeryLarge);
    }

    if (m_ui->showMenuOnHoverVal->isChecked()) {
        mask |= MenuAlwaysShow;
        m_exception->setMenuAlwaysShow(false);
    } else {
        m_exception->setMenuAlwaysShow(true);
    }

    if (m_ui->hamburgerMenuVal->isChecked()) {
        mask |= HamburgerMenu;
        m_exception->setHamburgerMenu(true);
    } else {
        m_exception->setHamburgerMenu(false);
    }

    m_exception->setMask(mask);

    setChanged(false);
}

void ExceptionDialog::setChanged(bool value)
{
    m_changed = value;
    emit changed(value);
}

void ExceptionDialog::updateChanged()
{
    bool modified = false;

    if (m_exception->exceptionType() != m_ui->exceptionType->currentIndex()) {
        modified = true;
    } else if (m_exception->exceptionPattern() != m_ui->exceptionEditor->text()) {
        modified = true;
    } else if (m_exception->hideTitleBar() != m_ui->hideTitleBar->isChecked()) {
        modified = true;
    } else if (m_ui->titleAlignmentCheckBox->isChecked() && m_exception->titleAlignment() != m_ui->titleAlignmentComboBox->currentIndex()) {
        modified = true;
    } else if (m_ui->cornerRadiusCheckBox->isChecked() && m_exception->cornerRadius() != m_ui->cornerRadiusSpinBox->value()) {
        modified = true;
    } else if (m_ui->opacityCheckBox->isChecked() && (m_exception->activeOpacity() != static_cast<double>(m_ui->activeOpacitySpinBox->value()) / 100.0 || m_exception->inactiveOpacity() != static_cast<double>(m_ui->inactiveOpacitySpinBox->value()) / 100.0)) {
        modified = true;
    } else if (m_ui->outlineActiveVal->isChecked() != ((m_exception->mask() & OutlineActive) && m_exception->outlineActive())) {
        modified = true;
    } else if (m_ui->hideShadowVal->isChecked() != ((m_exception->mask() & ShadowSize) && m_exception->shadowSize() == InternalSettings::ShadowNone)) {
        modified = true;
    } else if (m_ui->showMenuOnHoverVal->isChecked() != ((m_exception->mask() & MenuAlwaysShow) && m_exception->menuAlwaysShow() == false)) {
        modified = true;
    } else if (m_ui->hamburgerMenuVal->isChecked() != ((m_exception->mask() & HamburgerMenu) && m_exception->hamburgerMenu())) {
        modified = true;
    } else {
        for (auto it = m_checkboxes.begin(); it != m_checkboxes.end(); ++it) {
            if (it.value()->isChecked() != static_cast<bool>(m_exception->mask() & it.key())) {
                modified = true;
                break;
            }
        }
    }

    setChanged(modified);
}

void ExceptionDialog::selectWindowProperties()
{
    if (!m_detectDialog) {
        m_detectDialog = new DetectDialog(this);
        connect(m_detectDialog, &DetectDialog::detectionDone, this, &ExceptionDialog::readWindowProperties);
    }
    m_detectDialog->detect();
}

void ExceptionDialog::readWindowProperties(bool success)
{
    if (success && m_detectDialog) {
        const QVariantMap properties = m_detectDialog->properties();
        switch (m_ui->exceptionType->currentIndex()) {
        default:
        case InternalSettings::ExceptionWindowClassName:
            m_ui->exceptionEditor->setText(properties.value(QStringLiteral("resourceClass")).toString());
            break;
        case InternalSettings::ExceptionWindowTitle:
            m_ui->exceptionEditor->setText(properties.value(QStringLiteral("caption")).toString());
            break;
        }
    }
    if (m_detectDialog) {
        m_detectDialog->deleteLater();
        m_detectDialog = nullptr;
    }
}

} // namespace Material
