/*
 * Copyright (C) 2026 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 or any later version.
 */

#include "AppMenuSearchScore.h"

#include <QList>

#include <algorithm>

namespace Material::AppMenuSearchPrivate
{

struct LowercaseText {
    QString text;
    QList<qsizetype> originalIndices;
};

static LowercaseText lowercaseWithOriginalIndices(const QString &text)
{
    LowercaseText result;
    result.text = text.toLower();
    result.originalIndices.reserve(result.text.length());

    // Most strings have one-to-one case mappings, so avoid per-character
    // conversions unless lowercasing actually changes the UTF-16 length.
    if (result.text.length() == text.length()) {
        for (qsizetype i = 0; i < text.length(); ++i) {
            result.originalIndices.append(i);
        }
        return result;
    }

    for (qsizetype originalIdx = 0; originalIdx < text.length();) {
        qsizetype sourceLength = 1;
        if (text.at(originalIdx).isHighSurrogate()
            && originalIdx + 1 < text.length()
            && text.at(originalIdx + 1).isLowSurrogate()) {
            sourceLength = 2;
        }

        const qsizetype lowercaseLength = text.mid(originalIdx, sourceLength).toLower().length();
        for (qsizetype i = 0; i < lowercaseLength; ++i) {
            result.originalIndices.append(originalIdx);
        }
        originalIdx += sourceLength;
    }

    Q_ASSERT(result.originalIndices.length() == result.text.length());
    return result;
}

/**
 * Computes a fuzzy match score between a pattern and text.
 *
 * Uses sequential character matching with bonuses for:
 * - Exact substring matches (highest score)
 * - Word boundaries and camelCase matches
 * - Consecutive character matches
 *
 * @param pattern The search pattern to match.
 * @param text The text to search within.
 * @param patternLower Pre-lowercased pattern (optional optimization to avoid redundant toLower() calls).
 * @return Score value (higher is better), or 0 if no match.
 */
int calculateFuzzyScore(const QString &pattern, const QString &text, const QString &patternLower)
{
    if (pattern.isEmpty() || text.isEmpty()) {
        return 0;
    }

    const int patternLen = pattern.length();

    // 1. Contiguous exact substring match check
    const int exactIdx = text.indexOf(pattern, 0, Qt::CaseInsensitive);
    if (exactIdx != -1) {
        int score = 1000 + (100 * patternLen) - (exactIdx * 2);
        if (exactIdx == 0 || !text.at(exactIdx - 1).isLetterOrNumber()) {
            score += 500; // Word boundary bonus
        }
        return std::max(1, score);
    }

    // 2. Sequential character matching & scoring
    int patternIdx = 0;
    int score = 0;
    int consecutive = 0;
    int prevMatchIdx = -1;

    const QString pLower = patternLower.isEmpty() ? pattern.toLower() : patternLower;
    const LowercaseText normalizedText = lowercaseWithOriginalIndices(text);
    const QString &textLower = normalizedText.text;

    const int pLowerLen = pLower.length();
    const int textLowerLen = textLower.length();

    if (pLowerLen == 0 || textLowerLen == 0) {
        return 0;
    }

    QChar pChar = pLower.at(0);

    for (int textIdx = 0; textIdx < textLowerLen && patternIdx < pLowerLen; ++textIdx) {
        const QChar tChar = textLower.at(textIdx);

        if (pChar == tChar) {
            patternIdx++;
            if (patternIdx < pLowerLen) {
                pChar = pLower.at(patternIdx);
            }
            int charScore = 10;

            const qsizetype originalIdx = normalizedText.originalIndices.at(textIdx);
            const bool isStart = (textIdx == 0);
            const bool isFirstNormalizedUnit = (textIdx == 0 || normalizedText.originalIndices.at(textIdx - 1) != originalIdx);
            const bool isBoundary = (isFirstNormalizedUnit && originalIdx > 0 && !text.at(originalIdx - 1).isLetterOrNumber());
            const bool isCamel = (isFirstNormalizedUnit && originalIdx > 0 && text.at(originalIdx).isUpper() && text.at(originalIdx - 1).isLower());

            if (isStart || isBoundary) {
                charScore += 50;
            } else if (isCamel) {
                charScore += 40;
            }

            if (prevMatchIdx != -1 && textIdx == prevMatchIdx + 1) {
                consecutive++;
                charScore += (20 * consecutive);
            } else {
                consecutive = 0;
                if (prevMatchIdx != -1) {
                    charScore -= (textIdx - prevMatchIdx - 1);
                }
            }

            prevMatchIdx = textIdx;
            score += charScore;
        }
    }

    if (patternIdx < pLowerLen) {
        return 0; // Not all pattern characters matched in sequence
    }

    return std::max(1, score);
}

} // namespace Material::AppMenuSearchPrivate
