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
#include <QAbstractTableModel>

namespace Material
{

class ExceptionModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColumnEnabled = 0,
        ColumnType,
        ColumnRegExp,
        nColumns
    };

    explicit ExceptionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void set(const InternalSettingsList &list);
    InternalSettingsList get() const { return m_exceptions; }

    InternalSettingsPtr get(const QModelIndex &index) const;
    InternalSettingsList get(const QModelIndexList &indexes) const;

    void add(InternalSettingsPtr exception);
    void remove(const InternalSettingsList &exceptions);

    bool contains(const QModelIndex &index) const;
    QModelIndex index(InternalSettingsPtr exception) const;

private:
    InternalSettingsList m_exceptions;
};

} // namespace Material
