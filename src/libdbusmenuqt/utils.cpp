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

namespace
{
bool hasFileExtension(QStringView label, qsizetype underscore)
{
    static const QRegularExpression extension(QStringLiteral("\\.[^\\s./\\\\\\[\\]]+$"));
    const auto matchesFilename = [underscore](QStringView candidate) {
        const QStringView text = candidate.trimmed();
        static const QRegularExpression drive(QStringLiteral("^[A-Za-z]:[\\\\/]"));
        const bool absolutePath = text.startsWith(QLatin1Char('/'))
            || text.startsWith(QLatin1String("\\\\")) || drive.matchView(text).hasMatch();
        const qsizetype separator = qMax(candidate.lastIndexOf(QLatin1Char('/')),
                                        candidate.lastIndexOf(QLatin1Char('\\')));
        // Preserve directory underscores only when the candidate itself is an absolute path.
        return (absolutePath || underscore > separator) && underscore < candidate.size()
            && extension.matchView(text).hasMatch();
    };
    // Recent-file entries can append a path or other details after " [".
    const qsizetype details = label.indexOf(QLatin1String(" ["));
    return matchesFilename(label)
        || (details > 0 && matchesFilename(label.first(details)));
}
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
        && !label.contains(QLatin1String("__"))
        && hasFileExtension(QStringView(label), underscore)) {
        QString text = label;
        text.replace(QLatin1Char('&'), QLatin1String("&&"));
        return text;
    }
    return swapMnemonicChar(label, '_', '&');
}
