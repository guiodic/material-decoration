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
#include <utility>

static constexpr int MAX_SEARCH_RESULTS = 100;
static constexpr int MAX_SEARCH_CANDIDATES = 5000;

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
    return text.length() < MINIMUM_SEARCH_LENGTH;
}

void AppMenuSearch::filter(QMenu *searchMenu, const QString &text, bool ignoreTopLevel, bool ignoreSubMenus, bool showDisabledActions)
{
    if (!searchMenu) {
        return;
    }
    m_lastSearchQuery = text;

    // Clear results if search text is too short
    if (isQueryTooShort(text)) {
        clear(searchMenu);
        m_lastResults.clear();
        Q_EMIT repositionRequested();
        return;
    }

    if (!m_appMenuModel) {
        return;
    }

    {
        // Find results
        rebuildSearchCandidatesIfNeeded();
        QStringMatcher matcher(text, Qt::CaseInsensitive);
        QList<SearchResult> results = matchSearchCandidates(matcher, ignoreTopLevel, ignoreSubMenus);

        // If results are the same as last time, do nothing to prevent the freeze.
        if (m_lastResults == results) {
            return;
        }

        m_lastResults = std::move(results);
    } // 'results' goes out of scope here to prevent accidental use-after-move

    searchMenu->setUpdatesEnabled(false);

    // Clear previous results
    clear(searchMenu);

    int resultCount = 0;
    // Map each *original* action group to the QActionGroup we create for its
    // search-result proxies, so results that were mutually exclusive in the
    // real menu (e.g. radio-button items) stay mutually exclusive here too.
    QHash<QActionGroup *, QActionGroup *> groupMap;
    for (const SearchResult &result : std::as_const(m_lastResults)) {
        if (resultCount >= MAX_SEARCH_RESULTS) { // stop after 100 results
            break;
        }

        const ActionInfo &info = result.info;
        QAction *action = result.action;
        if (!info.isEffectivelyEnabled && !showDisabledActions) {
            continue;
        }
        QAction *newAction = new QAction(action->icon(), info.path, searchMenu);
        newAction->setEnabled(info.isEffectivelyEnabled);
        newAction->setCheckable(action->isCheckable());
        newAction->setChecked(action->isChecked());

        if (QActionGroup *originalGroup = action->actionGroup(); originalGroup && originalGroup->isExclusive()) {
            QActionGroup *&proxyGroup = groupMap[originalGroup];
            if (!proxyGroup) {
                proxyGroup = new QActionGroup(searchMenu);
                proxyGroup->setExclusionPolicy(originalGroup->exclusionPolicy());
                m_searchResultGroups.append(proxyGroup);
            }
            proxyGroup->addAction(newAction);
        }
      
        QPointer<QAction> safeAction = action;
        connect(newAction, &QAction::triggered, this, [safeAction, searchMenu]() {
            if (safeAction) {
                safeAction->trigger();
            }
            if (searchMenu) {
                searchMenu->hide();
            }
        });
        searchMenu->addAction(newAction);
        resultCount++;
    }

    Q_EMIT repositionRequested();
    searchMenu->setUpdatesEnabled(true);
}

void AppMenuSearch::clear(QMenu *searchMenu)
{
    if (!searchMenu) {
        return;
    }

    const auto actions = searchMenu->actions();
    for (int i = actions.count() - 1; i >= 2; --i) {
        QAction *action = actions.at(i);
        searchMenu->removeAction(action);
        // Detach action from its group before scheduling deletion
        if (QActionGroup *group = action->actionGroup()) {
            group->removeAction(action);
        }
        action->deleteLater();
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
    m_searchCandidates.clear();
    m_actionTextCache.clear();
}

void AppMenuSearch::clearLastResults()
{
    m_lastResults.clear();
    m_lastSearchQuery.clear();
}

QString AppMenuSearch::lastSearchQuery() const
{
    return m_lastSearchQuery;
}

void AppMenuSearch::rebuildSearchCandidatesIfNeeded()
{
    if (!m_searchCandidatesDirty) {
        return;
    }
    m_searchCandidatesDirty = false;
    m_searchCandidates.clear();

    if (!m_appMenuModel) {
        return;
    }
    QMenu *rootMenu = m_appMenuModel->menu();
    if (!rootMenu) {
        return;
    }

    QSet<QMenu *> visited;
    QList<QPointer<QAction>> namedAncestors;
    QList<QPointer<QAction>> enablementAncestors;
    collectSearchCandidates(rootMenu, visited, namedAncestors, enablementAncestors);
}

void AppMenuSearch::collectSearchCandidates(QMenu *menu, QSet<QMenu *> &visited, QList<QPointer<QAction>> &namedAncestors, QList<QPointer<QAction>> &enablementAncestors)
{
    if (!menu || visited.contains(menu) || m_searchCandidates.size() >= MAX_SEARCH_CANDIDATES) {
        return;
    }
    visited.insert(menu);

    QAction *menuAction = menu->menuAction();
    bool addedNamed = false;
    bool addedEnablement = false;
    if (menuAction) {
        // Unconditional: even an untitled submenu still gates whether its
        // children are reachable/enabled, so its own enabled state must
        // propagate down regardless of whether it has display text.
        enablementAncestors.append(menuAction);
        addedEnablement = true;
        if (!getActionText(menuAction).isEmpty()) {
            namedAncestors.append(menuAction);
            addedNamed = true;
        }
    }

    for (QAction *action : menu->actions()) {
        if (m_searchCandidates.size() >= MAX_SEARCH_CANDIDATES) {
            break;
        }
        if (action->isSeparator()) {
            continue;
        }
        if (action->menu()) {
            collectSearchCandidates(action->menu(), visited, namedAncestors, enablementAncestors);
        } else {
            m_searchCandidates.append({action, namedAncestors, enablementAncestors});
        }
    }

    if (addedNamed) {
        namedAncestors.removeLast();
    }
    if (addedEnablement) {
        enablementAncestors.removeLast();
    }
}

QList<AppMenuSearch::SearchResult> AppMenuSearch::matchSearchCandidates(const QStringMatcher &matcher, bool ignoreTopLevel, bool ignoreSubMenus) const
{
    QList<SearchResult> results;

    // Two independent memoized chains, mirroring how the original recursive
    // algorithm kept these concerns separate: a submenu's *enabled* state
    // always propagates to its children, even if the submenu has no title
    // of its own; its *title* only contributes to the visible path/matching
    // when non-empty. Folding both into a single "named ancestors" chain
    // would silently drop the disabled state of untitled submenus.
    QHash<QAction *, bool> enabledCache; // cumulative isEnabled up to and including this ancestor
    struct MatchState {
        bool currentMatched;
        QString text;
    };
    QHash<QAction *, MatchState> matchCache; // cumulative match state + this ancestor's text

    for (const SearchCandidate &candidate : std::as_const(m_searchCandidates)) {
        if (results.size() >= MAX_SEARCH_RESULTS) {
            break;
        }
        QAction *action = candidate.action;
        if (!action) {
            continue; // Action was destroyed since the cache was built.
        }

        bool ancestorsStillValid = true;

        // --- Enabled chain (every ancestor, named or not) ---
        bool isCurrentEnabled = true;
        if (!candidate.enablementAncestors.isEmpty()) {
            QAction *deepest = candidate.enablementAncestors.last();
            if (!deepest) {
                ancestorsStillValid = false;
            } else {
                auto it = enabledCache.find(deepest);
                if (it != enabledCache.end()) {
                    isCurrentEnabled = it.value();
                } else {
                    for (QAction *ancestor : std::as_const(candidate.enablementAncestors)) {
                        if (!ancestor) {
                            ancestorsStillValid = false;
                            break;
                        }
                        auto it2 = enabledCache.find(ancestor);
                        if (it2 != enabledCache.end()) {
                            isCurrentEnabled = it2.value();
                        } else {
                            if (!ancestor->isEnabled()) {
                                isCurrentEnabled = false;
                            }
                            enabledCache.insert(ancestor, isCurrentEnabled);
                        }
                    }
                }
            }
        }

        // --- Match/text chain (only ancestors with a non-empty title) ---
        bool currentMatched = false;
        if (ancestorsStillValid && !candidate.namedAncestors.isEmpty()) {
            QAction *parent = candidate.namedAncestors.last();
            if (!parent) {
                ancestorsStillValid = false;
            } else {
                auto it = matchCache.find(parent);
                if (it != matchCache.end()) {
                    currentMatched = it.value().currentMatched;
                } else {
                    for (int i = 0; i < candidate.namedAncestors.size(); ++i) {
                        QAction *ancestor = candidate.namedAncestors.at(i);
                        if (!ancestor) {
                            ancestorsStillValid = false;
                            break;
                        }
                        auto it2 = matchCache.find(ancestor);
                        if (it2 != matchCache.end()) {
                            currentMatched = it2.value().currentMatched;
                        } else {
                            QString ancestorText = getActionText(ancestor);
                            if (!currentMatched && (!ignoreTopLevel || i > 0)) {
                                if (matcher.indexIn(ancestorText) != -1) {
                                    currentMatched = true;
                                }
                            }
                            matchCache.insert(ancestor, {currentMatched, ancestorText});
                        }
                    }
                }
            }
        }

        if (!ancestorsStillValid) {
            continue; // A submenu in the path was rebuilt/destroyed; skip until next full rebuild.
        }

        const QString itemText = getActionText(action);
        bool match = currentMatched;
        if (ignoreSubMenus) {
            match = matcher.indexIn(itemText) != -1;
        } else if (!match && (!ignoreTopLevel || !candidate.namedAncestors.isEmpty())) {
            if (matcher.indexIn(itemText) != -1) {
                match = true;
            }
        }

        if (!match) {
            continue; // Skip building path entirely for non-matching entries!
        }

        // 2. Only perform path allocation and joins for matching results
        QStringList currentPath;
        currentPath.reserve(candidate.namedAncestors.size() + 1);
        for (int i = 0; i < candidate.namedAncestors.size(); ++i) {
            QAction *ancestor = candidate.namedAncestors.at(i);
            currentPath.append(matchCache.value(ancestor).text);
        }

        ActionInfo info;
        info.label = itemText;
        info.isEffectivelyEnabled = isCurrentEnabled && action->isEnabled();

        currentPath.append(itemText);
        info.path = currentPath.join(QStringLiteral(" » "));
        info.searchablePath = (currentPath.size() > 1) ? currentPath.mid(1).join(QStringLiteral(" » ")) : itemText;

        results.append({action, info});
    }

    return results;
}

QString AppMenuSearch::getActionText(QAction *action) const
{
    if (!action) {
        return QString();
    }
    const QString rawText = action->text();
    auto it = m_actionTextCache.find(rawText);
    if (it != m_actionTextCache.end()) {
        return it.value();
    }
    const QString cleanedText = KLocalizedString::removeAcceleratorMarker(rawText.trimmed());
    m_actionTextCache.insert(rawText, cleanedText);
    return cleanedText;
}

} // namespace Material
