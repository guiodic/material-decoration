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

#include "ExceptionModel.h"
#include <KLocalizedString>

namespace Material
{

ExceptionModel::ExceptionModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ExceptionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_exceptions.size();
}

int ExceptionModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return nColumns;
}

QVariant ExceptionModel::data(const QModelIndex &index, int role) const
{
    if (!contains(index)) {
        return QVariant();
    }

    const InternalSettingsPtr &configuration = m_exceptions.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColumnType: {
            switch (configuration->exceptionType()) {
            case InternalSettings::ExceptionWindowTitle:
                return i18n("Window Title");
            default:
            case InternalSettings::ExceptionWindowClassName:
                return i18n("Window Class Name");
            }
        }
        case ColumnRegExp:
            return configuration->exceptionPattern();
        default:
            return QVariant();
        }
    } else if (role == Qt::CheckStateRole && index.column() == ColumnEnabled) {
        return configuration->enabled() ? Qt::Checked : Qt::Unchecked;
    } else if (role == Qt::ToolTipRole && index.column() == ColumnEnabled) {
        return i18n("Enable/disable this exception");
    }

    return QVariant();
}

QVariant ExceptionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case ColumnEnabled:
            return QString();
        case ColumnType:
            return i18n("Exception Type");
        case ColumnRegExp:
            return i18n("Regular Expression");
        default:
            break;
        }
    }
    return QVariant();
}

void ExceptionModel::set(const InternalSettingsList &list)
{
    beginResetModel();
    m_exceptions = list;
    endResetModel();
}

InternalSettingsPtr ExceptionModel::get(const QModelIndex &index) const
{
    if (contains(index)) {
        return m_exceptions.at(index.row());
    }
    return InternalSettingsPtr();
}

InternalSettingsList ExceptionModel::get(const QModelIndexList &indexes) const
{
    InternalSettingsList list;
    for (const QModelIndex &index : indexes) {
        if (contains(index) && index.column() == 0) {
            list.append(m_exceptions.at(index.row()));
        }
    }
    return list;
}

void ExceptionModel::add(InternalSettingsPtr exception)
{
    beginInsertRows(QModelIndex(), m_exceptions.size(), m_exceptions.size());
    m_exceptions.append(exception);
    endInsertRows();
}

void ExceptionModel::remove(const InternalSettingsList &exceptions)
{
    for (const InternalSettingsPtr &exception : exceptions) {
        int row = m_exceptions.indexOf(exception);
        if (row >= 0) {
            beginRemoveRows(QModelIndex(), row, row);
            m_exceptions.removeAt(row);
            endRemoveRows();
        }
    }
}

bool ExceptionModel::contains(const QModelIndex &index) const
{
    return index.isValid() && index.row() >= 0 && index.row() < m_exceptions.size() && index.column() >= 0 && index.column() < nColumns;
}

QModelIndex ExceptionModel::index(InternalSettingsPtr exception) const
{
    int row = m_exceptions.indexOf(exception);
    if (row >= 0) {
        return createIndex(row, 0);
    }
    return QModelIndex();
}

} // namespace Material
