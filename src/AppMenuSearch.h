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
    static bool isQueryTooShort(const QString &text);

    struct ActionInfo {
        QString path;
        QString label;
        bool isEffectivelyEnabled = false;
    };

    struct SearchCandidate {
        QPointer<QAction> action;
        QList<QPointer<QAction>> namedAncestors;
        QList<QPointer<QAction>> enablementAncestors;
    };

    struct SearchResult {
        QPointer<QAction> action;
        ActionInfo info;

        bool operator==(const SearchResult &other) const {
            return action == other.action
            && info.isEffectivelyEnabled == other.info.isEffectivelyEnabled
            && info.path == other.info.path
            && (!action || (action->isChecked() == other.action->isChecked()
            && action->isCheckable() == other.action->isCheckable()));
        }
    };

    struct FilterOptions {
        bool ignoreTopLevel = false;
        bool ignoreSubMenus = false;
        bool showDisabledActions = false;
    };

    void filter(QMenu *searchMenu, const QString &text, const FilterOptions &options);
    void clear(QMenu *searchMenu);
    
    void invalidateCandidates();
    void clearLastResults();
    
    QString lastSearchQuery() const;

signals:
    void repositionRequested();

private:
    void rebuildSearchCandidatesIfNeeded();
    void collectSearchCandidates(QMenu *menu, QSet<QMenu *> &visited, QList<QPointer<QAction>> &namedAncestors, QList<QPointer<QAction>> &enablementAncestors);
    QList<SearchResult> matchSearchCandidates(const QStringMatcher &matcher, bool ignoreTopLevel, bool ignoreSubMenus, bool showDisabledActions) const;
    QString getActionText(QAction *action) const;

    QPointer<AppMenuModel> m_appMenuModel;
    QString m_lastSearchQuery;
    bool m_lastShowDisabledActions = false;
    bool m_lastIgnoreTopLevel = false;
    bool m_lastIgnoreSubMenus = false;
    QList<SearchResult> m_lastResults;
    QList<SearchCandidate> m_searchCandidates;
    bool m_searchCandidatesDirty = true;
    QList<QPointer<QActionGroup>> m_searchResultGroups;
    mutable QHash<QString, QString> m_actionTextCache;
};

} // namespace Material
