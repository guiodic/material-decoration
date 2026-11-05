/*
 * Copyright (C) 2025 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "AppMenuSearchMatching.h"

#include <QElapsedTimer>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

namespace Material::SearchMatching
{
static constexpr int MAX_QUERY_TOKENS = 6;
static constexpr qint64 FUZZY_SEARCH_BUDGET_NS = 17'000'000;

/**
 * @brief Splits a string into lowercased word tokens based on whitespace, punctuation, and camelCase boundaries.
 */
QStringList tokenizeText(const QString &text)
{
    QStringList tokens;
    if (text.isEmpty()) {
        return tokens;
    }

    QString current;
    current.reserve(32);

    const int len = text.length();
    for (int i = 0; i < len; ++i) {
        const QChar ch = text.at(i);
        const bool isLetterOrDigit = ch.isLetterOrNumber();

        if (!isLetterOrDigit) {
            if (!current.isEmpty()) {
                tokens.append(current.toLower());
                current.clear();
            }
            continue;
        }

        // Handle camelCase and acronym-to-word transitions (e.g. "saveAs" -> "save", "As"; "XMLParser" -> "XML", "Parser")
        if (ch.isLower() && current.length() > 1 &&
            current.at(current.length() - 1).isUpper() &&
            current.at(current.length() - 2).isUpper()) {
            tokens.append(current.left(current.length() - 1).toLower());
            current = current.right(1);
        } else if (ch.isUpper() && !current.isEmpty() && current.at(current.length() - 1).isLower()) {
            tokens.append(current.toLower());
            current.clear();
        }

        current.append(ch);
    }

    if (!current.isEmpty()) {
        tokens.append(current.toLower());
    }

    return tokens;
}

/**
 * @brief Computes max allowed edit distance (typos) for a token of given length.
 */
static inline int maxAllowedDistance(int tokenLength)
{
    if (tokenLength < 4) {
        return 0; // Very short tokens (1-3 chars) require exact prefix or match
    }
    if (tokenLength == 4) {
        return 1; // E.g., "colr" (4) -> "color" (1 edit)
    }
    return 2; // 5+ char tokens allow up to 2 edits (e.g., "colowr" -> "colori")
}

bool searchExpired(const QElapsedTimer &timer)
{
    // An unstarted timer explicitly requests unbounded scoring, e.g. when
    // checking scores independently of wall-clock scheduling in unit tests.
    return timer.isValid() && timer.nsecsElapsed() >= FUZZY_SEARCH_BUDGET_NS;
}

/**
 * @brief Bounded Damerau-Levenshtein distance calculation (insertions, deletions, substitutions, transpositions).
 * Returns distance if <= maxDistance, otherwise returns maxDistance + 1.
 */
static int damerauLevenshteinDistance(const QString &s1, const QString &s2, int maxDistance, const QElapsedTimer &searchTimer)
{
    const int len1 = s1.length();
    const int len2 = s2.length();

    if (std::abs(len1 - len2) > maxDistance) {
        return maxDistance + 1;
    }
    if (len1 == 0) {
        return len2 <= maxDistance ? len2 : maxDistance + 1;
    }
    if (len2 == 0) {
        return len1 <= maxDistance ? len1 : maxDistance + 1;
    }

    // Three row buffers rotated via pointers to avoid vector allocations/copies
    std::vector<int> r0(len2 + 1, 0);
    std::vector<int> r1(len2 + 1, 0);
    std::vector<int> r2(len2 + 1, 0);

    int *row0 = r0.data();
    int *row1 = r1.data();
    int *row2 = r2.data();

    for (int j = 0; j <= len2; ++j) {
        row1[j] = j;
    }

    for (int i = 1; i <= len1; ++i) {
        // Poll sparsely using the existing indices; time is the only work limit.
        if ((i & 15) == 1 && searchExpired(searchTimer)) return maxDistance + 1;
        row0[0] = i;
        int minRowVal = row0[0];

        const QChar char1 = s1.at(i - 1);

        for (int j = 1; j <= len2; ++j) {
            if ((j & 255) == 0 && searchExpired(searchTimer)) return maxDistance + 1;
            const QChar char2 = s2.at(j - 1);
            const int cost = (char1 == char2) ? 0 : 1;

            int val = std::min({row1[j] + 1, row0[j - 1] + 1, row1[j - 1] + cost});

            // Check adjacent transposition
            if (i > 1 && j > 1 && char1 == s2.at(j - 2) && s1.at(i - 2) == char2) {
                val = std::min(val, row2[j - 2] + cost);
            }

            row0[j] = val;
            minRowVal = std::min(minRowVal, val);
        }

        if (minRowVal > maxDistance) {
            return maxDistance + 1;
        }

        // Pointer rotation: row2 becomes previous row1, row1 becomes current row0, row0 becomes old row2
        int *tmp = row2;
        row2 = row1;
        row1 = row0;
        row0 = tmp;
    }

    return row1[len2];
}

/**
 * @brief Calculates a fuzzy matching score between a search pattern and text using Google/Spotlight style word/token matching.
 *
 * Scoring rules:
 * 1. Contiguous exact substring match fast-path (O(L)) with word boundary bonuses (10000+ base score).
 *    Note: The contiguous exact substring match check intentionally runs BEFORE checking MAX_QUERY_TOKENS.
 *    This allows long exact phrases (e.g. pasted multi-word menu titles) to match instantly without token cap limits,
 *    since linear substring matching incurs no combinatorial assignment overhead.
 * 2. Tokenized word matching: query tokens must match target words via exact match, prefix match, or bounded edit distance.
 *    Non-contiguous token matching requires queryTokens.size() <= MAX_QUERY_TOKENS (6) to bound worst-case backtracking.
 * 3. Substring matching (`contains`) requires query token length >= 3 to prevent noise from 1-2 char tokens.
 * 4. Ghost result elimination: eliminates sparse character subsequence matches across unrelated words.
 *
 * @param pattern The raw search pattern string to match
 * @param queryTokens Pre-tokenized query tokens
 * @param text The text or hierarchical path string to search within
 * @param targetTokens Pre-tokenized target words extracted from text/path
 * @return Score value (higher is better), or 0 if pattern does not match
 */
int calculateFuzzyScore(const QString &pattern, const QStringList &queryTokens, const QString &text, const QStringList &targetTokens, const QElapsedTimer &searchTimer)
{
    if (searchExpired(searchTimer)) return 0;
    if (pattern.isEmpty() || text.isEmpty() || queryTokens.isEmpty()) {
        return 0;
    }

    const int patternLen = pattern.length();

    // 1. Contiguous exact substring match check (10000+ tier ensures contiguous phrase hits rank above token-by-token matches)
    const int exactIdx = text.indexOf(pattern, 0, Qt::CaseInsensitive);
    if (exactIdx != -1) {
        int score = 10000 + (100 * patternLen) - (exactIdx * 2);
        if (exactIdx == 0 || !text.at(exactIdx - 1).isLetterOrNumber()) {
            score += 500; // Word boundary bonus
        }
        return std::max(1, score);
    }

    // 2. Token-based word and prefix matching
    if (queryTokens.size() > MAX_QUERY_TOKENS || targetTokens.isEmpty()) {
        return 0; // Reject non-contiguous token queries exceeding query token limit
    }

    const int numQ = queryTokens.size();
    const int numT = targetTokens.size();

    // Pre-calculate pairwise match scores between each query token and target token
    const std::size_t allocSize = static_cast<std::size_t>(numQ) * static_cast<std::size_t>(numT);
    std::vector<int> pairwiseScores(allocSize, 0);
    std::array<int, MAX_QUERY_TOKENS> maxPairwiseScore{};

    for (int qIdx = 0; qIdx < numQ; ++qIdx) {
        const QString &qToken = queryTokens.at(qIdx);
        const int qLen = qToken.length();
        const int maxDist = maxAllowedDistance(qLen);

        int bestScoreForQ = 0;
        for (int tIdx = 0; tIdx < numT; ++tIdx) {
            if ((tIdx & 31) == 0 && searchExpired(searchTimer)) return 0;
            const QString &tToken = targetTokens.at(tIdx);
            int tokenScore = 0;

            if (qToken == tToken) {
                tokenScore = 1000 + (qLen * 50);
            } else if (tToken.startsWith(qToken)) {
                tokenScore = 700 + (qLen * 40);
            } else if (qLen >= 3 && tToken.contains(qToken)) {
                tokenScore = 500 + (qLen * 20);
            } else {
                // Compare against target word prefixes around qLen (qLen - maxDist to qLen + maxDist)
                const int tLen = tToken.length();
                const int minCompLen = std::max(0, qLen - maxDist);
                const int maxCompLen = std::min(tLen, qLen + maxDist);

                int minDist = maxDist + 1;
                for (int compLen = minCompLen; compLen <= maxCompLen; ++compLen) {
                    const int dist = damerauLevenshteinDistance(qToken, tToken.left(compLen), maxDist, searchTimer);
                    if (dist < minDist) {
                        minDist = dist;
                    }
                }

                if (minDist <= maxDist) {
                    tokenScore = 400 - (minDist * 150) + (qLen * 30);
                }
            }

            const std::size_t flatIdx = static_cast<std::size_t>(qIdx) * static_cast<std::size_t>(numT) + static_cast<std::size_t>(tIdx);
            pairwiseScores[flatIdx] = tokenScore;
            bestScoreForQ = std::max(bestScoreForQ, tokenScore);
        }

        if (bestScoreForQ == 0) {
            return 0; // Short-circuit early if any query token cannot match any target token
        }
        maxPairwiseScore[qIdx] = bestScoreForQ;
    }

    // Pre-calculate upper bound suffix sums for branch-and-bound pruning
    std::array<int, MAX_QUERY_TOKENS + 1> maxSuffixSum{};
    for (int i = numQ - 1; i >= 0; --i) {
        maxSuffixSum[i] = maxSuffixSum[i + 1] + maxPairwiseScore[i];
    }

    // Pre-sort target candidates per query token for greedy-first traversal
    struct TargetCandidate {
        int targetIdx;
        int score;
    };
    std::array<std::vector<TargetCandidate>, MAX_QUERY_TOKENS> sortedCandidates;
    for (int qIdx = 0; qIdx < numQ; ++qIdx) {
        for (int tIdx = 0; tIdx < numT; ++tIdx) {
            if ((tIdx & 31) == 0 && searchExpired(searchTimer)) return 0;
            const std::size_t flatIdx = static_cast<std::size_t>(qIdx) * static_cast<std::size_t>(numT) + static_cast<std::size_t>(tIdx);
            int score = pairwiseScores[flatIdx];
            if (score > 0) {
                sortedCandidates[qIdx].push_back({tIdx, score});
            }
        }
        std::sort(sortedCandidates[qIdx].begin(), sortedCandidates[qIdx].end(), [](const TargetCandidate &a, const TargetCandidate &b) {
            return a.score > b.score;
        });
    }

    // Find complete 1-to-1 distinct assignment using branch-and-bound backtracking
    std::array<int, MAX_QUERY_TOKENS> currentAssignment;
    currentAssignment.fill(-1);

    int maxTotalScore = -1;

    auto backtrackAssignment = [&](auto &self, int qIdx, int currentSum) -> void {
        if (qIdx == numQ) {
            if (currentSum > maxTotalScore) {
                maxTotalScore = currentSum;
            }
            return;
        }

        // currentSum includes order bonuses for the assigned prefix.
        // Each remaining token can add at most one order bonus of 100.
        const int maxRemainingOrderBonus = (numQ - qIdx) * 100;

        // Branch-and-bound pruning: stop search if currentSum + maxSuffixSum[qIdx] + maxRemainingOrderBonus <= maxTotalScore
        if (currentSum + maxSuffixSum[qIdx] + maxRemainingOrderBonus <= maxTotalScore) {
            return;
        }

        for (const auto &cand : sortedCandidates[qIdx]) {
            if (searchExpired(searchTimer)) return;
            const auto assignedEnd = currentAssignment.begin() + qIdx;
            if (std::find(currentAssignment.begin(), assignedEnd, cand.targetIdx) == assignedEnd) {
                currentAssignment[qIdx] = cand.targetIdx;

                const int orderBonus = (qIdx > 0 && cand.targetIdx > currentAssignment[qIdx - 1]) ? 100 : 0;
                self(self, qIdx + 1, currentSum + cand.score + orderBonus);

                currentAssignment[qIdx] = -1;
            }
        }
    };

    backtrackAssignment(backtrackAssignment, 0, 0);

    if (maxTotalScore <= 0) {
        return 0; // No complete distinct 1-to-1 assignment found
    }

    return std::max(1, maxTotalScore);
}

}
