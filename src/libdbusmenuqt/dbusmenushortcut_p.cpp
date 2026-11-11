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

static inline QLatin1StringView translate(QStringView token, int srcCol, int dstCol)
{
    for (const Row *ptr = table; ptr->zero != nullptr; ++ptr) {
        const char *from = (srcCol == QT_COLUMN ? ptr->zero : ptr->one);
        if (token == QLatin1StringView(from)) {
            return QLatin1StringView(dstCol == QT_COLUMN ? ptr->zero : ptr->one);
        }
    }
    return {};
}

DBusMenuShortcut DBusMenuShortcut::fromKeySequence(const QKeySequence &sequence)
{
    const QString string = sequence.toString();
    DBusMenuShortcut shortcut;
    for (auto token : QStringTokenizer{string, QLatin1StringView(", "), Qt::SkipEmptyParts}) {
        if (token == QLatin1StringView("+")) {
            shortcut.append({QLatin1String("plus")});
            continue;
        }

        // Qt serializes Ctrl+Plus as "Ctrl++". Preserve the final key while
        // tokenizing the modifier separators.
        const bool endsWithPlusPlus = token.endsWith(QLatin1StringView("++"));
        const QStringView subToken = endsWithPlusPlus ? token.chopped(2) : token;

        QStringList keyTokens;
        keyTokens.reserve(4);
        for (auto kt : QStringTokenizer{subToken, QLatin1Char('+'), Qt::SkipEmptyParts}) {
            if (const auto t = translate(kt, QT_COLUMN, DM_COLUMN); !t.isEmpty()) {
                keyTokens.append(t);
            } else {
                keyTokens.append(kt.toString());
            }
        }

        if (endsWithPlusPlus) {
            keyTokens.append(QLatin1String("plus"));
        }

        shortcut.append(std::move(keyTokens));
    }
    return shortcut;
}

QKeySequence DBusMenuShortcut::toKeySequence() const
{
    QString res;
    // Reserve room for typical modifier/key combinations and separators.
    res.reserve(size() * 24);

    for (const QStringList &keyTokens : std::as_const(*this)) {
        if (keyTokens.isEmpty()) {
            continue;
        }
        if (!res.isEmpty()) {
            res.append(QLatin1StringView(", "));
        }
        bool first = true;
        for (const QString &token : keyTokens) {
            if (!first) {
                res.append(QLatin1Char('+'));
            }
            first = false;
            if (const auto t = translate(token, DM_COLUMN, QT_COLUMN); !t.isEmpty()) {
                res.append(t);
            } else {
                res.append(token);
            }
        }
    }
    return QKeySequence::fromString(res);
}
