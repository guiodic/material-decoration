// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>
#include <QStringList>

class QElapsedTimer;

namespace Material::SearchMatching
{
QStringList tokenizeText(const QString &text);
bool searchExpired(const QElapsedTimer &timer);
int calculateFuzzyScore(const QString &pattern, const QStringList &queryTokens,
                        const QString &text, const QStringList &targetTokens,
                        const QElapsedTimer &searchTimer);
}
