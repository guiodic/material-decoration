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

class QListView;
class QPushButton;

namespace Material
{

class ExceptionListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ExceptionListWidget(QWidget *parent = nullptr);
    ~ExceptionListWidget() override = default;

    void load();
    void save();
    void defaults();
    bool isChanged() const;

    const InternalSettingsList &exceptions() const { return m_model->exceptions(); }

signals:
    void changed(bool value);

private slots:
    void add();
    void edit();
    void remove();
    void up();
    void down();
    void updateButtons();

private:
    QListView *m_listView = nullptr;
    ExceptionModel *m_model = nullptr;

    QPushButton *m_addButton = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_moveUpButton = nullptr;
    QPushButton *m_moveDownButton = nullptr;

    InternalSettingsList m_initialExceptions;
};

} // namespace Material
