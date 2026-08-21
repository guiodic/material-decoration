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

#include <QAbstractListModel>

namespace Material
{

class ExceptionModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit ExceptionModel(QObject *parent = nullptr);
    ~ExceptionModel() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void set(const InternalSettingsList &exceptions);
    const InternalSettingsList &exceptions() const { return m_exceptions; }

    InternalSettingsPtr get(int index) const;
    void add(const InternalSettingsPtr &exception);
    void update(int index, const InternalSettingsPtr &exception);
    void remove(int index);
    void moveUp(int index);
    void moveDown(int index);

private:
    InternalSettingsList m_exceptions;
};

} // namespace Material
