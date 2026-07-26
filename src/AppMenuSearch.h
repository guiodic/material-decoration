/*
 * Copyright (C) 2025 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
 * Copyright (C) 2020 Chris Holland <zrenfire@gmail.com>
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

    struct ActionInfo {
        QString path;
        QString searchablePath;
        QString label;
        bool isEffectivelyEnabled;
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

    void filter(QMenu *searchMenu, const QString &text, bool ignoreTopLevel, bool ignoreSubMenus, bool showDisabledActions);
    void clear(QMenu *searchMenu);
    
    void invalidateCandidates();
    void clearLastResults();
    
    QString lastSearchQuery() const;

signals:
    void repositionRequested();

private:
    void rebuildSearchCandidatesIfNeeded();
    void collectSearchCandidates(QMenu *menu, QSet<QMenu *> &visited, QList<QPointer<QAction>> &namedAncestors, QList<QPointer<QAction>> &enablementAncestors);
    QList<SearchResult> matchSearchCandidates(const QStringMatcher &matcher, bool ignoreTopLevel, bool ignoreSubMenus) const;
    QString getActionText(QAction *action) const;

    AppMenuModel *m_appMenuModel;
    QString m_lastSearchQuery;
    QList<SearchResult> m_lastResults;
    QList<SearchCandidate> m_searchCandidates;
    bool m_searchCandidatesDirty = true;
    QList<QPointer<QActionGroup>> m_searchResultGroups;
    mutable QHash<QString, QString> m_actionTextCache;
};

} // namespace Material
