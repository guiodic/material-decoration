/*
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

// own
#include "AppMenuModel.h"
#include "SearchButton.h"

// KDecoration
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/DecorationButton>
#include <KDecoration3/DecorationButtonGroup>

// Qt
#include <QMenu>
#include <QVariantAnimation>
#include <QLineEdit>

namespace Material
{

class Decoration;

class AppMenuButtonGroup : public KDecoration3::DecorationButtonGroup
{
    Q_OBJECT

public:
    AppMenuButtonGroup(Decoration *decoration);
    ~AppMenuButtonGroup() override;

    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(int overflowing READ overflowing WRITE setOverflowing NOTIFY overflowingChanged)
    Q_PROPERTY(bool hovered READ hovered WRITE setHovered NOTIFY hoveredChanged)
    Q_PROPERTY(bool showing READ showing WRITE setShowing NOTIFY showingChanged)
    Q_PROPERTY(bool alwaysShow READ alwaysShow WRITE setAlwaysShow NOTIFY alwaysShowChanged)
    Q_PROPERTY(bool animationEnabled READ animationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)


    int currentIndex() const;
    void setCurrentIndex(int set);

    bool overflowing() const;
    void setOverflowing(bool set);

    bool hovered() const;
    void setHovered(bool value);

    bool showing() const;
    void setShowing(bool value);

    bool alwaysShow() const;
    void setAlwaysShow(bool value);

    bool animationEnabled() const;
    void setAnimationEnabled(bool value);

    int animationDuration() const;
    void setAnimationDuration(int duration);

    qreal opacity() const;
    void setOpacity(qreal value);

    bool isMenuOpen() const;

    KDecoration3::DecorationButton* buttonAt(int x, int y) const;

    void unPressAllButtons();

public slots:
    void onMenuReadyForSearch();
    void initAppMenuModel();
    void updateAppMenuModel();
    void updateOverflow(QRectF availableRect);
    void trigger(int index);
    void triggerOverflow();
    void updateShowing();
    void onMenuAboutToHide();

private slots:
    void onShowingChanged(bool hovered);
    void filterMenu(const QString &text);
    void onSearchReturnPressed();
    void onSearchMenuHidden();

signals:
    void menuUpdated();
    void requestActivateIndex(int index);
    void requestActivateOverflow();

    void currentIndexChanged();
    void overflowingChanged();
    void hoveredChanged(bool);
    void showingChanged(bool);
    void alwaysShowChanged(bool);
    void animationEnabledChanged(bool);
    void animationDurationChanged(int);
    void opacityChanged(qreal);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct ActionInfo {
        QString path;
        bool isEffectivelyEnabled;
    };

    void setupSearchMenu();
    void resetButtons();
    void searchMenu(QMenu *menu, const QString &text, QList<QAction *> &results);
    ActionInfo getActionPath(QAction *action) const;
    void clampToScreen(QMenu* menu);

    AppMenuModel *m_appMenuModel;
    int m_currentIndex;
    int m_overflowIndex;
    int m_searchIndex;
    bool m_overflowing;
    bool m_hovered;
    bool m_showing;
    bool m_alwaysShow;
    bool m_animationEnabled;
    QVariantAnimation *m_animation;
    qreal m_opacity;
    QPointer<QMenu> m_currentMenu;

    SearchButton *m_searchButton;
    QPointer<QMenu> m_searchMenu;
    QPointer<QLineEdit> m_searchLineEdit;
    bool m_searchUiVisible = false;

    bool m_menuReadyForSearch = false;
    QString m_lastSearchQuery;
};

} // namespace Material
