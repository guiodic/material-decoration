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

/**
 * @brief Interactive dialog for detecting target window properties (window class and title) via mouse selection.
 *
 * Prompts the user to click a target window on screen, querying KWin via DBus to extract its resource class and title.
 */
class DetectDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a DetectDialog instance.
     * @param parent Optional parent widget.
     */
    explicit DetectDialog(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~DetectDialog() override = default;

    /**
     * @brief Returns detected window class string.
     * @return Window class string.
     */
    QString windowClass() const { return m_windowClass; }

    /**
     * @brief Returns detected window title/caption string.
     * @return Window caption string.
     */
    QString caption() const { return m_caption; }

private slots:
    /**
     * @brief Triggers window picker mode and handles DBus query response from KWin.
     */
    void detectWindow();

private:
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_detectButton = nullptr;

    QString m_windowClass;
    QString m_caption;
};

} // namespace Material
