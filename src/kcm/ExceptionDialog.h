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

#include "ExceptionList.h"
#include <QDialog>
#include <QMap>
#include <memory>

class QCheckBox;

namespace Ui
{
class MaterialExceptionDialog;
}

namespace Material
{

class DetectDialog;

class ExceptionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExceptionDialog(QWidget *parent = nullptr);
    ~ExceptionDialog() override;

    void setException(InternalSettingsPtr exception);
    void save();

    bool isChanged() const { return m_changed; }

Q_SIGNALS:
    void changed(bool changed);

protected:
    void setChanged(bool value);

private Q_SLOTS:
    void updateChanged();
    void selectWindowProperties();
    void readWindowProperties(bool success);

private:
    using CheckBoxMap = QMap<ExceptionMask, QCheckBox *>;

    std::unique_ptr<Ui::MaterialExceptionDialog> m_ui;
    CheckBoxMap m_checkboxes;
    InternalSettingsPtr m_exception;
    DetectDialog *m_detectDialog = nullptr;
    bool m_changed = false;
};

} // namespace Material
