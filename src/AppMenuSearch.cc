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
        QList<SearchResult> results = matchSearchCandidates(matcher, options.ignoreTopLevel, options.ignoreSubMenus, options.showDisabledActions);

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

void AppMenuSearch::collectSearchCandidates(QMenu *menu, QSet<QMenu *> &visited, QList<QPointer<QAction>> &ancestors)
{
    if (!menu || visited.contains(menu) || m_searchCandidates.size() >= MAX_SEARCH_CANDIDATES || ancestors.size() >= MAX_MENU_DEPTH) {
        return;
    }
    visited.insert(menu);

    QAction *menuAction = menu->menuAction();
    bool addedAncestor = false;
    if (menuAction) {
        ancestors.append(menuAction);
        addedAncestor = true;
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
            collectSearchCandidates(action->menu(), visited, ancestors);
        } else {
            m_searchCandidates.append({action, ancestors});
        }
    }

    if (addedAncestor) {
        ancestors.removeLast();
    }
}

bool AppMenuSearch::matchesAncestorsOrText(const SearchCandidate &candidate, const QStringMatcher &matcher, bool ignoreTopLevel, QHash<QAction *, MatchState> &matchCache) const
{
    const QString itemText = getActionText(candidate.action);
    bool isTopLevel = true;
    bool hasNamedAncestors = false;
    for (QAction *ancestor : candidate.ancestors) {
        if (!ancestor) {
            continue;
        }

        auto it = matchCache.find(ancestor);
        QString ancestorText;
        bool matched = false;
        if (it != matchCache.end()) {
            ancestorText = it.value().text;
            matched = it.value().matched;
        } else {
            ancestorText = getActionText(ancestor);
            matched = (matcher.indexIn(ancestorText) != -1);
            matchCache.insert(ancestor, {ancestorText, matched});
        }

        if (ancestorText.isEmpty()) {
            continue;
        }
        hasNamedAncestors = true;
        if (ignoreTopLevel && isTopLevel) {
            isTopLevel = false;
            continue;
        }
        isTopLevel = false;

        if (matched) {
            return true;
        }
    }

    if (!ignoreTopLevel || hasNamedAncestors) {
        if (matcher.indexIn(itemText) != -1) {
            return true;
        }
    }

    return false;
}

QList<AppMenuSearch::SearchResult> AppMenuSearch::matchSearchCandidates(const QStringMatcher &matcher, bool ignoreTopLevel, bool ignoreSubMenus, bool showDisabledActions) const
{
    QList<SearchResult> results;
    QHash<QAction *, MatchState> matchCache;

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

        if (ignoreSubMenus) {
            bool hasNamedAncestors = false;
            for (QAction *ancestor : candidate.ancestors) {
                if (ancestor && !getActionText(ancestor).isEmpty()) {
                    hasNamedAncestors = true;
                    break;
                }
            }
            if (ignoreTopLevel && !hasNamedAncestors) {
                match = false;
            } else {
                match = (matcher.indexIn(itemText) != -1);
            }
        } else {
            match = matchesAncestorsOrText(candidate, matcher, ignoreTopLevel, matchCache);
        }

        if (!match) {
            continue;
        }

        QStringList currentPath;
        currentPath.reserve(candidate.ancestors.size() + 1);
        for (QAction *ancestor : candidate.ancestors) {
            if (ancestor) {
                QString text;
                auto it = matchCache.find(ancestor);
                if (it != matchCache.end()) {
                    text = it.value().text;
                } else {
                    text = getActionText(ancestor);
                    matchCache.insert(ancestor, {text, matcher.indexIn(text) != -1});
                }
                if (!text.isEmpty()) {
                    currentPath.append(text);
                }
            }
        }

        ActionInfo info;
        info.label = itemText;
        info.isEffectivelyEnabled = isEffectivelyEnabled;
        info.isChecked = action->isChecked();
        info.isCheckable = action->isCheckable();

        currentPath.append(itemText);
        info.path = currentPath.join(QStringLiteral(" » "));

        results.append({action, info, action->icon().cacheKey()});
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
