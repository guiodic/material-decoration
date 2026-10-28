/* This file is part of the dbusmenu-qt library
    SPDX-FileCopyrightText: 2010 Canonical
    SPDX-FileContributor: Aurelien Gateau <aurelien.gateau@canonical.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "utils_p.h"

// Qt
#include <QString>
#include <QStringView>
#include <QRegularExpression>

// STL
#include <algorithm>
#include <iterator>

QString swapMnemonicChar(const QString &in, char src, char dst)
{
    if (in.isEmpty()) {
        return in;
    }

    const QChar qsrc = QLatin1Char(src);
    const QChar qdst = QLatin1Char(dst);

    // Optimization: find first character that needs processing
    const QChar *data = in.constData();
    const QChar *const end = data + in.length();
    const QChar *ptr = std::find_if(data, end, [qsrc, qdst](QChar c) {
        return c == qsrc || c == qdst;
    });

    if (ptr == end) {
        return in;
    }

    QString out;
    out.reserve(in.length() + 4);
    out.append(data, std::distance(data, ptr));

    bool mnemonicFound = false;

    for (; ptr < end; ++ptr) {
        const QChar ch = *ptr;
        if (ch == qsrc) {
            if (ptr + 1 == end) {
                // 'src' at the end of string, keep it
                out.append(qsrc);
            } else if (*(ptr + 1) == qsrc) {
                // A real 'src'
                out.append(qsrc);
                ++ptr;
            } else {
                bool isMnemonic = !mnemonicFound;

                // Heuristic: if followed by a digit and preceded by an alphanumeric char,
                // it's probably part of an identifier (like video_1) rather than a mnemonic.
                if (isMnemonic && (ptr + 1)->isDigit() && ptr > data && (ptr - 1)->isLetterOrNumber()) {
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

QString menuLabelToQt(const QString &label)
{
    const qsizetype underscore = label.indexOf(QLatin1Char('_'));
    // Correctly escaped labels and leading mnemonic markers keep protocol decoding.
    // Qt's exporter leaves literal underscores unescaped, so an internal marker
    // in a filename is ambiguous. Preserve it rather than hiding part of the name.
    if (underscore > 0 && underscore + 1 < label.size()
        && label.at(underscore - 1).isLetterOrNumber()
        && label.at(underscore + 1).isLetterOrNumber()
        && !label.contains(QLatin1String("__"))) {
        const auto isPath = [](QStringView text) {
            static const QRegularExpression drive(QStringLiteral("^[A-Za-z]:[\\\\/]"));
            return text.startsWith(QLatin1Char('/'))
                || text.startsWith(QLatin1String("~/"))
                || text.startsWith(QLatin1String("./"))
                || text.startsWith(QLatin1String("../"))
                || text.startsWith(QLatin1String("file://"))
                || text.startsWith(QLatin1String("\\\\"))
                || drive.matchView(text).hasMatch();
        };
        const QStringView view(label);
        qsizetype hint = -1;
        if (view.endsWith(QLatin1Char(']'))) {
            // Match the whole final bracketed block, not a path nested inside it.
            qsizetype depth = 0;
            for (qsizetype i = view.size() - 1; i >= 0; --i) {
                if (view.at(i) == QLatin1Char(']')) {
                    ++depth;
                } else if (view.at(i) == QLatin1Char('[')) {
                    --depth;
                    if (depth == 0) {
                        if (i > 0 && view.at(i - 1) == QLatin1Char(' ')
                            && isPath(view.sliced(i + 1, view.size() - i - 2))) {
                            hint = i - 1;
                        }
                        break;
                    }
                }
            }
        }
        const bool pathHint = hint >= 0;
        const QStringView name = (pathHint ? view.first(hint) : view).trimmed();
        static const QRegularExpression extension(QStringLiteral("\\.[\\p{L}\\p{N}]+$"));
        if (isPath(name) || pathHint || extension.matchView(name).hasMatch()) {
            QString text = label;
            text.replace(QLatin1Char('&'), QLatin1String("&&"));
            return text;
        }
    }
    return swapMnemonicChar(label, '_', '&');
}
