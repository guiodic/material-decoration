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

#include <QObject>
#include <QVariantMap>

namespace Material
{

class DetectDialog : public QObject
{
    Q_OBJECT

public:
    explicit DetectDialog(QObject *parent = nullptr);

    void detect();
    QVariantMap properties() const;

Q_SIGNALS:
    void detectionDone(bool success);

private:
    QVariantMap m_properties;
};

} // namespace Material
