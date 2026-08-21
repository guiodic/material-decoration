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

#include <KLocalizedString>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace Material
{

ExceptionDialog::ExceptionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Window Exception Settings"));

    auto mainLayout = new QVBoxLayout(this);

    // Matching Criteria Group
    auto criteriaGroup = new QGroupBox(i18n("Window Identification"), this);
    auto criteriaLayout = new QFormLayout(criteriaGroup);

    m_patternLineEdit = new QLineEdit(this);
    m_detectButton = new QPushButton(i18n("Detect Window Properties..."), this);

    auto patternLayout = new QHBoxLayout();
    patternLayout->addWidget(m_patternLineEdit);
    patternLayout->addWidget(m_detectButton);

    criteriaLayout->addRow(i18n("Regular Expression / Window Property:"), patternLayout);

    m_exceptionTypeCombo = new QComboBox(this);
    m_exceptionTypeCombo->addItems({i18n("Window Title"), i18n("Window Class")});
    criteriaLayout->addRow(i18n("Match against:"), m_exceptionTypeCombo);

    m_matchingModeCombo = new QComboBox(this);
    m_matchingModeCombo->addItems({i18n("Exact Match"), i18n("Regular Expression")});
    criteriaLayout->addRow(i18n("Matching mode:"), m_matchingModeCombo);

    mainLayout->addWidget(criteriaGroup);

    // Overrides / Options Group
    auto optionsGroup = new QGroupBox(i18n("Window Options"), this);
    auto optionsLayout = new QVBoxLayout(optionsGroup);

    m_hideTitleBarCheckBox = new QCheckBox(i18n("Hide window title bar"), this);
    m_hideApplicationMenuCheckBox = new QCheckBox(i18n("Never show LIM menu"), this);
    m_hamburgerMenuCheckBox = new QCheckBox(i18n("Show menu as hamburger"), this);
    m_hideShadowCheckBox = new QCheckBox(i18n("Don't draw window shadow"), this);
    m_squareCornersCheckBox = new QCheckBox(i18n("Don't round window corners"), this);

    optionsLayout->addWidget(m_hideTitleBarCheckBox);
    optionsLayout->addWidget(m_hideApplicationMenuCheckBox);
    optionsLayout->addWidget(m_hamburgerMenuCheckBox);
    optionsLayout->addWidget(m_hideShadowCheckBox);
    optionsLayout->addWidget(m_squareCornersCheckBox);

    mainLayout->addWidget(optionsGroup);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(m_detectButton, &QPushButton::clicked, this, &ExceptionDialog::onDetectClicked);
    connect(m_hideTitleBarCheckBox, &QCheckBox::toggled, this, &ExceptionDialog::onHideTitleBarToggled);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ExceptionDialog::onHideTitleBarToggled(bool checked)
{
    m_hideApplicationMenuCheckBox->setEnabled(!checked);
    m_hamburgerMenuCheckBox->setEnabled(!checked);
}

void ExceptionDialog::onDetectClicked()
{
    DetectDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        if (m_exceptionTypeCombo->currentIndex() == 1) { // Window Class
            if (!dialog.windowClass().isEmpty()) {
                m_patternLineEdit->setText(dialog.windowClass());
            }
        } else { // Window Title
            if (!dialog.caption().isEmpty()) {
                m_patternLineEdit->setText(dialog.caption());
            }
        }
    }
}

void ExceptionDialog::setException(const InternalSettingsPtr &exception)
{
    if (!exception) {
        return;
    }

    m_patternLineEdit->setText(exception->exceptionPattern());
    m_exceptionTypeCombo->setCurrentIndex(exception->exceptionType());
    m_matchingModeCombo->setCurrentIndex(exception->matchingMode());

    const int mask = exception->mask();

    const bool hideTitleBar = (mask & ExceptionMask::HideTitleBar) || exception->hideTitleBar();
    const bool hideAppMenu = (mask & ExceptionMask::HideApplicationMenu) || exception->hideApplicationMenu();
    const bool hamburger = (mask & ExceptionMask::HamburgerMenu) || exception->hamburgerMenu();
    const bool hideShadow = (mask & ExceptionMask::HideShadow) || exception->hideShadow();
    const bool squareCorners = (mask & ExceptionMask::SquareCorners) || exception->squareCorners();

    m_hideTitleBarCheckBox->setChecked(hideTitleBar);
    m_hideApplicationMenuCheckBox->setChecked(hideAppMenu);
    m_hamburgerMenuCheckBox->setChecked(hamburger);
    m_hideShadowCheckBox->setChecked(hideShadow);
    m_squareCornersCheckBox->setChecked(squareCorners);

    onHideTitleBarToggled(hideTitleBar);
}

void ExceptionDialog::applyToException(InternalSettingsPtr &exception)
{
    if (!exception) {
        exception = InternalSettingsPtr(new InternalSettings());
    }

    exception->setExceptionPattern(m_patternLineEdit->text());
    exception->setExceptionType(m_exceptionTypeCombo->currentIndex());
    exception->setMatchingMode(m_matchingModeCombo->currentIndex());

    int mask = ExceptionMask::None;

    const bool hideTitleBar = m_hideTitleBarCheckBox->isChecked();
    if (hideTitleBar) {
        mask |= ExceptionMask::HideTitleBar;
    }
    exception->setHideTitleBar(hideTitleBar);

    const bool hideAppMenu = m_hideApplicationMenuCheckBox->isChecked();
    if (hideAppMenu) {
        mask |= ExceptionMask::HideApplicationMenu;
    }
    exception->setHideApplicationMenu(hideAppMenu);

    const bool hamburger = m_hamburgerMenuCheckBox->isChecked();
    if (hamburger) {
        mask |= ExceptionMask::HamburgerMenu;
    }
    exception->setHamburgerMenu(hamburger);

    const bool hideShadow = m_hideShadowCheckBox->isChecked();
    if (hideShadow) {
        mask |= ExceptionMask::HideShadow;
    }
    exception->setHideShadow(hideShadow);

    const bool squareCorners = m_squareCornersCheckBox->isChecked();
    if (squareCorners) {
        mask |= ExceptionMask::SquareCorners;
    }
    exception->setSquareCorners(squareCorners);

    exception->setMask(mask);
}

} // namespace Material
