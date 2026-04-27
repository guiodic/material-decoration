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
    QString out;
    out.reserve(in.length());
    bool mnemonicFound = false;

    for (int pos = 0; pos < in.length();) {
        QChar ch = in[pos];
        if (ch == src) {
            if (pos == in.length() - 1) {
                // 'src' at the end of string, keep it
                out += src;
                ++pos;
            } else if (in[pos + 1] == src) {
                // A real 'src'
                out += src;
                pos += 2;
            } else {
                bool isMnemonic = !mnemonicFound;

                // Heuristic: if followed by a digit and preceded by an alphanumeric char,
                // it's probably part of an identifier (like video_1) rather than a mnemonic.
                if (isMnemonic && in[pos + 1].isDigit() && pos > 0 && in[pos - 1].isLetterOrNumber()) {
                    isMnemonic = false;
                }

                if (isMnemonic) {
                    mnemonicFound = true;
                    out += dst;
                    ++pos;
                } else {
                    out += src;
                    ++pos;
                }
            }
        } else if (ch == dst) {
            // Escape 'dst'
            out += dst;
            out += dst;
            ++pos;
        } else {
            out += ch;
            ++pos;
        }
    }

    return out;
}
