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

// KDecoration
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/DecorationButton>
#include <KDecoration3/DecorationButtonGroup>

// Qt
#include <QMenu>
#include <QTimer>
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

    int visibleWidth() const;

    bool isMenuOpen() const;
    bool menuLoadedOnce() const;

    KDecoration3::DecorationButton* buttonAt(int x, int y) const;

    void unPressAllButtons();
    void handleHoverMove(const QPointF &pos);

public slots:
    void setHamburgerMenu(bool value);
    void onMenuReadyForSearch();
    void updateAppMenuModel();
    void updateOverflow(QRectF availableRect);
    void trigger(int index);
    void triggerOverflow();
    void updateShowing();
    void onMenuAboutToHide();

private slots:
    void resetButtons();
    void onHitLeft();
    void onHitRight();
    void onHasApplicationMenuChanged(bool hasMenu);
    void onApplicationMenuChanged();
    void performDebouncedMenuUpdate();
    void onMenuUpdateThrottleTimeout();
    void onDelayedCacheTimerTimeout();
    void onShowingChanged(bool hovered);
    void filterMenu(const QString &text);
    void onSearchTimerTimeout();
    void onSubMenuReady(QMenu *menu);

signals:
    void menuUpdated();
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
    void repositionSearchMenu();
    void searchMenu(QMenu *menu, const QString &text, QList<QAction *> &results);
    ActionInfo getActionPath(QAction *action) const;
    int findNextVisibleButtonIndex(int currentIndex, bool forward) const;

    void popupMenu(QMenu *menu, int buttonIndex);
    void handleSearchTrigger();
    void handleOverflowTrigger();
    void handleMenuButtonTrigger(int buttonIndex);

    AppMenuModel *m_appMenuModel;
    int m_currentIndex;
    int m_overflowIndex;
    int m_searchIndex;
    bool m_overflowing;
    bool m_hamburgerMenu;
    bool m_hovered;
    bool m_showing;
    bool m_alwaysShow;
    bool m_animationEnabled;
    QVariantAnimation *m_animation;
    qreal m_opacity;
    int m_visibleWidth;
    QPointer<QMenu> m_currentMenu;
    int m_buttonIndexWaitingForPopup = -1;
    int m_buttonIndexOfMenuToCache = -1;

    QPointer<QMenu> m_searchMenu;
    QPointer<QLineEdit> m_searchLineEdit;
    QTimer *m_searchDebounceTimer;
    QTimer *m_menuUpdateDebounceTimer;

    QTimer *m_delayedCacheTimer;
    QPointer<QMenu> m_menuToCache;

    bool m_searchUiVisible = false;

    bool m_isMenuUpdateThrottled = false;
    bool m_pendingMenuUpdate = false;
    bool m_menuReadyForSearch = false;
    bool m_menuLoadedOnce = false;
    QString m_lastSearchQuery;
    QList<QAction *> m_lastResults;

    QPointer<KDecoration3::DecorationButton> m_hoveredButton = nullptr;
};

} // namespace Material
