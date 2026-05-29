/* This file is part of the dbusmenu-qt library
    SPDX-FileCopyrightText: 2009 Canonical
    SPDX-FileContributor: Aurelien Gateau <aurelien.gateau@canonical.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "dbusmenushortcut_p.h"

// Qt
#include <QKeySequence>
#include <QStringTokenizer>

// STL
#include <utility>

static const int QT_COLUMN = 0;
static const int DM_COLUMN = 1;

struct Row {
    const char *zero;
    const char *one;
};

static const Row table[] = {{"Meta", "Super"},
                            {"Ctrl", "Control"},
                            // Special cases for compatibility with libdbusmenu-glib which uses
                            // "plus" for "+" and "minus" for "-".
                            // cf https://bugs.launchpad.net/libdbusmenu-qt/+bug/712565
                            {"+", "plus"},
                            {"-", "minus"},
                            {nullptr, nullptr}};

static QString translate(QStringView token, int srcCol, int dstCol)
{
    for (const Row *ptr = table; ptr->zero != nullptr; ++ptr) {
        const char *from = (srcCol == QT_COLUMN ? ptr->zero : ptr->one);
        if (token == QLatin1String(from)) {
            const char *to = (dstCol == QT_COLUMN ? ptr->zero : ptr->one);
            return QLatin1String(to);
        }
    }
    return {};
}

DBusMenuShortcut DBusMenuShortcut::fromKeySequence(const QKeySequence &sequence)
{
    const QString string = sequence.toString();
    DBusMenuShortcut shortcut;
    for (auto token : QStringTokenizer{string, QLatin1String(", ")}) {
        if (token == QLatin1String("+")) {
            shortcut.append({QLatin1String("plus")});
            continue;
        }

        // Hack: Qt::CTRL | Qt::Key_Plus is turned into the string "Ctrl++",
        // but we don't want the call to token.split() to consider the
        // second '+' as a separator so we handle it by checking if the token
        // ends with "++".
        const bool endsWithPlusPlus = token.endsWith(QLatin1String("++"));
        const QStringView subToken = endsWithPlusPlus ? token.sliced(0, token.size() - 2) : token;

        QStringList keyTokens;
        qsizetype start = 0;
        qsizetype next = -1;
        do {
            next = subToken.indexOf(QLatin1Char('+'), start);
            const auto kt = (next != -1) ? subToken.sliced(start, next - start) : subToken.sliced(start);
            const QString t = translate(kt, QT_COLUMN, DM_COLUMN);
            keyTokens.append(t.isNull() ? kt.toString() : t);
            start = next + 1;
        } while (next != -1);

        if (endsWithPlusPlus) {
            keyTokens.append(QLatin1String("plus"));
        }

        shortcut.append(std::move(keyTokens));
    }
    return shortcut;
}

QKeySequence DBusMenuShortcut::toKeySequence() const
{
    QStringList tmp;
    tmp.reserve(size());
    for (const QStringList &keyTokens : std::as_const(*this)) {
        QStringList translatedTokens;
        translatedTokens.reserve(keyTokens.size());
        for (const QString &token : keyTokens) {
            const QString t = translate(token, DM_COLUMN, QT_COLUMN);
            translatedTokens.append(t.isNull() ? token : t);
        }
        tmp.append(translatedTokens.join(QLatin1Char('+')));
    }
    return QKeySequence::fromString(tmp.join(QLatin1String(", ")));
}
