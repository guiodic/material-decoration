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

/**
 * @brief Qt list model (`QAbstractListModel`) exposing window exception rules to `QListView`.
 */
class ExceptionModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an ExceptionModel instance.
     * @param parent Optional parent object.
     */
    explicit ExceptionModel(QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~ExceptionModel() override = default;

    /**
     * @brief Returns number of exception rows in model.
     * @param parent Model index parent.
     * @return Number of rows.
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief Returns display or edit data for exception item at index.
     * @param index Model index.
     * @param role Data role (DisplayRole, CheckStateRole, etc.).
     * @return Data variant.
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /**
     * @brief Sets data value for item at index (e.g. toggling rule enabled checkbox).
     * @param index Model index.
     * @param value New value variant.
     * @param role Data role.
     * @return True if successful.
     */
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    /**
     * @brief Returns item flags for index.
     * @param index Model index.
     * @return Item flags.
     */
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    /**
     * @brief Replaces internal exception list with new list and resets model.
     * @param exceptions New exception settings list.
     */
    void set(const InternalSettingsList &exceptions);

    /**
     * @brief Returns internal exception settings list.
     * @return Reference to exception list.
     */
    const InternalSettingsList &exceptions() const { return m_exceptions; }

    /**
     * @brief Returns exception settings pointer at specified row index.
     * @param index Row index.
     * @return Exception settings pointer.
     */
    InternalSettingsPtr get(int index) const;

    /**
     * @brief Appends a new exception rule to model.
     * @param exception New exception settings pointer.
     */
    void add(const InternalSettingsPtr &exception);

    /**
     * @brief Updates exception rule at specified index.
     * @param index Row index to update.
     * @param exception Updated exception settings pointer.
     */
    void update(int index, const InternalSettingsPtr &exception);

    /**
     * @brief Removes exception rule at specified row index.
     * @param index Row index to remove.
     */
    void remove(int index);

    /**
     * @brief Moves exception rule at index up one position.
     * @param index Target row index.
     */
    void moveUp(int index);

    /**
     * @brief Moves exception rule at index down one position.
     * @param index Target row index.
     */
    void moveDown(int index);

private:
    InternalSettingsList m_exceptions;
};

} // namespace Material
