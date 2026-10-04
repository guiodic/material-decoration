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
    /**
     * @brief Constructs an AppMenuSearch instance.
     * @param model Pointer to the AppMenuModel to search within
     * @param parent Optional parent QObject
     */
    explicit AppMenuSearch(AppMenuModel *model, QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~AppMenuSearch() override;

    static constexpr int MINIMUM_SEARCH_LENGTH = 3;
    static constexpr const char PROPERTY_SEARCH_PROXY[] = "isAppMenuSearchProxy";

    /**
     * @brief Checks if a search query is too short to be processed.
     * @param text The query text to check
     * @return True if the simplified text length is less than MINIMUM_SEARCH_LENGTH, false otherwise
     */
    static bool isQueryTooShort(const QString &text);

    /**
     * @brief Detailed state metadata for a matched action result.
     */
    struct ActionInfo {
        QString path;                 ///< Full displayable menu path (e.g., "File » Open » Recent")
        QString label;                ///< Cleaned label text of the action
        bool isEffectivelyEnabled = false; ///< True if action and all its ancestors are enabled
        bool isChecked = false;       ///< Checked state of the action
        bool isCheckable = false;     ///< Checkable state of the action
    };

    /**
     * @brief Represents an indexed searchable candidate collected from the menu hierarchy.
     */
    struct SearchCandidate {
        QPointer<QAction> action;     ///< Target menu action
        /**
         * @brief Ancestor menu actions from root to direct parent.
         *
         * Includes all parent menu actions, including untitled submenus.
         * Invariant: Even untitled submenus gate descendant reachability, so enabled state propagates down.
         */
        QList<QPointer<QAction>> ancestors;
        bool hasNamedAncestor = false; ///< True if at least one ancestor has a non-empty label
        QString parentFullPath;       ///< Cached full parent path string constructed during indexing
        QString parentEvalPath;       ///< Cached evaluation path string (excluding top-level if needed)
        QStringList itemTokens;       ///< Pre-tokenized word tokens for candidate label
        QStringList parentFullTokens; ///< Pre-tokenized word tokens for full parent path
        QStringList parentEvalTokens; ///< Pre-tokenized word tokens for evaluation path
    };

    /**
     * @brief Represents a matched search result entry with scoring details.
     */
    struct SearchResult {
        QPointer<QAction> action;     ///< Pointer to original target QAction
        ActionInfo info;              ///< Action metadata snapshot
        qint64 iconCacheKey = 0;      ///< Cache key for action icon
        int score = 0;                 ///< Match relevance score (higher is better)

        bool operator==(const SearchResult &other) const {
            return action == other.action
            && iconCacheKey == other.iconCacheKey
            && info.isEffectivelyEnabled == other.info.isEffectivelyEnabled
            && info.path == other.info.path
            && info.isChecked == other.info.isChecked
            && info.isCheckable == other.info.isCheckable
            && score == other.score;
        }
    };

    /**
     * @brief Options controlling candidate filtering and search matching logic.
     */
    struct FilterOptions {
        bool ignoreTopLevel = false;      ///< Skip matching top-level menu categories
        bool ignoreSubMenus = false;      ///< Restrict search strictly to leaf action labels
        bool showDisabledActions = false; ///< Include disabled actions in search results
        bool fuzzyMatching = false;       ///< Enable fuzzy word/token matching algorithm

        bool operator==(const FilterOptions &other) const = default;
    };

    /**
     * @brief Sets the menu where search results will be rendered.
     * @param searchMenu Pointer to the QMenu that will display search result proxy actions
     */
    void setSearchMenu(QMenu *searchMenu);

    /**
     * @brief Filters menu actions based on the search query and renders matching results.
     *
     * This method rebuilds search candidates if needed, searches for matches using either
     * exact substring or fuzzy matching, creates proxy actions for results, and populates
     * the search menu. Uses a synchronous text cache that is cleared when the method exits.
     *
     * @param text The search query text
     * @param options Filter options controlling search behavior (top-level filtering, submenu inclusion, disabled actions, fuzzy matching)
     */
    void filter(const QString &text, const FilterOptions &options);

    /**
     * @brief Clears all search result proxy actions from the search menu.
     *
     * Removes and schedules deletion of all actions marked with PROPERTY_SEARCH_PROXY,
     * detaches them from their action groups, and cleans up the proxy action groups.
     */
    void clear();

    /**
     * @brief Clears both the rendered result actions and the cached search state.
     *
     * Combines clear() with resetSearchState() to completely reset the search,
     * including query, results, and options. Used when the search UI is dismissed.
     */
    void reset();

    /**
     * @brief Marks the search candidates cache as dirty and clears cached data.
     *
     * Forces a rebuild of search candidates on the next filter() call. Clears
     * the candidates list, action text cache, last results, and filter options,
     * but intentionally preserves m_lastSearchQuery so hasValidQuery() can still
     * report an in-progress query.
     */
    void invalidateCandidates();

    /**
     * @brief Checks if there is a valid search query currently active.
     * @return True if the last search query is non-empty and meets minimum length requirements
     */
    bool hasValidQuery() const;

signals:
    void candidatesInvalidated();
    void repositionRequested();

private:
    /**
     * @brief Rebuilds the search candidates cache if marked dirty.
     *
     * Clears and repopulates m_searchCandidates by recursively collecting actions
     * from the application menu if m_searchCandidatesDirty is true.
     */
    void rebuildSearchCandidatesIfNeeded();

    /**
     * @brief Recursively collects searchable action candidates from a menu hierarchy.
     *
     * Traverses the menu tree depth-first, tracking ancestors and visited menus to avoid
     * cycles. Skips separators and invisible actions. Stops if the candidate limit or
     * maximum depth is reached.
     *
     * @param menu The menu to collect candidates from
     * @param visited Set of already-visited menus to prevent infinite recursion
     * @param ancestors List of ancestor menu actions from root to current menu
     * @param hasNamedAncestor True if any ancestor in the chain has a non-empty label
     */
    void collectSearchCandidates(QMenu *menu, QSet<QMenu *> &visited, QList<QPointer<QAction>> &ancestors, bool hasNamedAncestor = false);
    
    struct MatchState {
        QString text;
        bool matched = false;
    };

    struct MatchContext {
        QHash<QAction *, MatchState> &matchCache;
        QHash<QAction *, bool> &pathMatchCache;
    };

    /**
     * @brief Checks if a candidate matches the search query in its ancestors or item text.
     *
     * Uses a two-tier caching strategy: first checks if the parent path match is cached (O(1)),
     * then falls back to sequential evaluation with per-ancestor caching. Respects ignoreTopLevel
     * by skipping the first named ancestor from evaluation.
     *
     * @param candidate The search candidate to evaluate
     * @param itemText The cleaned text of the candidate action
     * @param matcher String matcher for substring search
     * @param ignoreTopLevel If true, skip matching the first named ancestor
     * @param context Match context containing caches for ancestor text and path matches
     * @return True if the candidate or any evaluated ancestor matches the query
     */
    bool matchesAncestorsOrText(const SearchCandidate &candidate, const QString &itemText, const QStringMatcher &matcher, bool ignoreTopLevel, MatchContext &context) const;

    /**
     * @brief Builds the full hierarchical path for a candidate action.
     *
     * Constructs a displayable path string with all named ancestors separated by » symbols.
     *
     * @param candidate The search candidate
     * @param itemText The text of the candidate action
     * @return Full path string like "File » Open » Recent"
     */
    QString buildFullPath(const SearchCandidate &candidate, const QString &itemText) const;

    /**
     * @brief Builds the evaluation path for a candidate, optionally skipping the top-level ancestor.
     *
     * Similar to buildFullPath but can exclude the first named ancestor when ignoreTopLevel is true.
     *
     * @param candidate The search candidate
     * @param itemText The text of the candidate action
     * @param ignoreTopLevel If true, skip the first named ancestor in the path
     * @return Evaluation path string for matching purposes
     */
    QString buildEvalPath(const SearchCandidate &candidate, const QString &itemText, bool ignoreTopLevel) const;

    /**
     * @brief Searches all candidates and returns matching results.
     *
     * Iterates through search candidates, checks if each matches based on filter options
     * (substring or fuzzy matching, top-level/submenu filtering, disabled action handling),
     * and builds a list of SearchResult objects. For fuzzy matching, results are sorted by
     * score and limited to MAX_SEARCH_RESULTS.
     *
     * @param matcher String matcher for substring search (used in non-fuzzy mode)
     * @param options Filter options controlling search behavior
     * @param query The search query string (used in fuzzy mode)
     * @return List of matching search results, sorted by score if fuzzy matching is enabled
     */
    QList<SearchResult> matchSearchCandidates(const QStringMatcher &matcher, const FilterOptions &options, const QString &query) const;

    /**
     * @brief Gets the cleaned display text for an action with caching.
     *
     * Retrieves the action text with accelerator markers removed and leading/trailing
     * whitespace trimmed. Results are cached in m_actionTextCache for performance.
     *
     * @param action The action to get text from
     * @return Cleaned action text, or empty string if action is null
     */
    QString getActionText(QAction *action) const;

    /**
     * @brief Resets the cached search state (query, results, options, processed menu).
     *
     * Internal helper that clears m_lastSearchQuery, m_lastResults, m_lastProcessedMenu,
     * and m_lastOptions without touching the rendered menu or candidates cache.
     */
    void resetSearchState();

private Q_SLOTS:
    /**
     * @brief Slot invoked when a tracked QAction emits changed().
     *
     * Invalidates candidates only if the action's cleaned text label actually changed.
     * State changes (enabled, checked, icon) do not invalidate search candidates.
     */
    void onActionChanged();

    /**
     * @brief Slot invoked when a tracked QAction emits visibleChanged().
     *
     * Invalidates candidates when an action becomes visible or hidden.
     */
    void onActionVisibilityChanged();

private:
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
    
    // This cache maps QAction* pointers directly to their cleansed text labels (accelerator markers removed).
    // It is automatically cleared on exit of each filter() pass using a qScopeGuard.
    mutable QHash<QAction *, QString> m_actionTextCache;

    // Actions connected to onActionChanged() and their known cleaned text labels at candidate build time
    QList<QPointer<QAction>> m_trackedActions;
    QHash<QAction *, QString> m_knownActionTexts;
};

} // namespace Material
