/* This file is part of the dbusmenu-qt library
    SPDX-FileCopyrightText: 2010 Canonical
    SPDX-FileContributor: Aurelien Gateau <aurelien.gateau@canonical.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "utils_p.h"

// Qt
#include <QString>

QString swapMnemonicChar(const QString &in, const char src, const char dst)
{
    const int len = in.length();
    if (len == 0) {
        return in;
    }

    const QChar qsrc = QLatin1Char(src);
    const QChar qdst = QLatin1Char(dst);

    // Optimization: find first character that needs processing
    int first = 0;
    const QChar *const data = in.constData();
    while (first < len && data[first] != qsrc && data[first] != qdst) {
        ++first;
    }

    if (first == len) {
        return in;
    }

    QString out;
    out.reserve(len + 4);
    out.append(in.constData(), first);

    bool mnemonicFound = false;

    for (int pos = first; pos < len; ++pos) {
        const QChar ch = data[pos];
        if (ch == qsrc) {
            if (pos == len - 1) {
                // 'src' at the end of string, keep it
                out.append(qsrc);
            } else if (data[pos + 1] == qsrc) {
                // A real 'src'
                out.append(qsrc);
                ++pos;
            } else {
                bool isMnemonic = !mnemonicFound;

                // Heuristic: if followed by a digit and preceded by an alphanumeric char,
                // it's probably part of an identifier (like video_1) rather than a mnemonic.
                if (isMnemonic && data[pos + 1].isDigit() && pos > 0 && data[pos - 1].isLetterOrNumber()) {
                    isMnemonic = false;
                }

                if (isMnemonic) {
                    mnemonicFound = true;
                    out.append(qdst);
                } else {
                    out.append(qsrc);
                }
            }
        } else if (ch == qdst) {
            // Escape 'dst'
            out.append(qdst);
            out.append(qdst);
        } else {
            out.append(ch);
        }
    }

    return out;
}
