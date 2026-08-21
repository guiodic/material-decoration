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

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;

namespace Material
{

class DetectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DetectDialog(QWidget *parent = nullptr);
    ~DetectDialog() override = default;

    QString windowClass() const { return m_windowClass; }
    QString caption() const { return m_caption; }

private slots:
    void detectWindow();

private:
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_detectButton = nullptr;

    QString m_windowClass;
    QString m_caption;
};

} // namespace Material
