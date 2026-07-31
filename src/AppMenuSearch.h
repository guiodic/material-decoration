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

#pragma once

#include <QObject>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QPointer>
#include <QList>
#include <QSet>
#include <QHash>
#include <QStringList>
#include <QStringMatcher>

namespace Material
{

class AppMenuModel;

class AppMenuSearch : public QObject
{
    Q_OBJECT

public:
    explicit AppMenuSearch(AppMenuModel *model, QObject *parent = nullptr);
    ~AppMenuSearch() override;

    static constexpr int MINIMUM_SEARCH_LENGTH = 3;
    static constexpr const char PROPERTY_SEARCH_PROXY[] = "isAppMenuSearchProxy";
    static bool isQueryTooShort(const QString &text);

    struct ActionInfo {
        QString path;
        QString label;
        bool isEffectivelyEnabled = false;
        bool isChecked = false;
        bool isCheckable = false;
    };

    struct SearchCandidate {
        QPointer<QAction> action;
        // Contains all parent menu actions, including those without a title/label.
        // This is a strict invariant: even an untitled submenu gates whether its children
        // are reachable, so its enabled state must propagate down to all descendants.
        QList<QPointer<QAction>> ancestors;
        // True if at least one ancestor in the whole parent chain (not just the immediate parent)
        // has a non-empty title/label. Used to correctly identify top-level leaf actions.
        bool hasNamedAncestor = false;
    };

    struct SearchResult {
        QPointer<QAction> action;
        ActionInfo info;
        qint64 iconCacheKey = 0;

        bool operator==(const SearchResult &other) const {
            return action == other.action
            && iconCacheKey == other.iconCacheKey
            && info.isEffectivelyEnabled == other.info.isEffectivelyEnabled
            && info.path == other.info.path
            && info.isChecked == other.info.isChecked
            && info.isCheckable == other.info.isCheckable;
        }
    };

    struct FilterOptions {
        bool ignoreTopLevel = false;
        bool ignoreSubMenus = false;
        bool showDisabledActions = false;

        bool operator==(const FilterOptions &other) const = default;
    };

    void setSearchMenu(QMenu *searchMenu);
    void filter(const QString &text, const FilterOptions &options);
    void clear();

    // Clears both the rendered result actions and the cached search state
    // (query, results, options). Used whenever the search UI is dismissed.
    void reset();
    
    void invalidateCandidates();
    bool hasValidQuery() const;

signals:
    void repositionRequested();

private:
    void rebuildSearchCandidatesIfNeeded();
    void collectSearchCandidates(QMenu *menu, QSet<QMenu *> &visited, QList<QPointer<QAction>> &ancestors, bool hasNamedAncestor = false);
    
    struct MatchState {
        QString text;
        bool matched = false;
        bool pathMatched = false;
        bool pathMatchedValid = false;
    };
    bool matchesAncestorsOrText(const SearchCandidate &candidate, const QString &itemText, const QStringMatcher &matcher, bool ignoreTopLevel, QHash<QAction *, MatchState> &matchCache) const;
    
    QList<SearchResult> matchSearchCandidates(const QStringMatcher &matcher, bool ignoreTopLevel, bool ignoreSubMenus, bool showDisabledActions) const;
    QString getActionText(QAction *action) const;
    void resetSearchState();

    QPointer<AppMenuModel> m_appMenuModel;
    QPointer<QMenu> m_searchMenu;
    QString m_lastSearchQuery;
    FilterOptions m_lastOptions;
    QList<SearchResult> m_lastResults;
    QPointer<QMenu> m_lastProcessedMenu;
    QList<SearchCandidate> m_searchCandidates;
    bool m_searchCandidatesDirty = true;
    bool m_menuIsRendered = false;
    bool m_candidateTruncationLogged = false;
    QList<QPointer<QActionGroup>> m_searchResultGroups;
    mutable QHash<QString, QString> m_actionTextCache;
};

} // namespace Material
