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

#include "ExceptionListWidget.h"
#include "ExceptionDialog.h"

#include <KLocalizedString>
#include <QHBoxLayout>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace Material
{

namespace
{

InternalSettingsPtr cloneException(const InternalSettingsPtr &src)
{
    return cloneInternalSettings(src);
}

InternalSettingsList cloneExceptionList(const InternalSettingsList &src)
{
    InternalSettingsList list;
    list.reserve(src.size());
    for (const auto &item : src) {
        list.append(cloneException(item));
    }
    return list;
}

} // anonymous namespace

ExceptionListWidget::ExceptionListWidget(QWidget *parent)
    : QWidget(parent)
    , m_listView(new QListView(this))
    , m_model(new ExceptionModel(this))
{
    m_listView->setModel(m_model);

    auto mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    mainLayout->addWidget(m_listView);

    auto buttonLayout = new QVBoxLayout();

    m_addButton = new QPushButton(i18n("Add..."), this);
    m_editButton = new QPushButton(i18n("Edit..."), this);
    m_removeButton = new QPushButton(i18n("Remove"), this);
    m_moveUpButton = new QPushButton(i18n("Move Up"), this);
    m_moveDownButton = new QPushButton(i18n("Move Down"), this);

    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addWidget(m_moveUpButton);
    buttonLayout->addWidget(m_moveDownButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);

    connect(m_addButton, &QPushButton::clicked, this, &ExceptionListWidget::add);
    connect(m_editButton, &QPushButton::clicked, this, &ExceptionListWidget::edit);
    connect(m_removeButton, &QPushButton::clicked, this, &ExceptionListWidget::remove);
    connect(m_moveUpButton, &QPushButton::clicked, this, &ExceptionListWidget::up);
    connect(m_moveDownButton, &QPushButton::clicked, this, &ExceptionListWidget::down);

    connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ExceptionListWidget::updateButtons);
    connect(m_listView, &QAbstractItemView::doubleClicked, this, &ExceptionListWidget::edit);
    connect(m_model, &QAbstractItemModel::dataChanged, this, [this] {
        updateButtons();
        emit changed(isChanged());
    });
    connect(m_model, &QAbstractItemModel::rowsMoved, this, [this] {
        updateButtons();
        emit changed(isChanged());
    });
    connect(m_model, &QAbstractItemModel::rowsInserted, this, [this] {
        updateButtons();
        emit changed(isChanged());
    });
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, [this] {
        updateButtons();
        emit changed(isChanged());
    });

    updateButtons();
}

void ExceptionListWidget::updateButtons()
{
    const QModelIndex index = m_listView->currentIndex();
    const bool hasSelection = index.isValid();
    const int row = index.row();
    const int count = m_model->rowCount();

    m_editButton->setEnabled(hasSelection);
    m_removeButton->setEnabled(hasSelection);
    m_moveUpButton->setEnabled(hasSelection && row > 0);
    m_moveDownButton->setEnabled(hasSelection && row < count - 1);
}

bool ExceptionListWidget::checkException(const InternalSettingsPtr &exception)
{
    if (!exception) {
        return false;
    }

    if (exception->exceptionPattern().trimmed().isEmpty()) {
        QMessageBox::warning(this, i18n("Warning"), i18n("Pattern cannot be empty."));
        return false;
    }

    if (exception->matchingMode() == 1) { // Regular Expression
        QRegularExpression regex(exception->exceptionPattern());
        if (!regex.isValid()) {
            QMessageBox::warning(this, i18n("Warning"), i18n("Regular Expression syntax error: %1", regex.errorString()));
            return false;
        }
    }

    return true;
}

void ExceptionListWidget::add()
{
    InternalSettingsPtr exception(new InternalSettings());
    ExceptionDialog dialog(this);
    dialog.setException(exception);

    if (dialog.exec() == QDialog::Accepted) {
        dialog.applyToException(exception);
        if (checkException(exception)) {
            m_model->add(exception);
            const QModelIndex index = m_model->index(m_model->rowCount() - 1, 0);
            m_listView->setCurrentIndex(index);
        }
    }
}

void ExceptionListWidget::edit()
{
    const QModelIndex index = m_listView->currentIndex();
    if (!index.isValid()) {
        return;
    }

    const auto current = m_model->get(index.row());
    if (!current) {
        return;
    }

    InternalSettingsPtr workingCopy = cloneInternalSettings(current);

    ExceptionDialog dialog(this);
    dialog.setException(workingCopy);

    if (dialog.exec() == QDialog::Accepted) {
        dialog.applyToException(workingCopy);
        if (checkException(workingCopy)) {
            m_model->update(index.row(), workingCopy);
        }
    }
}

void ExceptionListWidget::remove()
{
    const QModelIndex index = m_listView->currentIndex();
    if (!index.isValid()) {
        return;
    }

    m_model->remove(index.row());
    updateButtons();
}

void ExceptionListWidget::up()
{
    const QModelIndex index = m_listView->currentIndex();
    if (!index.isValid() || index.row() <= 0) {
        return;
    }

    const int newRow = index.row() - 1;
    m_model->moveUp(index.row());
    m_listView->setCurrentIndex(m_model->index(newRow, 0));
    updateButtons();
}

void ExceptionListWidget::down()
{
    const QModelIndex index = m_listView->currentIndex();
    if (!index.isValid() || index.row() >= m_model->rowCount() - 1) {
        return;
    }

    const int newRow = index.row() + 1;
    m_model->moveDown(index.row());
    m_listView->setCurrentIndex(m_model->index(newRow, 0));
    updateButtons();
}

void ExceptionListWidget::load()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
    ExceptionList list;
    list.readConfig(config);

    m_initialExceptions = cloneExceptionList(list.exceptions());
    m_model->set(cloneExceptionList(list.exceptions()));
    updateButtons();
}

void ExceptionListWidget::save()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("kdecoration_materialrc"));
    ExceptionList list;
    list.setExceptions(m_model->exceptions());
    list.writeConfig(config);

    m_initialExceptions = cloneExceptionList(m_model->exceptions());
    emit changed(false);
}

void ExceptionListWidget::defaults()
{
    m_model->set({});
    emit changed(isChanged());
}

bool ExceptionListWidget::isChanged() const
{
    const auto &current = m_model->exceptions();
    if (current.size() != m_initialExceptions.size()) {
        return true;
    }

    for (int i = 0; i < current.size(); ++i) {
        const auto &a = current.at(i);
        const auto &b = m_initialExceptions.at(i);

        if (a->exceptionPattern() != b->exceptionPattern() ||
            a->exceptionType() != b->exceptionType() ||
            a->matchingMode() != b->matchingMode() ||
            a->enabled() != b->enabled() ||
            a->mask() != b->mask() ||
            a->hideTitleBar() != b->hideTitleBar() ||
            a->hideApplicationMenu() != b->hideApplicationMenu() ||
            a->hamburgerMenu() != b->hamburgerMenu() ||
            a->hideShadow() != b->hideShadow() ||
            a->squareCorners() != b->squareCorners()) {
            return true;
        }
    }

    return false;
}

} // namespace Material
