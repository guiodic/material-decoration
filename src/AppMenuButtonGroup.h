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

// own
#include "AppMenuModel.h"
#include "AppMenuButton.h"

// KDecoration
#include <KDecoration3/DecorationButton>
#include <KDecoration3/DecorationButtonGroup>

// Qt
#include <QMenu>
#include <QLineEdit>
#include <QPointer>
#include <QVector>

class QTimer;
class QVariantAnimation;

namespace Material
{

class Decoration;
class TextButton;
class MenuOverflowButton;
class SearchButton;
class AppMenuSearch;

/**
 * @brief Button group managing top-level application menu buttons, overflow, search, and hamburger menu modes.
 *
 * AppMenuButtonGroup populates top-level menu category buttons (File, Edit, View, etc.), dynamically calculates
 * available space to overflow hidden buttons into an overflow menu, provides integrated menu search UI, and handles
 * hover animations and keyboard menu navigation.
 */
class AppMenuButtonGroup : public KDecoration3::DecorationButtonGroup
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an AppMenuButtonGroup instance for the parent decoration.
     * @param decoration Pointer to parent Decoration.
     */
    AppMenuButtonGroup(Decoration *decoration);

    /**
     * @brief Destructor.
     */
    ~AppMenuButtonGroup() override;

    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(int overflowing READ overflowing WRITE setOverflowing NOTIFY overflowingChanged)
    Q_PROPERTY(bool hovered READ hovered WRITE setHovered NOTIFY hoveredChanged)
    Q_PROPERTY(bool showing READ showing WRITE setShowing NOTIFY showingChanged)
    Q_PROPERTY(bool alwaysShow READ alwaysShow WRITE setAlwaysShow NOTIFY alwaysShowChanged)
    Q_PROPERTY(bool animationEnabled READ animationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)

    /**
     * @brief Checks if the button group area is currently hovered by the mouse.
     * @return True if hovered.
     */
    bool hovered() const;

    /**
     * @brief Sets the hovered state of the button group.
     * @param value True if hovered.
     */
    void setHovered(bool value);

    /**
     * @brief Checks if application menu buttons are set to always remain visible.
     * @return True if always visible.
     */
    bool alwaysShow() const;

    /**
     * @brief Sets whether application menu buttons should always remain visible.
     * @param value True to always show menu buttons.
     */
    void setAlwaysShow(bool value);

    /**
     * @brief Checks if hover/showing animations are enabled.
     * @return True if animations are enabled.
     */
    bool animationEnabled() const;

    /**
     * @brief Enables or disables hover/showing animations.
     * @param value True to enable animations.
     */
    void setAnimationEnabled(bool value);

    /**
     * @brief Returns hover animation duration in milliseconds.
     * @return Duration in ms.
     */
    int animationDuration() const;

    /**
     * @brief Sets hover animation duration in milliseconds.
     * @param duration Duration in ms.
     */
    void setAnimationDuration(int duration);

    /**
     * @brief Returns current opacity of the button group.
     * @return Opacity value between 0.0 and 1.0.
     */
    qreal opacity() const;

    /**
     * @brief Sets opacity of the button group.
     * @param value Opacity value between 0.0 and 1.0.
     */
    void setOpacity(qreal value);

    /**
     * @brief Calculates total visible width occupied by active text, overflow, and search buttons.
     * @return Total visible width in local units.
     */
    qreal visibleWidth() const;

    /**
     * @brief Checks if the DBus application menu model has completed its initial load.
     * @return True if menu model was loaded at least once.
     */
    bool menuLoadedOnce() const;

    /**
     * @brief Checks if a menu popup activation is pending DBus menu model completion.
     * @return True if waiting for menu load.
     */
    bool isWaitingForMenu() const;

    /**
     * @brief Processes mouse hover move events across menu buttons.
     * @param pos Hover position in local coordinates.
     */
    void handleHoverMove(const QPointF &pos);

public:
    /**
     * @brief Toggles hamburger menu mode versus full horizontal menu bar mode.
     * @param value True for hamburger menu mode.
     */
    void setHamburgerMenu(bool value);

    /**
     * @brief Re-queries the DBus application menu model and updates top-level menu buttons.
     */
    void updateAppMenuModel();

    /**
     * @brief Calculates which text buttons fit within availableRect and hides overflowing items.
     * @param availableRect Bounding rectangle available for menu buttons.
     */
    void updateOverflow(QRectF availableRect);

    /**
     * @brief Recalculates showing/hidden visibility state based on hover, configuration, and active menu status.
     */
    void updateShowing();

private:
    void onMenuReadyForSearch();
    void triggerOverflow();
    void onMenuAboutToHide();
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
    int currentIndex() const;
    void setCurrentIndex(int set);

    bool overflowing() const;
    void setOverflowing(bool set);

    bool showing() const;
    void setShowing(bool value);

    bool isMenuOpen() const;

    KDecoration3::DecorationButton* buttonAt(QPoint pos) const;

    void unPressAllButtons();

    void trigger(int index);

    void resetButtons();
    void setupSearchMenu();
    void repositionSearchMenu();
    AppMenuButton *getAppMenuButton(int index) const;
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
    qreal m_visibleWidth;
    QPointer<QMenu> m_currentMenu;
    int m_buttonIndexWaitingForPopup = -1;
    int m_buttonIndexOfMenuToCache = -1;

    QPointer<QMenu> m_searchMenu;
    QPointer<QMenu> m_overflowMenu;
    QPointer<QLineEdit> m_searchLineEdit;
    QTimer *m_searchDebounceTimer;
    QTimer *m_menuUpdateDebounceTimer;
    QTimer *m_delayedCacheTimer;
    QTimer *m_resetTimer;
    QTimer *m_menuLoadFallbackTimer;

    bool m_searchUiVisible = false;

    bool m_isMenuUpdateThrottled = false;
    bool m_pendingMenuUpdate = false;
    bool m_menuLoadedOnce = false;

    AppMenuSearch *m_search;

    QList<QPointer<TextButton>> m_textButtons;
    QPointer<MenuOverflowButton> m_overflowButton;
    QPointer<SearchButton> m_searchButton;

    QPointer<KDecoration3::DecorationButton> m_hoveredButton = nullptr;

    // Cached text button widths to avoid querying geometry().width() twice during overflow layout.
    // Invariant: m_cachedWidths.size() == m_textButtons.size() when in use.
    QVector<qreal> m_cachedWidths;

    friend class AppMenuButton;
    friend class Decoration;
};

} // namespace Material
