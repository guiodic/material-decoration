/* This file is part of the dbusmenu-qt library
    SPDX-FileCopyrightText: 2009 Canonical
    SPDX-FileContributor: Aurelien Gateau <aurelien.gateau@canonical.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#pragma once

// Qt
#include <QMetaType>
#include <QStringList>

class QKeySequence;

/**
 * @brief Represents a keyboard shortcut sequence transferred over DBus as a list of key modifier strings.
 */
class DBusMenuShortcut : public QList<QStringList>
{
public:
    /**
     * @brief Converts DBus key modifier string representation to Qt QKeySequence.
     * @return Converted QKeySequence.
     */
    QKeySequence toKeySequence() const;

    /**
     * @brief Constructs DBusMenuShortcut representation from Qt QKeySequence.
     * @param sequence Source QKeySequence.
     * @return DBusMenuShortcut instance.
     */
    static DBusMenuShortcut fromKeySequence(const QKeySequence &sequence);
};
