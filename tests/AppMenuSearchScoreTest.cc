/*
 * Copyright (C) 2026 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 or any later version.
 */

#include "AppMenuSearchScore.h"

#include <QDebug>

#include <cstdlib>

using Material::AppMenuSearchPrivate::calculateFuzzyScore;

static bool expectScore(const QString &pattern, const QString &text, int expected)
{
    const int actual = calculateFuzzyScore(pattern, text, pattern.toLower());
    if (actual == expected) {
        return true;
    }

    qCritical() << "Expected fuzzy score" << expected << "but got" << actual
                << "for pattern" << pattern << "and text" << text;
    return false;
}

int main()
{
    // U+0130 lowercases to two UTF-16 units (i + combining dot). Matches
    // after it must still use their original positions for scoring bonuses.
    const QString text = QStringLiteral("a\u0130bCd");

    const bool preservesCamelCaseBonus = expectScore(QStringLiteral("\u0130cd"), text, 159);
    const bool avoidsShiftedCamelCaseBonus = expectScore(QStringLiteral("\u0130bd"), text, 139);
    const bool preservesBoundaryBonus = expectScore(QStringLiteral("\u0130cd"), QStringLiteral("a\u0130b cd"), 168);

    return preservesCamelCaseBonus && avoidsShiftedCamelCaseBonus && preservesBoundaryBonus ? EXIT_SUCCESS : EXIT_FAILURE;
}
