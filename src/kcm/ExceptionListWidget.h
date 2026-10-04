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

/**
 * @brief KCM container widget displaying and managing the list of window exception rules.
 *
 * Provides a list view (`QListView`) of configured exception rules with buttons to add, edit,
 * remove, and reorder (move up/down) exception rules, tracking changes against initial baseline copies.
 */
class ExceptionListWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an ExceptionListWidget instance.
     * @param parent Optional parent widget.
     */
    explicit ExceptionListWidget(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~ExceptionListWidget() override = default;

    /**
     * @brief Loads exception rules from configuration file into list model.
     */
    void load();

    /**
     * @brief Saves current exception rules from list model to configuration file.
     */
    void save();

    /**
     * @brief Resets exception rules model to default empty state.
     */
    void defaults();

    /**
     * @brief Checks if current model exception rules differ from initial loaded state.
     * @return True if changed.
     */
    bool isChanged() const;

    /**
     * @brief Returns reference to current list of exception settings.
     * @return List of exception settings.
     */
    const InternalSettingsList &exceptions() const { return m_model->exceptions(); }

signals:
    /**
     * @brief Emitted when exception rules modification status changes.
     * @param value True if modified relative to initial baseline.
     */
    void changed(bool value);

private slots:
    /**
     * @brief Opens ExceptionDialog to create a new exception rule.
     */
    void add();

    /**
     * @brief Opens ExceptionDialog to edit selected exception rule.
     */
    void edit();

    /**
     * @brief Removes selected exception rule from model.
     */
    void remove();

    /**
     * @brief Moves selected exception rule up in evaluation order.
     */
    void up();

    /**
     * @brief Moves selected exception rule down in evaluation order.
     */
    void down();

    /**
     * @brief Updates enabled states of edit, remove, up, and down buttons based on current selection.
     */
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
