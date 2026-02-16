/*
 * Copyright (C) 2025 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
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

#include "IconProvider.h"

#include <QStandardPaths>
#include <QFileInfo>

namespace Material
{

QMap<QString, QSharedPointer<QSvgRenderer>> IconProvider::s_renderers;

QSharedPointer<QSvgRenderer> IconProvider::getRenderer(const QString &iconName)
{
    auto it = s_renderers.find(iconName);
    if (it != s_renderers.end()) {
        return it.value();
    }

    const QString path = findIcon(iconName);
    if (path.isEmpty()) {
        return {};
    }

    QSharedPointer<QSvgRenderer> renderer(new QSvgRenderer(path));
    if (renderer->isValid()) {
        s_renderers.insert(iconName, renderer);
        return renderer;
    }

    return {};
}

QString IconProvider::findIcon(const QString &iconName)
{
    const QString fileName = iconName + QStringLiteral(".svg");

    // 1. GenericDataLocation (installed)
    QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("material-decoration/icons/") + fileName);

    // 2. Relative to the current working directory (development/sandbox)
    if (path.isEmpty()) {
        QFileInfo localFile(QStringLiteral("icons/") + fileName);
        if (localFile.exists()) {
            path = localFile.absoluteFilePath();
        }
    }

    return path;
}

} // namespace Material
