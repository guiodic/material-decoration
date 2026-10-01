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

#include "AppMenuSearch.h"
#include "AppMenuModel.h"

// KF
#include <KLocalizedString>

// Qt
#include <QDebug>
#include <QScopeGuard>
#include <algorithm>
#include <utility>
#include <vector>

static constexpr int MAX_SEARCH_RESULTS = 100;
static constexpr int MAX_SEARCH_CANDIDATES = 5000;
static constexpr int MAX_MENU_DEPTH = 20;
static constexpr int MAX_QUERY_TOKENS = 6;

namespace Material
{

static inline QChar fastToLower(QChar ch)
{
    const ushort u = ch.unicode();
    if (u >= 'A' && u <= 'Z') {
        return QChar(u + 32);
    }
    if (u < 128) {
        return ch;
    }
    return ch.toLower();
}

/**
 * @brief Splits a string into lowercased word tokens based on whitespace, punctuation, and camelCase boundaries.
 */
static QStringList tokenizeText(const QString &text)
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

AppMenuSearch::AppMenuSearch(AppMenuModel *model, QObject *parent)
    : QObject(parent)
    , m_appMenuModel(model)
{
}

AppMenuSearch::~AppMenuSearch() = default;

bool AppMenuSearch::isQueryTooShort(const QString &text)
{
    return text.simplified().length() < MINIMUM_SEARCH_LENGTH;
}

void AppMenuSearch::setSearchMenu(QMenu *searchMenu)
{
    m_searchMenu = searchMenu;
}

void AppMenuSearch::filter(const QString &text, const FilterOptions &options)
{
    // Synchronous lifetime cache guard: the text cache lives 
    // only during the synchronous execution of this filter() call. 
    QPointer<AppMenuSearch> safeThis(this);
    auto clearActionTextCacheGuard = qScopeGuard([safeThis]() {
        if (safeThis) {
            safeThis->m_actionTextCache.clear();
        }
    });

    if (!m_searchMenu) {
        return;
    }

    // Clear results if search text is too short or model is unavailable
    if (isQueryTooShort(text) || !m_appMenuModel) {
        clear();
        resetSearchState();
        Q_EMIT repositionRequested();
        return;
    }
    
    const QString simplifiedText = text.simplified();

    m_lastSearchQuery = simplifiedText;

    {
        // Find results
        rebuildSearchCandidatesIfNeeded();
        QStringMatcher matcher(simplifiedText, Qt::CaseInsensitive);
        QList<SearchResult> results = matchSearchCandidates(matcher, options, simplifiedText);

        // If results and options are the same as last time, do nothing to prevent the freeze.
        if (m_menuIsRendered && m_lastProcessedMenu == m_searchMenu && m_lastResults == results && m_lastOptions == options) {
            return;
        }

        m_lastOptions = options;
        m_lastResults = std::move(results);
        m_lastProcessedMenu = m_searchMenu;
    } // 'results' goes out of scope here to prevent accidental use-after-move

    m_searchMenu->setUpdatesEnabled(false);

    // Clear previous results
    clear();

    // Map each *original* action group to the QActionGroup we create for its
    // search-result proxies, so results that were mutually exclusive in the
    // real menu (e.g. radio-button items) stay mutually exclusive here too.
    QHash<QActionGroup *, QActionGroup *> groupMap;
    for (const SearchResult &result : std::as_const(m_lastResults)) {
        const ActionInfo &info = result.info;
        QAction *action = result.action.data();
        if (!action) {
            continue;
        }
        QAction *newAction = new QAction(action->icon(), info.path, m_searchMenu);
        newAction->setEnabled(info.isEffectivelyEnabled);
        newAction->setCheckable(info.isCheckable);
        newAction->setChecked(info.isChecked);
        newAction->setProperty(PROPERTY_SEARCH_PROXY, true); // Uniquely mark as a proxy result action

        if (QActionGroup *originalGroup = action->actionGroup(); originalGroup && originalGroup->isExclusive()) {
            QActionGroup *&proxyGroup = groupMap[originalGroup];
            if (!proxyGroup) {
                proxyGroup = new QActionGroup(m_searchMenu);
                proxyGroup->setExclusionPolicy(originalGroup->exclusionPolicy());
                m_searchResultGroups.append(proxyGroup);
            }
            proxyGroup->addAction(newAction);
        }
      
        QPointer<QAction> safeAction = action;
        connect(newAction, &QAction::triggered, this, [safeAction, searchMenu = m_searchMenu]() {
            if (safeAction) {
                safeAction->trigger();
            }
            if (searchMenu) {
                searchMenu->hide();
            }
        });
        m_searchMenu->addAction(newAction);
    }

    m_menuIsRendered = true;
    m_searchMenu->setUpdatesEnabled(true);
    Q_EMIT repositionRequested();
}

void AppMenuSearch::clear()
{
    if (!m_searchMenu) {
        return;
    }

    m_menuIsRendered = false;

    const auto actions = m_searchMenu->actions();
    for (QAction *action : actions) {
        if (action && action->property(PROPERTY_SEARCH_PROXY).toBool() == true) {
            m_searchMenu->removeAction(action);
            // Detach action from its group before scheduling deletion
            if (QActionGroup *group = action->actionGroup()) {
                group->removeAction(action);
            }
            action->deleteLater();
        }
    }

    // The old proxy actions no longer reference these groups (deleteLater()
    // above), so nothing else owns them: delete explicitly to avoid leaking
    // one QActionGroup per exclusive result set on every keystroke.
    for (const QPointer<QActionGroup> &oldGroup : std::as_const(m_searchResultGroups)) {
        if (oldGroup) {
            oldGroup->deleteLater();
        }
    }
    m_searchResultGroups.clear();
}

void AppMenuSearch::invalidateCandidates()
{
    m_searchCandidatesDirty = true;
    m_candidateTruncationLogged = false;
    m_searchCandidates.clear();
    m_actionTextCache.clear();
    // Note: m_lastSearchQuery is intentionally preserved here so that
    // hasValidQuery() still reports the in-progress query (e.g. while a
    // submenu is loading), letting the debounce timer re-run the search.
    m_lastResults.clear();
    m_lastProcessedMenu = nullptr;
    m_lastOptions = FilterOptions();
}

bool AppMenuSearch::hasValidQuery() const
{
    return !m_lastSearchQuery.isEmpty() && !isQueryTooShort(m_lastSearchQuery);
}

void AppMenuSearch::reset()
{
    clear();
    resetSearchState();
    m_actionTextCache.clear();
}

void AppMenuSearch::resetSearchState()
{
    m_lastSearchQuery.clear();
    m_lastResults.clear();
    m_lastProcessedMenu = nullptr;
    m_lastOptions = FilterOptions();
}

void AppMenuSearch::rebuildSearchCandidatesIfNeeded()
{
    if (!m_searchCandidatesDirty) {
        return;
    }
    m_searchCandidates.clear();
    m_searchCandidates.reserve(MAX_SEARCH_CANDIDATES);
    m_candidateTruncationLogged = false;

    if (!m_appMenuModel) {
        return;
    }
    QMenu *rootMenu = m_appMenuModel->menu();
    if (!rootMenu) {
        return;
    }

    m_searchCandidatesDirty = false;
    QSet<QMenu *> visited;
    QList<QPointer<QAction>> ancestors;
    collectSearchCandidates(rootMenu, visited, ancestors);
}

void AppMenuSearch::collectSearchCandidates(QMenu *menu, QSet<QMenu *> &visited, QList<QPointer<QAction>> &ancestors, bool hasNamedAncestor)
{
    if (!menu || m_searchCandidates.size() >= MAX_SEARCH_CANDIDATES || ancestors.size() >= MAX_MENU_DEPTH) {
        return;
    }
    const int oldSize = visited.size();
    visited.insert(menu);
    if (visited.size() == oldSize) {
        return;
    }

    QAction *menuAction = menu->menuAction();
    bool addedAncestor = false;
    bool childHasNamedAncestor = hasNamedAncestor;
    if (menuAction) {
        ancestors.append(menuAction);
        addedAncestor = true;
        if (!getActionText(menuAction).isEmpty()) {
            childHasNamedAncestor = true;
        }
        connect(menuAction, &QAction::changed, this, &AppMenuSearch::invalidateCandidates, Qt::UniqueConnection);
    }

    QString parentFullPath;
    QString parentEvalPath;
    QStringList parentFullTokens;
    QStringList parentEvalTokens;
    bool pathsComputed = false;

    auto ensurePathsComputed = [&]() {
        if (pathsComputed) {
            return;
        }
        pathsComputed = true;
        parentFullPath.reserve(64);
        parentEvalPath.reserve(64);

        bool firstFull = true;
        bool firstEval = true;
        bool skippedTopLevel = false;

        for (QAction *ancestor : ancestors) {
            if (ancestor) {
                const QString text = getActionText(ancestor);
                if (!text.isEmpty()) {
                    const QStringList ancestorTokens = tokenizeText(text);
                    if (!firstFull) {
                        parentFullPath.append(QStringLiteral(" » "));
                    }
                    parentFullPath.append(text);
                    parentFullTokens.append(ancestorTokens);
                    firstFull = false;

                    if (!skippedTopLevel) {
                        skippedTopLevel = true;
                    } else {
                        if (!firstEval) {
                            parentEvalPath.append(QStringLiteral(" » "));
                        }
                        parentEvalPath.append(text);
                        parentEvalTokens.append(ancestorTokens);
                        firstEval = false;
                    }
                }
            }
        }
    };

    for (QAction *action : menu->actions()) {
        if (!action || !action->isVisible()) {
            continue;
        }
        if (m_searchCandidates.size() >= MAX_SEARCH_CANDIDATES) {
            if (!m_candidateTruncationLogged) {
                qWarning() << "AppMenuSearch: Maximum search candidates limit reached (" << MAX_SEARCH_CANDIDATES << "), remaining candidates will be discarded";
                m_candidateTruncationLogged = true;
            }
            break;
        }
        if (action->isSeparator()) {
            continue;
        }
        if (action->menu()) {
            collectSearchCandidates(action->menu(), visited, ancestors, childHasNamedAncestor);
        } else {
            ensurePathsComputed();
            const QString itemText = getActionText(action);
            const QStringList itemTokens = tokenizeText(itemText);
            m_searchCandidates.append({action, ancestors, childHasNamedAncestor, parentFullPath, parentEvalPath, itemTokens, parentFullTokens, parentEvalTokens});
        }
    }

    if (addedAncestor) {
        ancestors.removeLast();
    }
}

bool AppMenuSearch::matchesAncestorsOrText(const SearchCandidate &candidate, const QString &itemText, const QStringMatcher &matcher, bool ignoreTopLevel, MatchContext &context) const
{
    // 1. O(1) Fast-Path: check if the direct parent menu's path evaluation is already cached.
    // Safe within this search pass: collectSearchCandidates() visits every QMenu
    // at most once, so each submenu action has a unique root-to-parent path.
    QAction *lastAncestor = candidate.ancestors.isEmpty() ? nullptr : candidate.ancestors.last();
    if (lastAncestor) {
        auto it = context.pathMatchCache.find(lastAncestor);
        if (it != context.pathMatchCache.end()) {
            if (it.value()) {
                return true;
            }
            if (!ignoreTopLevel || candidate.hasNamedAncestor) {
                if (matcher.indexIn(itemText) != -1) {
                    return true;
                }
            }
            return false;
        }
    }

    // 2. Fallback path: Evaluate sequentially and cache individual elements
    bool isTopLevelAncestor = true;
    bool anyAncestorMatched = false;

    for (QAction *ancestor : candidate.ancestors) {
        if (!ancestor) {
            continue;
        }

        auto it = context.matchCache.find(ancestor);
        QString ancestorText;
        bool matched = false;

        if (it != context.matchCache.end()) {
            ancestorText = it.value().text;
            matched = it.value().matched;
        } else {
            ancestorText = getActionText(ancestor);
            matched = (matcher.indexIn(ancestorText) != -1);
            context.matchCache.insert(ancestor, {ancestorText, matched});
        }

        if (ancestorText.isEmpty()) {
            continue;
        }

        // If ignoreTopLevel is true, the first non-empty ancestor is skipped from evaluation.
        // This ensures anyAncestorMatched remains false for the top-level menu match,
        // correctly preventing children of the top-level menu from matching solely due to their parent.
        if (ignoreTopLevel && isTopLevelAncestor) {
            isTopLevelAncestor = false;
            continue;
        }
        isTopLevelAncestor = false;

        if (matched) {
            anyAncestorMatched = true;
            break; // Stop evaluating further ancestors since we found a match
        }
    }

    // Cache the cumulative root-to-parent match result for this submenu action.
    if (lastAncestor) {
        Q_ASSERT(!context.pathMatchCache.contains(lastAncestor));
        context.pathMatchCache.insert(lastAncestor, anyAncestorMatched);
    }

    if (anyAncestorMatched) {
        return true;
    }

    if (!ignoreTopLevel || candidate.hasNamedAncestor) {
        if (matcher.indexIn(itemText) != -1) {
            return true;
        }
    }

    return false;
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

/**
 * @brief Bounded Damerau-Levenshtein distance calculation (insertions, deletions, substitutions, transpositions).
 * Returns distance if <= maxDistance, otherwise returns maxDistance + 1.
 */
static int damerauLevenshteinDistance(const QString &s1, const QString &s2, int maxDistance)
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
        row0[0] = i;
        int minRowVal = row0[0];

        const QChar char1 = fastToLower(s1.at(i - 1));

        for (int j = 1; j <= len2; ++j) {
            const QChar char2 = fastToLower(s2.at(j - 1));
            const int cost = (char1 == char2) ? 0 : 1;

            int val = std::min({row1[j] + 1, row0[j - 1] + 1, row1[j - 1] + cost});

            // Check adjacent transposition
            if (i > 1 && j > 1 && char1 == fastToLower(s2.at(j - 2)) && fastToLower(s1.at(i - 2)) == char2) {
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
 * 1. Contiguous exact substring match fast-path with word boundary bonuses (5000+ base score to guarantee top rank over token matches).
 * 2. Tokenized word matching: query tokens must match target words via exact match, prefix match, or bounded edit distance.
 * 3. Substring matching (`contains`) requires query token length >= 3 to prevent noise from 1-2 char tokens.
 * 4. Ghost result elimination: eliminates sparse character subsequence matches across unrelated words.
 *
 * @param pattern The search pattern to match
 * @param queryTokens Pre-tokenized query tokens
 * @param text The text to search within
 * @return Score value (higher is better), or 0 if pattern does not match
 */
static int calculateFuzzyScore(const QString &pattern, const QStringList &queryTokens, const QString &text, const QStringList &targetTokens)
{
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
    constexpr int MAX_T_TOKENS = 32;
    const int numT = std::min<int>(targetTokens.size(), MAX_T_TOKENS);

    // Pre-calculate pairwise match scores between each query token and target token using stack arrays
    int pairwiseScores[MAX_QUERY_TOKENS][MAX_T_TOKENS] = {};
    int maxPairwiseScore[MAX_QUERY_TOKENS] = {};

    for (int qIdx = 0; qIdx < numQ; ++qIdx) {
        const QString &qToken = queryTokens.at(qIdx);
        const int qLen = qToken.length();
        const int maxDist = maxAllowedDistance(qLen);

        int bestScoreForQ = 0;
        for (int tIdx = 0; tIdx < numT; ++tIdx) {
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
                    const int dist = damerauLevenshteinDistance(qToken, tToken.left(compLen), maxDist);
                    if (dist < minDist) {
                        minDist = dist;
                    }
                }

                if (minDist <= maxDist) {
                    tokenScore = 400 - (minDist * 150) + (qLen * 30);
                }
            }

            pairwiseScores[qIdx][tIdx] = tokenScore;
            bestScoreForQ = std::max(bestScoreForQ, tokenScore);
        }

        if (bestScoreForQ == 0) {
            return 0; // Short-circuit early if any query token cannot match any target token
        }
        maxPairwiseScore[qIdx] = bestScoreForQ;
    }

    // Pre-calculate upper bound suffix sums for branch-and-bound pruning
    int maxSuffixSum[MAX_QUERY_TOKENS + 1] = {};
    for (int i = numQ - 1; i >= 0; --i) {
        maxSuffixSum[i] = maxSuffixSum[i + 1] + maxPairwiseScore[i];
    }

    // Pre-sort target candidates per query token for greedy-first traversal
    struct TargetCandidate {
        int targetIdx;
        int score;
    };
    std::vector<TargetCandidate> sortedCandidates[MAX_QUERY_TOKENS];
    for (int qIdx = 0; qIdx < numQ; ++qIdx) {
        for (int tIdx = 0; tIdx < numT; ++tIdx) {
            int score = pairwiseScores[qIdx][tIdx];
            if (score > 0) {
                sortedCandidates[qIdx].push_back({tIdx, score});
            }
        }
        std::sort(sortedCandidates[qIdx].begin(), sortedCandidates[qIdx].end(), [](const TargetCandidate &a, const TargetCandidate &b) {
            return a.score > b.score;
        });
    }

    // Find complete 1-to-1 distinct assignment using branch-and-bound backtracking
    int currentAssignment[MAX_QUERY_TOKENS];
    int bestAssignment[MAX_QUERY_TOKENS];
    std::fill(currentAssignment, currentAssignment + MAX_QUERY_TOKENS, -1);
    std::fill(bestAssignment, bestAssignment + MAX_QUERY_TOKENS, -1);
    bool usedTarget[MAX_T_TOKENS] = {};
    int maxTotalScore = -1;

    auto backtrackAssignment = [&](auto &self, int qIdx, int currentSum) -> void {
        if (qIdx == numQ) {
            if (currentSum > maxTotalScore) {
                maxTotalScore = currentSum;
                for (int k = 0; k < numQ; ++k) {
                    bestAssignment[k] = currentAssignment[k];
                }
            }
            return;
        }

        // Branch-and-bound pruning: stop search if currentSum + best possible remaining score cannot exceed maxTotalScore
        if (currentSum + maxSuffixSum[qIdx] <= maxTotalScore) {
            return;
        }

        for (const auto &cand : sortedCandidates[qIdx]) {
            if (!usedTarget[cand.targetIdx]) {
                usedTarget[cand.targetIdx] = true;
                currentAssignment[qIdx] = cand.targetIdx;

                self(self, qIdx + 1, currentSum + cand.score);

                usedTarget[cand.targetIdx] = false;
                currentAssignment[qIdx] = -1;
            }
        }
    };

    backtrackAssignment(backtrackAssignment, 0, 0);

    if (maxTotalScore <= 0) {
        return 0; // No complete distinct 1-to-1 assignment found
    }

    int totalScore = maxTotalScore;

    // Add sequential ordering bonus in original query token order
    for (int i = 1; i < numQ; ++i) {
        if (bestAssignment[i] > bestAssignment[i - 1]) {
            totalScore += 100;
        }
    }

    return std::max(1, totalScore);
}

QString AppMenuSearch::buildFullPath(const SearchCandidate &candidate, const QString &itemText) const
{
    if (candidate.parentFullPath.isEmpty()) {
        return itemText;
    }
    return candidate.parentFullPath + QStringLiteral(" » ") + itemText;
}

QString AppMenuSearch::buildEvalPath(const SearchCandidate &candidate, const QString &itemText, bool ignoreTopLevel) const
{
    const QString &prefix = ignoreTopLevel ? candidate.parentEvalPath : candidate.parentFullPath;
    if (prefix.isEmpty()) {
        return itemText;
    }
    return prefix + QStringLiteral(" » ") + itemText;
}

QList<AppMenuSearch::SearchResult> AppMenuSearch::matchSearchCandidates(const QStringMatcher &matcher, const FilterOptions &options, const QString &query) const
{
    QList<SearchResult> results;
    QHash<QAction *, MatchState> matchCache;
    QHash<QAction *, bool> pathMatchCache;
    MatchContext context{matchCache, pathMatchCache};

    const bool ignoreTopLevel = options.ignoreTopLevel;
    const bool ignoreSubMenus = options.ignoreSubMenus;
    const bool showDisabledActions = options.showDisabledActions;
    const bool fuzzyMatching = options.fuzzyMatching;
    const QStringList queryTokens = fuzzyMatching ? tokenizeText(query) : QStringList();

    for (const SearchCandidate &candidate : std::as_const(m_searchCandidates)) {
        if (!fuzzyMatching && results.size() >= MAX_SEARCH_RESULTS) {
            break;
        }
        QAction *action = candidate.action;
        if (!action) {
            continue; // Action was destroyed since the cache was built.
        }

        bool isEffectivelyEnabled = action->isEnabled();
        bool ancestorsStillValid = true;
        for (const auto &ancestor : candidate.ancestors) {
            if (!ancestor) {
                ancestorsStillValid = false;
                break;
            }
            if (!ancestor->isEnabled()) {
                isEffectivelyEnabled = false;
                // Early exit optimization. If an ancestor is disabled and showDisabledActions is false, 
                // `isEffectivelyEnabled` is set to false, meaning the search candidate will be skipped. 
                // Immediately break out of the ancestor loop since further iterations cannot change 
                // this outcome.
                if (!showDisabledActions) {
                    break;
                }
            }
        }

        if (!ancestorsStillValid) {
            continue; // A submenu in the path was rebuilt/destroyed; skip until next full rebuild.
        }

        if (!isEffectivelyEnabled && !showDisabledActions) {
            continue;
        }

        const QString itemText = getActionText(action);
        bool match = false;
        int candidateScore = 0;

        if (fuzzyMatching) {
            if (ignoreTopLevel && !candidate.hasNamedAncestor) {
                match = false;
            } else if (ignoreSubMenus) {
                candidateScore = calculateFuzzyScore(query, queryTokens, itemText, candidate.itemTokens);
                match = (candidateScore > 0);
            } else {
                const int itemScore = calculateFuzzyScore(query, queryTokens, itemText, candidate.itemTokens);
                if (itemScore > 0) {
                    candidateScore = itemScore + 500;
                    match = true;
                } else {
                    const QString evalPath = buildEvalPath(candidate, itemText, ignoreTopLevel);
                    const QStringList evalTokens = (ignoreTopLevel ? candidate.parentEvalTokens : candidate.parentFullTokens) + candidate.itemTokens;
                    const int pathScore = calculateFuzzyScore(query, queryTokens, evalPath, evalTokens);
                    if (pathScore > 0) {
                        candidateScore = pathScore;
                        match = true;
                    }
                }
            }
        } else {
            if (ignoreSubMenus) {
                if (ignoreTopLevel && !candidate.hasNamedAncestor) {
                    match = false;
                } else {
                    match = (matcher.indexIn(itemText) != -1);
                }
            } else {
                match = matchesAncestorsOrText(candidate, itemText, matcher, ignoreTopLevel, context);
            }
        }

        if (!match) {
            continue;
        }

        ActionInfo info;
        info.label = itemText;
        info.isEffectivelyEnabled = isEffectivelyEnabled;
        info.isChecked = action->isChecked();
        info.isCheckable = action->isCheckable();
        info.path = buildFullPath(candidate, itemText);

        results.append({action, info, action->icon().cacheKey(), candidateScore});
    }

    if (fuzzyMatching) {
        std::stable_sort(results.begin(), results.end(), [](const SearchResult &a, const SearchResult &b) {
            return a.score > b.score;
        });
        if (results.size() > MAX_SEARCH_RESULTS) {
            results.resize(MAX_SEARCH_RESULTS);
        }
    }

    return results;
}

QString AppMenuSearch::getActionText(QAction *action) const
{
    if (!action) {
        return QString();
    }
    QString &cachedText = m_actionTextCache[action];
    if (!cachedText.isNull()) {
        return cachedText;
    }
    const QString rawText = action->text();
    const QString cleanedText = KLocalizedString::removeAcceleratorMarker(rawText.trimmed());
    cachedText = cleanedText.isNull() ? QStringLiteral("") : cleanedText;
    return cachedText;
}

} // namespace Material
