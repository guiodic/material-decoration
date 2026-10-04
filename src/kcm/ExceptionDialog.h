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

#pragma once

#include "../ExceptionList.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;

namespace Material
{

/**
 * @brief Editor dialog for creating or editing individual window exception rules.
 *
 * Allows configuring rule matching criteria (window class or title, exact match or regex),
 * window detection via mouse click, and setting individual override flags (hide title bar,
 * hide app menu, hamburger menu, hide shadow, square corners, active outline).
 */
class ExceptionDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an ExceptionDialog instance.
     * @param parent Optional parent widget.
     */
    explicit ExceptionDialog(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~ExceptionDialog() override = default;

    /**
     * @brief Populates dialog controls from an existing exception settings object.
     * @param exception Source exception settings.
     */
    void setException(const InternalSettingsPtr &exception);

    /**
     * @brief Writes dialog control state into an exception settings object.
     * @param exception Target exception settings receiving changes.
     */
    void applyToException(InternalSettingsPtr &exception);

    /**
     * @brief Validates input pattern and regular expression syntax before accepting dialog.
     */
    void accept() override;

private slots:
    /**
     * @brief Opens DetectDialog to detect window class or title.
     */
    void onDetectClicked();

    /**
     * @brief Handles toggling 'Hide title bar' checkbox to enable/disable title bar specific controls.
     * @param checked True if 'Hide title bar' is checked.
     */
    void onHideTitleBarToggled(bool checked);

private:
    QLineEdit *m_patternLineEdit = nullptr;
    QComboBox *m_exceptionTypeCombo = nullptr;
    QComboBox *m_matchingModeCombo = nullptr;
    QPushButton *m_detectButton = nullptr;

    QCheckBox *m_hideTitleBarCheckBox = nullptr;
    QCheckBox *m_hideApplicationMenuCheckBox = nullptr;
    QCheckBox *m_hamburgerMenuCheckBox = nullptr;
    QCheckBox *m_hideShadowCheckBox = nullptr;
    QCheckBox *m_squareCornersCheckBox = nullptr;
    QCheckBox *m_outlineActiveCheckBox = nullptr;
};

} // namespace Material
