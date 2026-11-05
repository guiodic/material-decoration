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
#include "AppMenuSearchMatching.h"
#include "AppMenuModel.h"

// KF
#include <KLocalizedString>

// Qt
#include <QDebug>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QStringBuilder>
#include <algorithm>
#include <utility>

static constexpr int MAX_SEARCH_RESULTS = 100;
static constexpr int MAX_SEARCH_CANDIDATES = 5000;
static constexpr int MAX_MENU_DEPTH = 20;

namespace Material
{
using SearchMatching::tokenizeText;
using SearchMatching::calculateFuzzyScore;
using SearchMatching::searchExpired;

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
    if (m_searchMenu == searchMenu) {
        return;
    }

    // Remove proxy actions from the old menu before switching the guarded pointer.
    clear();
    m_searchMenu = searchMenu;
    m_actionTextCache.clear();
    resetSearchState();
    m_menuIsRendered = false;
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
        // A cold cache must not exhaust the matching budget before any action
        // is examined. Cache preparation and rendering are outside this budget.
        QElapsedTimer searchTimer;
        searchTimer.start();
        QStringMatcher matcher(simplifiedText, Qt::CaseInsensitive);
        QList<SearchResult> results = matchSearchCandidates(matcher, options, simplifiedText, searchTimer);

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
        QPointer<QMenu> safeSearchMenu = m_searchMenu;
        connect(newAction, &QAction::triggered, this, [safeAction, safeSearchMenu]() {
            // Triggering the original action may synchronously close the window
            // and destroy the search menu, so guard both objects across the call.
            if (safeAction) {
                safeAction->trigger();
            }
            if (safeSearchMenu) {
                safeSearchMenu->hide();
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
    m_menuIsRendered = false;

    if (m_searchMenu) {
        const auto actions = m_searchMenu->actions();
        for (QAction *action : actions) {
            if (action && action->property(PROPERTY_SEARCH_PROXY).toBool()) {
                m_searchMenu->removeAction(action);
                // Detach action from its group before scheduling deletion
                if (QActionGroup *group = action->actionGroup()) {
                    group->removeAction(action);
                }
                action->deleteLater();
            }
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
    clearSearchResultsState();
    Q_EMIT candidatesInvalidated();
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
    clearSearchResultsState();
}

void AppMenuSearch::clearSearchResultsState()
{
    m_lastResults.clear();
    m_lastProcessedMenu = nullptr;
    m_lastOptions = FilterOptions();
}

void AppMenuSearch::rebuildSearchCandidatesIfNeeded()
{
    if (!m_searchCandidatesDirty) {
        return;
    }

    for (const QPointer<QAction> &action : std::as_const(m_trackedActions)) {
        if (action) {
            disconnect(action.data(), &QAction::changed, this, &AppMenuSearch::onActionChanged);
            disconnect(action.data(), &QAction::visibleChanged, this, &AppMenuSearch::onActionVisibilityChanged);
            disconnect(action.data(), &QObject::destroyed, this, &AppMenuSearch::onTrackedActionDestroyed);
        }
    }
    m_trackedActions.clear();

    for (const QPointer<QMenu> &menu : std::as_const(m_trackedMenus)) {
        if (menu) {
            menu->removeEventFilter(this);
        }
    }
    m_trackedMenus.clear();
    m_trackedActionSet.clear();

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

bool AppMenuSearch::eventFilter(QObject *watched, QEvent *event)
{
    // The search menu contains generated proxy actions, not source candidates.
    // Never let adding/removing those proxies invalidate the source-menu cache.
    if (watched == m_searchMenu) {
        return QObject::eventFilter(watched, event);
    }

    if (event && (event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved)
        && qobject_cast<QMenu *>(watched)) {
        // Multiple membership events can be emitted while a dynamic menu is
        // rebuilt. Once invalidated and cleared, further events need no work
        // until the next search rebuilds the candidate cache.
        if (!m_searchCandidatesDirty || m_menuIsRendered) {
            clear();
            invalidateCandidates();
        }
    }

    return QObject::eventFilter(watched, event);
}

void AppMenuSearch::onActionChanged()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action) {
        return;
    }
    // QAction::changed covers searchable text, submenu associations, and
    // presentation properties copied into rendered proxies. Remove stale
    // proxies synchronously, then coalesce repeated changes while a rebuild
    // is already pending.
    if (!m_searchCandidatesDirty || m_menuIsRendered) {
        clear();
        invalidateCandidates();
    }
}

void AppMenuSearch::onActionVisibilityChanged()
{
    // Visibility affects candidate membership. Remove rendered proxies now so
    // hidden actions cannot remain clickable during the debounce interval.
    if (!m_searchCandidatesDirty || m_menuIsRendered) {
        clear();
        invalidateCandidates();
    }
}

void AppMenuSearch::onTrackedActionDestroyed(QObject *destroyedObject)
{
    Q_UNUSED(destroyedObject);

    // A destroyed action may be a result itself or an ancestor of results
    // collected from one of its submenus. Child actions can outlive their menu,
    // so remove every rendered proxy synchronously rather than only proxies
    // mapped to the destroyed action. clear() detaches proxies from the menu
    // immediately and defers their destruction safely.
    //
    // Teardown can destroy many tracked actions in succession. The first
    // callback clears rendered proxies and invalidates the cache; subsequent
    // callbacks can then return until a fresh candidate rebuild occurs.
    if (m_searchCandidatesDirty && !m_menuIsRendered) {
        return;
    }

    clear();

    // Preserve the current query while invalidating candidates so the existing
    // debounce path can rebuild results from the current menu structure.
    invalidateCandidates();
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

    menu->installEventFilter(this);
    m_trackedMenus.append(menu);

    QAction *menuAction = menu->menuAction();
    bool addedAncestor = false;
    bool childHasNamedAncestor = hasNamedAncestor;
    if (menuAction) {
        ancestors.append(menuAction);
        addedAncestor = true;
        if (!getActionText(menuAction).isEmpty()) {
            childHasNamedAncestor = true;
        }
        if (!m_trackedActionSet.contains(menuAction)) {
            m_trackedActions.append(menuAction);
            m_trackedActionSet.insert(menuAction);
            connect(menuAction, &QAction::changed, this, &AppMenuSearch::onActionChanged);
            connect(menuAction, &QAction::visibleChanged, this, &AppMenuSearch::onActionVisibilityChanged);
            connect(menuAction, &QObject::destroyed, this, &AppMenuSearch::onTrackedActionDestroyed);
        }
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
        if (!action || action->isSeparator()) {
            continue;
        }
        if (!m_trackedActionSet.contains(action)) {
            m_trackedActions.append(action);
            m_trackedActionSet.insert(action);
            connect(action, &QAction::changed, this, &AppMenuSearch::onActionChanged);
            connect(action, &QAction::visibleChanged, this, &AppMenuSearch::onActionVisibilityChanged);
            connect(action, &QObject::destroyed, this, &AppMenuSearch::onTrackedActionDestroyed);
        }
        if (!action->isVisible()) {
            continue;
        }
        if (m_searchCandidates.size() >= MAX_SEARCH_CANDIDATES) {
            if (!m_candidateTruncationLogged) {
                qWarning() << "AppMenuSearch: Maximum search candidates limit reached (" << MAX_SEARCH_CANDIDATES << "), remaining candidates will be discarded";
                m_candidateTruncationLogged = true;
            }
            break;
        }
        if (action->menu()) {
            collectSearchCandidates(action->menu(), visited, ancestors, childHasNamedAncestor);
        } else {
            ensurePathsComputed();
            const QString itemText = getActionText(action);
            const QStringList itemTokens = tokenizeText(itemText);

            QString fullPath = parentFullPath.isEmpty() ? itemText : parentFullPath % QStringLiteral(" » ") % itemText;
            QString evalPath = parentEvalPath.isEmpty() ? itemText : parentEvalPath % QStringLiteral(" » ") % itemText;
            QStringList fullTokens = parentFullTokens + itemTokens;
            QStringList evalTokens = parentEvalTokens + itemTokens;

            m_searchCandidates.append({action, ancestors, childHasNamedAncestor, std::move(fullPath), std::move(evalPath), itemTokens, std::move(fullTokens), std::move(evalTokens)});
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

        const QString ancestorText = getActionText(ancestor);
        if (ancestorText.isEmpty()) {
            continue;
        }

        auto it = context.matchCache.find(ancestor);
        bool matched = false;

        if (it != context.matchCache.end()) {
            matched = it.value();
        } else {
            matched = (matcher.indexIn(ancestorText) != -1);
            context.matchCache.insert(ancestor, matched);
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


QList<AppMenuSearch::SearchResult> AppMenuSearch::matchSearchCandidates(const QStringMatcher &matcher, const FilterOptions &options, const QString &query, const QElapsedTimer &searchTimer) const
{
    QList<SearchResult> results;
    QHash<QAction *, bool> matchCache;
    QHash<QAction *, bool> pathMatchCache;
    MatchContext context{matchCache, pathMatchCache};

    const bool ignoreTopLevel = options.ignoreTopLevel;
    const bool ignoreSubMenus = options.ignoreSubMenus;
    const bool showDisabledActions = options.showDisabledActions;
    const bool fuzzyMatching = options.fuzzyMatching;
    const QStringList queryTokens = fuzzyMatching ? tokenizeText(query) : QStringList();

    for (const SearchCandidate &candidate : std::as_const(m_searchCandidates)) {
        if (fuzzyMatching && searchExpired(searchTimer)) break;
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
                candidateScore = calculateFuzzyScore(query, queryTokens, itemText, candidate.itemTokens, searchTimer);
                match = (candidateScore > 0);
            } else {
                const int itemScore = calculateFuzzyScore(query, queryTokens, itemText, candidate.itemTokens, searchTimer);
                const int itemScoreWithBonus = (itemScore > 0) ? (itemScore + 500) : 0;

                const QString &evalPath = ignoreTopLevel ? candidate.evalPath : candidate.fullPath;
                const QStringList &evalTokens = ignoreTopLevel ? candidate.evalTokens : candidate.fullTokens;
                const int pathScore = calculateFuzzyScore(query, queryTokens, evalPath, evalTokens, searchTimer);

                candidateScore = std::max(itemScoreWithBonus, pathScore);
                match = (candidateScore > 0);
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

        if (results.isEmpty()) {
            results.reserve(MAX_SEARCH_RESULTS);
        }

        ActionInfo info;
        info.isEffectivelyEnabled = isEffectivelyEnabled;
        info.isChecked = action->isChecked();
        info.isCheckable = action->isCheckable();
        info.path = candidate.fullPath;

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
