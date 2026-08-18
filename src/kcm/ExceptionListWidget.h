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

#include "ExceptionModel.h"
#include <QWidget>
#include <memory>

namespace Ui
{
class MaterialExceptionListWidget;
}

namespace Material
{

class ExceptionListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ExceptionListWidget(QWidget *parent = nullptr);
    ~ExceptionListWidget() override;

    void setExceptions(const InternalSettingsList &exceptions);
    InternalSettingsList exceptions();

    bool isChanged() const { return m_changed; }

Q_SIGNALS:
    void changed(bool changed);

protected:
    const ExceptionModel &model() const { return m_model; }
    ExceptionModel &model() { return m_model; }

protected Q_SLOTS:
    void updateButtons();
    void add();
    void edit();
    void remove();
    void toggle(const QModelIndex &index);
    void up();
    void down();

protected:
    void resizeColumns() const;
    bool checkException(InternalSettingsPtr exception);
    void setChanged(bool value);

private:
    ExceptionModel m_model;
    std::unique_ptr<Ui::MaterialExceptionListWidget> m_ui;
    bool m_changed = false;
};

} // namespace Material
