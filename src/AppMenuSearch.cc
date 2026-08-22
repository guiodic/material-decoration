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
#include <utility>

static constexpr int MAX_SEARCH_RESULTS = 100;
static constexpr int MAX_SEARCH_CANDIDATES = 5000;
static constexpr int MAX_MENU_DEPTH = 20;

namespace Material
{

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
    if (!menu || visited.contains(menu) || m_searchCandidates.size() >= MAX_SEARCH_CANDIDATES || ancestors.size() >= MAX_MENU_DEPTH) {
        return;
    }
    visited.insert(menu);

    QAction *menuAction = menu->menuAction();
    bool addedAncestor = false;
    bool childHasNamedAncestor = hasNamedAncestor;
    if (menuAction) {
        ancestors.append(menuAction);
        addedAncestor = true;
        if (!getActionText(menuAction).isEmpty()) {
            childHasNamedAncestor = true;
        }
    }

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
            m_searchCandidates.append({action, ancestors, childHasNamedAncestor});
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

static int calculateFuzzyScore(const QString &pattern, const QString &text)
{
    if (pattern.isEmpty() || text.isEmpty()) {
        return 0;
    }

    const int patternLen = pattern.length();
    const int textLen = text.length();

    // 1. Contiguous exact substring match check
    const int exactIdx = text.indexOf(pattern, 0, Qt::CaseInsensitive);
    if (exactIdx != -1) {
        int score = 1000 + (100 * patternLen) - (exactIdx * 2);
        if (exactIdx == 0 || !text.at(exactIdx - 1).isLetterOrNumber()) {
            score += 500; // Word boundary bonus
        }
        return score;
    }

    // 2. Sequential character matching & scoring
    int patternIdx = 0;
    int score = 0;
    int consecutive = 0;
    int prevMatchIdx = -1;

    for (int textIdx = 0; textIdx < textLen && patternIdx < patternLen; ++textIdx) {
        const QChar pChar = pattern.at(patternIdx).toLower();
        const QChar tChar = text.at(textIdx).toLower();

        if (pChar == tChar) {
            patternIdx++;
            int charScore = 10;

            const bool isStart = (textIdx == 0);
            const bool isBoundary = (!isStart && !text.at(textIdx - 1).isLetterOrNumber());
            const bool isCamel = (text.at(textIdx).isUpper() && textIdx > 0 && text.at(textIdx - 1).isLower());

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

    if (patternIdx < patternLen) {
        return 0; // Not all pattern characters matched in sequence
    }

    return std::max(1, score);
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

    for (const SearchCandidate &candidate : std::as_const(m_searchCandidates)) {
        if (results.size() >= MAX_SEARCH_RESULTS) {
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

        QStringList currentPath;
        currentPath.reserve(candidate.ancestors.size() + 1);
        for (QAction *ancestor : std::as_const(candidate.ancestors)) {
            if (ancestor) {
                const QString text = getActionText(ancestor);
                if (!text.isEmpty()) {
                    currentPath.append(text);
                }
            }
        }
        currentPath.append(itemText);
        const QString fullPath = currentPath.join(QStringLiteral(" » "));

        if (fuzzyMatching) {
            if (ignoreSubMenus) {
                if (ignoreTopLevel && !candidate.hasNamedAncestor) {
                    match = false;
                } else {
                    candidateScore = calculateFuzzyScore(query, itemText);
                    match = (candidateScore > 0);
                }
            } else {
                if (ignoreTopLevel && !candidate.hasNamedAncestor) {
                    candidateScore = calculateFuzzyScore(query, itemText);
                    match = (candidateScore > 0);
                } else {
                    const int itemScore = calculateFuzzyScore(query, itemText);
                    if (itemScore > 0) {
                        candidateScore = itemScore + 500;
                        match = true;
                    } else {
                        const int pathScore = calculateFuzzyScore(query, fullPath);
                        if (pathScore > 0) {
                            candidateScore = pathScore;
                            match = true;
                        }
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
        info.path = fullPath;

        results.append({action, info, action->icon().cacheKey(), candidateScore});
    }

    if (fuzzyMatching) {
        std::stable_sort(results.begin(), results.end(), [](const SearchResult &a, const SearchResult &b) {
            return a.score > b.score;
        });
    }

    return results;
}

QString AppMenuSearch::getActionText(QAction *action) const
{
    if (!action) {
        return QString();
    }
    auto it = m_actionTextCache.find(action);
    if (it != m_actionTextCache.end()) {
        return it.value();
    }
    const QString rawText = action->text();
    const QString cleanedText = KLocalizedString::removeAcceleratorMarker(rawText.trimmed());
    m_actionTextCache.insert(action, cleanedText);
    return cleanedText;
}

} // namespace Material
