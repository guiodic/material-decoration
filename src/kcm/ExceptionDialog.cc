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
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
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

    criteriaLayout->addRow(i18n("Window Property / Pattern:"), patternLayout);

    m_exceptionTypeCombo = new QComboBox(this);
    m_exceptionTypeCombo->addItems({i18n("Window Title"), i18n("Window Class")});
    m_exceptionTypeCombo->setCurrentIndex(1); // Window Class default
    criteriaLayout->addRow(i18n("Match against:"), m_exceptionTypeCombo);

    m_matchingModeCombo = new QComboBox(this);
    m_matchingModeCombo->addItems({i18n("Exact Match"), i18n("Regular Expression")});
    criteriaLayout->addRow(i18n("Matching mode:"), m_matchingModeCombo);

    mainLayout->addWidget(criteriaGroup);

    // Overrides / Options Group
    auto optionsGroup = new QGroupBox(i18n("Window Options"), this);
    auto optionsLayout = new QVBoxLayout(optionsGroup);

    m_hideTitleBarCheckBox = new QCheckBox(i18n("Hide window title bar"), this);
    m_hideApplicationMenuCheckBox = new QCheckBox(i18n("Show the window title and hide the menu"), this);
    m_hamburgerMenuCheckBox = new QCheckBox(i18n("Show menu as hamburger"), this);
    m_hideShadowCheckBox = new QCheckBox(i18n("Don't draw window shadow"), this);
    m_squareCornersCheckBox = new QCheckBox(i18n("Don't round window corners"), this);
    m_outlineActiveCheckBox = new QCheckBox(i18n("Paint a thin line around the window"), this);

    optionsLayout->addWidget(m_hideTitleBarCheckBox);
    optionsLayout->addWidget(m_hideApplicationMenuCheckBox);
    optionsLayout->addWidget(m_hamburgerMenuCheckBox);
    optionsLayout->addWidget(m_hideShadowCheckBox);
    optionsLayout->addWidget(m_squareCornersCheckBox);
    optionsLayout->addWidget(m_outlineActiveCheckBox);

    mainLayout->addWidget(optionsGroup);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(m_detectButton, &QPushButton::clicked, this, &ExceptionDialog::onDetectClicked);
    connect(m_hideTitleBarCheckBox, &QCheckBox::toggled, this, &ExceptionDialog::onHideTitleBarToggled);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ExceptionDialog::accept()
{
    if (m_patternLineEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, i18n("Warning"), i18n("Pattern cannot be empty."));
        return;
    }

    if (static_cast<MatchingMode>(m_matchingModeCombo->currentIndex()) == MatchingMode::RegularExpression) {
        QRegularExpression regex(m_patternLineEdit->text().trimmed());
        if (!regex.isValid()) {
            QMessageBox::warning(this, i18n("Warning"), i18n("Regular Expression syntax error: %1", regex.errorString()));
            return;
        }
    }

    QDialog::accept();
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
        QString text;
        if (m_exceptionTypeCombo->currentIndex() == 1) { // Window Class
            text = dialog.windowClass();
        } else { // Window Title
            text = dialog.caption();
        }

        if (!text.isEmpty()) {
            m_patternLineEdit->setText(text);
        }
    }
}

void ExceptionDialog::setException(const InternalSettingsPtr &exception)
{
    if (!exception) {
        return;
    }

    m_patternLineEdit->setText(exception->exceptionPattern());
    const int typeIndex = qBound(0, exception->exceptionType(), m_exceptionTypeCombo->count() - 1);
    m_exceptionTypeCombo->setCurrentIndex(typeIndex);
    const int modeIndex = qBound(0, exception->matchingMode(), m_matchingModeCombo->count() - 1);
    m_matchingModeCombo->setCurrentIndex(modeIndex);

    m_hideTitleBarCheckBox->setChecked(exception->hideTitleBar());
    m_hideApplicationMenuCheckBox->setChecked(exception->hideApplicationMenu());
    m_hamburgerMenuCheckBox->setChecked(exception->hamburgerMenu());
    m_hideShadowCheckBox->setChecked(exception->hideShadow());
    m_squareCornersCheckBox->setChecked(exception->squareCorners());
    m_outlineActiveCheckBox->setChecked(exception->outlineActive());

    onHideTitleBarToggled(exception->hideTitleBar());
}

void ExceptionDialog::applyToException(InternalSettingsPtr &exception)
{
    if (!exception) {
        exception = InternalSettingsPtr(new InternalSettings());
    }

    exception->setExceptionPattern(m_patternLineEdit->text().trimmed());
    exception->setExceptionType(m_exceptionTypeCombo->currentIndex());
    exception->setMatchingMode(m_matchingModeCombo->currentIndex());

    int mask = ExceptionMask::HideTitleBar |
               ExceptionMask::HideApplicationMenu |
               ExceptionMask::HamburgerMenu |
               ExceptionMask::HideShadow |
               ExceptionMask::SquareCorners |
               ExceptionMask::OutlineActive;

    exception->setHideTitleBar(m_hideTitleBarCheckBox->isChecked());
    exception->setHideApplicationMenu(m_hideApplicationMenuCheckBox->isChecked());
    exception->setHamburgerMenu(m_hamburgerMenuCheckBox->isChecked());
    exception->setHideShadow(m_hideShadowCheckBox->isChecked());
    exception->setSquareCorners(m_squareCornersCheckBox->isChecked());
    exception->setOutlineActive(m_outlineActiveCheckBox->isChecked());

    exception->setMask(mask);
}

} // namespace Material
