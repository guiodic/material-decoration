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

class ExceptionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExceptionDialog(QWidget *parent = nullptr);
    ~ExceptionDialog() override = default;

    void setException(const InternalSettingsPtr &exception);
    void applyToException(InternalSettingsPtr &exception);

    void accept() override;

private slots:
    void onDetectClicked();
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
};

} // namespace Material
