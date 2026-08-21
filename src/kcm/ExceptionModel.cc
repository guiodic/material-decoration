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
#include <algorithm>

namespace Material
{

ExceptionModel::ExceptionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ExceptionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_exceptions.size();
}

QVariant ExceptionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_exceptions.size()) {
        return QVariant();
    }

    const auto &exception = m_exceptions.at(index.row());

    if (role == Qt::DisplayRole) {
        QString pattern = exception->exceptionPattern();
        if (pattern.isEmpty()) {
            pattern = i18n("Empty pattern");
        }
        const QString typeStr = (exception->exceptionType() == 0) ? i18n("Title") : i18n("Class");
        return QStringLiteral("%1 (%2)").arg(pattern, typeStr);
    } else if (role == Qt::CheckStateRole) {
        return exception->enabled() ? Qt::Checked : Qt::Unchecked;
    }

    return QVariant();
}

bool ExceptionModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_exceptions.size()) {
        return false;
    }

    if (role == Qt::CheckStateRole) {
        m_exceptions.at(index.row())->setEnabled(value.toInt() == Qt::Checked);
        emit dataChanged(index, index, {Qt::CheckStateRole});
        return true;
    }

    return false;
}

Qt::ItemFlags ExceptionModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
}

void ExceptionModel::set(const InternalSettingsList &exceptions)
{
    beginResetModel();
    m_exceptions = exceptions;
    endResetModel();
}

InternalSettingsPtr ExceptionModel::get(int index) const
{
    if (index >= 0 && index < m_exceptions.size()) {
        return m_exceptions.at(index);
    }
    return nullptr;
}

void ExceptionModel::add(const InternalSettingsPtr &exception)
{
    beginInsertRows(QModelIndex(), m_exceptions.size(), m_exceptions.size());
    m_exceptions.append(exception);
    endInsertRows();
}

void ExceptionModel::update(int index, const InternalSettingsPtr &exception)
{
    if (index >= 0 && index < m_exceptions.size()) {
        m_exceptions[index] = exception;
        emit dataChanged(this->index(index), this->index(index));
    }
}

void ExceptionModel::remove(int index)
{
    if (index >= 0 && index < m_exceptions.size()) {
        beginRemoveRows(QModelIndex(), index, index);
        m_exceptions.removeAt(index);
        endRemoveRows();
    }
}

void ExceptionModel::moveUp(int index)
{
    if (index > 0 && index < m_exceptions.size()) {
        beginMoveRows(QModelIndex(), index, index, QModelIndex(), index - 1);
        std::swap(m_exceptions[index], m_exceptions[index - 1]);
        endMoveRows();
    }
}

void ExceptionModel::moveDown(int index)
{
    if (index >= 0 && index < m_exceptions.size() - 1) {
        beginMoveRows(QModelIndex(), index, index, QModelIndex(), index + 2);
        std::swap(m_exceptions[index], m_exceptions[index + 1]);
        endMoveRows();
    }
}

} // namespace Material
