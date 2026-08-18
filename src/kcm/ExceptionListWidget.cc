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
#include <ui_ExceptionListWidget.h>

#include <KLocalizedString>
#include <QIcon>
#include <QMessageBox>
#include <QPointer>
#include <QRegularExpression>

namespace Material
{

ExceptionListWidget::ExceptionListWidget(QWidget *parent)
    : QWidget(parent)
    , m_ui(std::make_unique<Ui::MaterialExceptionListWidget>())
{
    m_ui->setupUi(this);

    m_ui->exceptionListView->setAllColumnsShowFocus(true);
    m_ui->exceptionListView->setRootIsDecorated(false);
    m_ui->exceptionListView->setSortingEnabled(false);
    m_ui->exceptionListView->setModel(&model());

    m_ui->moveUpButton->setIcon(QIcon::fromTheme(QStringLiteral("arrow-up")));
    m_ui->moveDownButton->setIcon(QIcon::fromTheme(QStringLiteral("arrow-down")));
    m_ui->addButton->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_ui->removeButton->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
    m_ui->editButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-rename")));

    connect(m_ui->addButton, &QPushButton::clicked, this, &ExceptionListWidget::add);
    connect(m_ui->editButton, &QPushButton::clicked, this, &ExceptionListWidget::edit);
    connect(m_ui->removeButton, &QPushButton::clicked, this, &ExceptionListWidget::remove);
    connect(m_ui->moveUpButton, &QPushButton::clicked, this, &ExceptionListWidget::up);
    connect(m_ui->moveDownButton, &QPushButton::clicked, this, &ExceptionListWidget::down);

    connect(m_ui->exceptionListView, &QAbstractItemView::doubleClicked, this, &ExceptionListWidget::edit);
    connect(m_ui->exceptionListView, &QAbstractItemView::clicked, this, &ExceptionListWidget::toggle);
    connect(m_ui->exceptionListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ExceptionListWidget::updateButtons);

    updateButtons();
    resizeColumns();
}

ExceptionListWidget::~ExceptionListWidget() = default;

void ExceptionListWidget::setExceptions(const InternalSettingsList &exceptions)
{
    model().set(exceptions);
    resizeColumns();
    setChanged(false);
}

InternalSettingsList ExceptionListWidget::exceptions()
{
    setChanged(false);
    return model().get();
}

void ExceptionListWidget::updateButtons()
{
    bool hasSelection = !m_ui->exceptionListView->selectionModel()->selectedRows().empty();
    m_ui->removeButton->setEnabled(hasSelection);
    m_ui->editButton->setEnabled(hasSelection);

    m_ui->moveUpButton->setEnabled(hasSelection && !m_ui->exceptionListView->selectionModel()->isRowSelected(0, QModelIndex()));
    m_ui->moveDownButton->setEnabled(hasSelection && !m_ui->exceptionListView->selectionModel()->isRowSelected(model().rowCount() - 1, QModelIndex()));
}

void ExceptionListWidget::add()
{
    QPointer<ExceptionDialog> dialog = new ExceptionDialog(this);
    dialog->setWindowTitle(i18n("New Window-Specific Override"));
    InternalSettingsPtr exception(new InternalSettings());
    exception->load();

    dialog->setException(exception);

    if (!dialog->exec()) {
        delete dialog;
        return;
    }

    dialog->save();
    delete dialog;

    if (!checkException(exception)) {
        return;
    }

    model().add(exception);
    setChanged(true);

    QModelIndex index = model().index(exception);
    if (index.isValid()) {
        m_ui->exceptionListView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_ui->exceptionListView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Current | QItemSelectionModel::Rows);
    }

    resizeColumns();
}

void ExceptionListWidget::edit()
{
    QModelIndex current = m_ui->exceptionListView->selectionModel()->currentIndex();
    if (!model().contains(current)) {
        return;
    }

    InternalSettingsPtr exception = model().get(current);

    QPointer<ExceptionDialog> dialog = new ExceptionDialog(this);
    dialog->setWindowTitle(i18n("Edit Window-Specific Override"));
    dialog->setException(exception);

    if (!dialog->exec()) {
        delete dialog;
        return;
    }

    if (!dialog->isChanged()) {
        delete dialog;
        return;
    }

    dialog->save();
    delete dialog;

    checkException(exception);
    resizeColumns();
    setChanged(true);
}

void ExceptionListWidget::remove()
{
    QMessageBox messageBox(QMessageBox::Question,
                           i18n("Question - Material Settings"),
                           i18n("Remove selected window-specific override?"),
                           QMessageBox::Yes | QMessageBox::Cancel,
                           this);
    messageBox.button(QMessageBox::Yes)->setText(i18n("Remove"));
    messageBox.setDefaultButton(QMessageBox::Cancel);
    if (messageBox.exec() == QMessageBox::Cancel) {
        return;
    }

    model().remove(model().get(m_ui->exceptionListView->selectionModel()->selectedRows()));
    resizeColumns();
    updateButtons();
    setChanged(true);
}

void ExceptionListWidget::toggle(const QModelIndex &index)
{
    if (!model().contains(index) || index.column() != ExceptionModel::ColumnEnabled) {
        return;
    }

    InternalSettingsPtr exception = model().get(index);
    if (exception) {
        exception->setEnabled(!exception->enabled());
        setChanged(true);
    }
}

void ExceptionListWidget::up()
{
    QModelIndexList selectedIndices = m_ui->exceptionListView->selectionModel()->selectedRows();
    if (selectedIndices.empty()) {
        return;
    }

    InternalSettingsList selectedExceptions = model().get(selectedIndices);
    InternalSettingsList currentExceptions = model().get();
    InternalSettingsList newExceptions;

    for (const auto &item : currentExceptions) {
        if (!newExceptions.empty() && selectedIndices.indexOf(model().index(item)) != -1 && selectedIndices.indexOf(model().index(newExceptions.back())) == -1) {
            InternalSettingsPtr last = newExceptions.back();
            newExceptions.removeLast();
            newExceptions.append(item);
            newExceptions.append(last);
        } else {
            newExceptions.append(item);
        }
    }

    model().set(newExceptions);

    if (!selectedExceptions.empty()) {
        m_ui->exceptionListView->selectionModel()->select(model().index(selectedExceptions.front()), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        for (const auto &item : selectedExceptions) {
            m_ui->exceptionListView->selectionModel()->select(model().index(item), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }

    setChanged(true);
}

void ExceptionListWidget::down()
{
    QModelIndexList selectedIndices = m_ui->exceptionListView->selectionModel()->selectedRows();
    if (selectedIndices.empty()) {
        return;
    }

    InternalSettingsList selectedExceptions = model().get(selectedIndices);
    InternalSettingsList currentExceptions = model().get();
    InternalSettingsList newExceptions;

    for (int i = currentExceptions.size() - 1; i >= 0; --i) {
        const auto &current = currentExceptions.at(i);
        if (!newExceptions.empty() && selectedIndices.indexOf(model().index(current)) != -1 && selectedIndices.indexOf(model().index(newExceptions.front())) == -1) {
            InternalSettingsPtr first = newExceptions.front();
            newExceptions.removeFirst();
            newExceptions.prepend(current);
            newExceptions.prepend(first);
        } else {
            newExceptions.prepend(current);
        }
    }

    model().set(newExceptions);

    if (!selectedExceptions.empty()) {
        m_ui->exceptionListView->selectionModel()->select(model().index(selectedExceptions.front()), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        for (const auto &item : selectedExceptions) {
            m_ui->exceptionListView->selectionModel()->select(model().index(item), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }

    setChanged(true);
}

void ExceptionListWidget::resizeColumns() const
{
    m_ui->exceptionListView->resizeColumnToContents(ExceptionModel::ColumnEnabled);
    m_ui->exceptionListView->resizeColumnToContents(ExceptionModel::ColumnType);
    m_ui->exceptionListView->resizeColumnToContents(ExceptionModel::ColumnRegExp);
}

bool ExceptionListWidget::checkException(InternalSettingsPtr exception)
{
    while (exception->exceptionPattern().isEmpty() || !QRegularExpression(exception->exceptionPattern()).isValid()) {
        QMessageBox::warning(this, i18n("Warning - Material Settings"), i18n("Regular Expression syntax is incorrect"));
        QPointer<ExceptionDialog> dialog = new ExceptionDialog(this);
        dialog->setException(exception);
        if (dialog->exec() == QDialog::Rejected) {
            delete dialog;
            return false;
        }
        dialog->save();
        delete dialog;
    }
    return true;
}

void ExceptionListWidget::setChanged(bool value)
{
    m_changed = value;
    emit changed(value);
}

} // namespace Material
