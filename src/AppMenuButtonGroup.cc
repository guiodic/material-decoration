/*
 * Copyright (C) 2025 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
 * Copyright (C) 2020 Chris Holland <zrenfire@gmail.com>
 * Copyright (C) 2016 Kai Uwe Broulik <kde@privat.broulik.de>
 * Copyright (C) 2014 by Hugo Pereira Da Costa <hugo.pereira@free.fr>
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

// own
#include "AppMenuButtonGroup.h"
#include "Material.h"
#include "BuildConfig.h"
#include "AppMenuModel.h"
#include "Decoration.h"
#include "NavigableMenu.h"
#include "TextButton.h"
#include "MenuOverflowButton.h"
#include "SearchButton.h"

// KDecoration
#include <KDecoration3/DecoratedWindow>

// KF
#include <KWindowSystem>
#include <KLocalizedString>

// Qt
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QSet>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidgetAction>



namespace Material
{

AppMenuButtonGroup::AppMenuButtonGroup(Decoration *decoration)
    : KDecoration3::DecorationButtonGroup(decoration)
    , m_appMenuModel(new AppMenuModel(this))
    , m_currentIndex(-1)
    , m_overflowIndex(-1)
    , m_searchIndex(-1)
    , m_hamburgerMenu(false)
    , m_hovered(false)
    , m_showing(true)
    , m_alwaysShow(true)
    , m_animationEnabled(false)
    , m_animation(new QVariantAnimation(this))
    , m_opacity(1)
    , m_visibleWidth(0)
    , m_searchMenu(nullptr)
    , m_searchLineEdit(nullptr)
    , m_searchDebounceTimer(nullptr)
    , m_searchUiVisible(false)
{
    m_searchDebounceTimer = new QTimer(this);
    m_searchDebounceTimer->setInterval(150);
    m_searchDebounceTimer->setSingleShot(true);
    connect(m_searchDebounceTimer, &QTimer::timeout, this, &AppMenuButtonGroup::onSearchTimerTimeout);

    m_menuUpdateDebounceTimer = new QTimer(this);
    m_menuUpdateDebounceTimer->setInterval(100);
    m_menuUpdateDebounceTimer->setSingleShot(true);
    connect(m_menuUpdateDebounceTimer, &QTimer::timeout, this, &AppMenuButtonGroup::onMenuUpdateThrottleTimeout);

    m_delayedCacheTimer = new QTimer(this);
    m_delayedCacheTimer->setInterval(200);
    m_delayedCacheTimer->setSingleShot(true);
    connect(m_delayedCacheTimer, &QTimer::timeout, this, &AppMenuButtonGroup::onDelayedCacheTimerTimeout);
    // Assign showing and opacity before we bind the onShowingChanged animation
    // so that new windows do not animate.
    setAlwaysShow(decoration->menuAlwaysShow());
    updateShowing();
    setOpacity(m_showing ? 1 : 0);

    connect(this, &AppMenuButtonGroup::showingChanged,
            this, &AppMenuButtonGroup::onShowingChanged);
    connect(this, &AppMenuButtonGroup::hoveredChanged,
            this, &AppMenuButtonGroup::updateShowing);
    connect(this, &AppMenuButtonGroup::alwaysShowChanged,
            this, &AppMenuButtonGroup::updateShowing);
    connect(this, &AppMenuButtonGroup::currentIndexChanged,
            this, &AppMenuButtonGroup::updateShowing);

    m_animationEnabled = decoration->animationsEnabled();
    m_animation->setDuration(decoration->animationsDuration());
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setOpacity(value.toReal());
    });

    auto decoratedClient = decoration->window();
    connect(decoratedClient, &KDecoration3::DecoratedWindow::hasApplicationMenuChanged,
            this, &AppMenuButtonGroup::onHasApplicationMenuChanged);
    connect(decoratedClient, &KDecoration3::DecoratedWindow::applicationMenuChanged,
            this, &AppMenuButtonGroup::onApplicationMenuChanged);

    connect(this, &AppMenuButtonGroup::requestActivateOverflow,
            this, &AppMenuButtonGroup::triggerOverflow);


    connect(m_appMenuModel, &AppMenuModel::modelReset,
        this, &AppMenuButtonGroup::updateAppMenuModel);
    connect(m_appMenuModel, &AppMenuModel::menuReadyForSearch,
        this, &AppMenuButtonGroup::onMenuReadyForSearch);
    connect(m_appMenuModel, &AppMenuModel::subMenuReady,
        this, &AppMenuButtonGroup::onSubMenuReady);

    if (decoratedClient->hasApplicationMenu()) {
        onHasApplicationMenuChanged(true);
    }
}

AppMenuButtonGroup::~AppMenuButtonGroup()
{
    if (m_searchMenu) {
        delete m_searchMenu.data();
    }
}

void AppMenuButtonGroup::setupSearchMenu()
{
    m_searchMenu = new NavigableMenu(nullptr);
    m_searchLineEdit = new QLineEdit(m_searchMenu);
    m_searchLineEdit->setMinimumWidth(200);

    auto *searchAction = new QWidgetAction(m_searchMenu);
    searchAction->setDefaultWidget(m_searchLineEdit);
    m_searchMenu->addAction(searchAction);
    m_searchMenu->addSeparator();

    m_searchMenu->installEventFilter(this);

    connect(m_searchLineEdit, &QLineEdit::textChanged, m_searchDebounceTimer, qOverload<>(&QTimer::start));

    m_searchLineEdit->installEventFilter(this);
    m_searchLineEdit->setFocusPolicy(Qt::StrongFocus);
    m_searchLineEdit->setPlaceholderText(i18nd("plasma_applet_org.kde.plasma.appmenu","Search")+QStringLiteral("…"));
    m_searchLineEdit->setClearButtonEnabled(false);
}

void AppMenuButtonGroup::repositionSearchMenu()
{
    if (!m_searchMenu || !m_searchMenu->isVisible()) {
        return;
    }

    auto *deco = qobject_cast<Decoration *>(decoration());
    KDecoration3::DecorationButton *button = buttons().value(m_searchIndex);
    if (!deco || !button) {
        return;
    }

    if (KWindowSystem::isPlatformX11()) { // X11
        const QRectF buttonRect = button->geometry();
        QPoint rootPosition = buttonRect.topLeft().toPoint();
        rootPosition += deco->windowPos();
        // Re-popping up at the original position
        m_searchMenu->popup(rootPosition);
    } else { // Wayland
        KDecoration3::Positioner positioner;
        positioner.setAnchorRect(button->geometry());
        deco->popup(positioner, m_searchMenu);
        m_searchMenu->popup(m_searchMenu->pos()); //HACK without this the scrollbar remain even if not necessary
    }
}

int AppMenuButtonGroup::currentIndex() const
{
    return m_currentIndex;
}

void AppMenuButtonGroup::setCurrentIndex(int set)
{
    if (m_currentIndex != set) {
        m_currentIndex = set;
        emit currentIndexChanged();
    }
}

bool AppMenuButtonGroup::overflowing() const
{
    return m_overflowing;
}

void AppMenuButtonGroup::setOverflowing(bool set)
{
    if (m_overflowing != set) {
        m_overflowing = set;
        emit overflowingChanged();
    }
}

bool AppMenuButtonGroup::hovered() const
{
    return m_hovered;
}

void AppMenuButtonGroup::setHovered(bool value)
{
    if (m_hovered != value) {
        m_hovered = value;
        emit hoveredChanged(value);
    }
}

bool AppMenuButtonGroup::showing() const
{
    return m_showing;
}

void AppMenuButtonGroup::setShowing(bool value)
{
    if (m_showing != value) {
        m_showing = value;
        emit showingChanged(value);
    }
}

bool AppMenuButtonGroup::alwaysShow() const
{
    return m_alwaysShow;
}

void AppMenuButtonGroup::setAlwaysShow(bool value)
{
    if (m_alwaysShow != value) {
        m_alwaysShow = value;
        emit alwaysShowChanged(value);
    }
}

bool AppMenuButtonGroup::animationEnabled() const
{
    return m_animationEnabled;
}

void AppMenuButtonGroup::setAnimationEnabled(bool value)
{
    if (m_animationEnabled != value) {
        m_animationEnabled = value;
        emit animationEnabledChanged(value);
    }
}

int AppMenuButtonGroup::animationDuration() const
{
    return m_animation->duration();
}

void AppMenuButtonGroup::setAnimationDuration(int value)
{
    if (m_animation->duration() != value) {
        m_animation->setDuration(value);
        emit animationDurationChanged(value);
    }
}

qreal AppMenuButtonGroup::opacity() const
{
    return m_opacity;
}

void AppMenuButtonGroup::setOpacity(qreal value)
{
    if (m_opacity != value) {
        m_opacity = value;

        for (auto *decoButton : buttons()) {
            if (auto *button = qobject_cast<Button *>(decoButton)) {
                button->setOpacity(m_opacity);
            }
        }

        emit opacityChanged(value);
    }
}

KDecoration3::DecorationButton* AppMenuButtonGroup::buttonAt(QPoint pos) const
{
    for (auto *button : buttons()) {
        if (button->isVisible() && button->geometry().contains(pos)) {
            return button;
        }
    }
    return nullptr;
}

void AppMenuButtonGroup::resetButtons()
{
    if (buttons().isEmpty()) {
        return;
    }

    // This removes all buttons with the "Custom" type from the group's list,
    // but does not delete the button widgets themselves. The buttons are parented
    // to this widget, so their deletion is handled by the QObject ownership system.
    // Manually calling qDeleteAll would lead to a double-free.
    removeButton(KDecoration3::DecorationButtonType::Custom);

    emit menuUpdated();
}

void AppMenuButtonGroup::onMenuReadyForSearch()
{
    m_menuReadyForSearch = true;
    if (!m_lastSearchQuery.isEmpty() && m_searchUiVisible) {
        filterMenu(m_lastSearchQuery);
    }
}

void AppMenuButtonGroup::onHasApplicationMenuChanged(bool hasMenu)
{
    if (hasMenu) {
        if (m_isMenuUpdateThrottled) {
            m_pendingMenuUpdate = true;
            return;
        }
        performDebouncedMenuUpdate();
        m_isMenuUpdateThrottled = true;
        m_menuUpdateDebounceTimer->start();
    } else {
        m_menuUpdateDebounceTimer->stop();
        m_isMenuUpdateThrottled = false;
        m_pendingMenuUpdate = false;
        resetButtons();
        m_menuLoadedOnce = false;
    }
}

void AppMenuButtonGroup::onApplicationMenuChanged()
{
    if (m_isMenuUpdateThrottled) {
        m_pendingMenuUpdate = true;
        return;
    }
    performDebouncedMenuUpdate();
    m_isMenuUpdateThrottled = true;
    m_menuUpdateDebounceTimer->start();
}

void AppMenuButtonGroup::onMenuUpdateThrottleTimeout()
{
    m_isMenuUpdateThrottled = false;
    if (m_pendingMenuUpdate) {
        m_pendingMenuUpdate = false;
        onApplicationMenuChanged();
    }
}

void AppMenuButtonGroup::onDelayedCacheTimerTimeout()
{
    if (!m_appMenuModel || !m_menuToCache) {
        return;
    }

    if (m_buttonIndexOfMenuToCache == m_searchIndex) {
        m_appMenuModel->startDeepCaching();
    } else {
        m_appMenuModel->cacheSubtree(m_menuToCache.data());
    }
}

void AppMenuButtonGroup::performDebouncedMenuUpdate()
{
    auto *deco = qobject_cast<Decoration *>(decoration());
    if (!deco) {
        return;
    }
    auto decoratedClient = deco->window();
    if (m_appMenuModel && decoratedClient->hasApplicationMenu()) {
        const QString serviceName = decoratedClient->applicationMenuServiceName();
        const QString menuObjectPath = decoratedClient->applicationMenuObjectPath();
        if (!serviceName.isEmpty() && !menuObjectPath.isEmpty()) {
            m_appMenuModel->updateApplicationMenu(serviceName, menuObjectPath);
        } else {
            resetButtons();
        }
    }
}

void AppMenuButtonGroup::updateAppMenuModel()
{
    m_menuLoadedOnce = true;
    auto *deco = qobject_cast<Decoration *>(decoration());
    if (!deco) {
        return;
    }
    auto decoratedClient = deco->window();

    // Don't display AppMenu in modal windows.
    if (decoratedClient->isModal()) {
        resetButtons();
        return;
    }

    if (!decoratedClient->hasApplicationMenu()) {
        resetButtons();
        return;
    }

    if (m_appMenuModel) {
        m_menuReadyForSearch = false;

        QMenu *menu = m_appMenuModel->menu();
        if (!menu) {
            resetButtons();
            return;
        }

        const auto actions = menu->actions();
        const int menuActionCount = actions.count();
        // The button list contains menu buttons, plus an overflow button and a search button
        const int currentButtonCount = buttons().count() > 0 ? buttons().count() - 2 : 0;

        // If the number of actions is the same, just update the existing buttons
        if (menuActionCount > 0 && menuActionCount == currentButtonCount) {
            for (int i = 0; i < menuActionCount; ++i) {
                QAction *itemAction = actions.at(i);
                TextButton *button = qobject_cast<TextButton*>(buttons().at(i));
                if (button) {
                    const QString itemLabel = itemAction->text();

                    button->setText(itemLabel);
                    button->setAction(itemAction);

                    if (itemLabel.isEmpty()) {
                        button->setEnabled(false);
                        button->setVisible(false);
                    } else {
                        button->setEnabled(true);
                        button->setVisible(true);
                    }
                }
            }
        } else { // Fallback to the old, brute-force method if counts differ
            resetButtons();

            // Populate
            for (int i = 0; i < menuActionCount; ++i) {
                QAction *itemAction = actions.at(i);
                const QString itemLabel = itemAction->text();

                TextButton *b = new TextButton(deco, i, this);
                b->setText(itemLabel);
                b->setAction(itemAction);
                b->setOpacity(m_opacity);

                // Skip items with empty labels (The first item in a Gtk app)
                if (itemLabel.isEmpty()) {
                    b->setEnabled(false);
                    b->setVisible(false);
                }

                addButton(QPointer<KDecoration3::DecorationButton>(b));
            }

            if (menuActionCount > 0) {
                m_overflowIndex = menuActionCount;
                addButton(new MenuOverflowButton(deco, m_overflowIndex, this));

                if (deco->searchEnabled()) {
                    m_searchIndex = menuActionCount + 1;
                    addButton(new SearchButton(deco, m_searchIndex, this));
                }
            }
        }

        emit menuUpdated();
    }
}

AppMenuButtonGroup::ActionInfo AppMenuButtonGroup::getActionPath(QAction *action) const
{
    if (!action) {
        return { QString(), false };
    }

    QStringList path;
    bool isEffectivelyEnabled = action->isEnabled();

    path.prepend(action->text().remove(QLatin1Char('&')));

    QSet<QMenu*> visitedMenus;
    QMenu *currentMenu = qobject_cast<QMenu*>(action->parent());

    while (currentMenu) {
        if (visitedMenus.contains(currentMenu)) {
            qWarning() << "Menu hierarchy cycle detected, breaking.";
            break;
        }
        visitedMenus.insert(currentMenu);

        QAction *parentAction = currentMenu->menuAction();
        if (parentAction) {
            if (!parentAction->isEnabled()) {
                isEffectivelyEnabled = false;
            }
            const QString text = parentAction->text().remove(QLatin1Char('&'));
            if (!text.isEmpty()) {
                path.prepend(text);
            }
            currentMenu = qobject_cast<QMenu*>(parentAction->parent());
        } else {
            // Root menu
            currentMenu = nullptr;
        }
    }

    return { path.join(QStringLiteral(" » ")), isEffectivelyEnabled };
}

void AppMenuButtonGroup::setHamburgerMenu(bool value)
{
    if (m_hamburgerMenu != value) {
        m_hamburgerMenu = value;
    }
}

void AppMenuButtonGroup::updateOverflow(QRectF availableRect)
{
    bool showOverflow = m_hamburgerMenu;
    for (KDecoration3::DecorationButton *button : buttons()) {
        if (qobject_cast<MenuOverflowButton *>(button)) {
            button->setVisible(showOverflow);
        } else if (qobject_cast<TextButton *>(button)) {
            if (button->isEnabled()) {
                if (m_hamburgerMenu || !availableRect.contains(button->geometry())) {
                    button->setVisible(false);
                    showOverflow = true;
                } else {
                    button->setVisible(true);
                }
            }
        }
    }
    setOverflowing(showOverflow);

    // calculate visible width
    qreal currentVisibleWidth = 0;
    for (KDecoration3::DecorationButton *button : buttons()) {
        if (button->isVisible()) {
            currentVisibleWidth += button->geometry().width();
        }
    }

    if (m_visibleWidth != currentVisibleWidth) {
        m_visibleWidth = currentVisibleWidth;
        emit menuUpdated();
    }
}

qreal AppMenuButtonGroup::visibleWidth() const
{
    return m_visibleWidth;
}

bool AppMenuButtonGroup::menuLoadedOnce() const
{
    return m_menuLoadedOnce;
}

void AppMenuButtonGroup::popupMenu(QMenu *menu, int buttonIndex)
{
    // Stop any caching that may be in progress from a previously opened menu.
    m_appMenuModel->stopCaching();

    auto *deco = qobject_cast<Decoration *>(decoration());
    KDecoration3::DecorationButton *button = buttons().value(buttonIndex);
    if (!menu || !deco || !button) {
        return;
    }

    QMenu *oldMenu = m_currentMenu;
    KDecoration3::DecorationButton *oldButton = (0 <= m_currentIndex && m_currentIndex < buttons().length()) ? buttons().value(m_currentIndex) : nullptr;

    // 1. Set the new internal state. This must happen before popup for positioning.
    setCurrentIndex(buttonIndex);
    button->setChecked(true);
    m_currentMenu = menu;

    // 2. Calculate position and show the new menu. This must happen before hiding the old one to prevent flicker.
    if (auto navMenu = qobject_cast<NavigableMenu *>(menu)) {
        connect(navMenu, &NavigableMenu::hitLeft, this, &AppMenuButtonGroup::onHitLeft, Qt::UniqueConnection);
        connect(navMenu, &NavigableMenu::hitRight, this, &AppMenuButtonGroup::onHitRight, Qt::UniqueConnection);
    }
    menu->installEventFilter(this);
    if (KWindowSystem::isPlatformWayland()) {
        KDecoration3::Positioner positioner;
        positioner.setAnchorRect(button->geometry());
        deco->popup(positioner, menu);
    } else { //X11
        const QRectF buttonRect = button->geometry();
        const QPoint position = buttonRect.topLeft().toPoint();
        QPoint rootPosition(position);
        rootPosition += deco->windowPos();
        menu->popup(rootPosition);
    }

    if (buttonIndex == m_searchIndex) {
        m_searchLineEdit->activateWindow();
        m_searchLineEdit->setFocus();
        m_searchUiVisible = true;
    }

    // 3. Connect the hide signal for the new menu.
    connect(menu, &QMenu::aboutToHide, this, &AppMenuButtonGroup::onMenuAboutToHide, Qt::UniqueConnection);

    // 4. Clean up the old menu and button state.
    if (oldMenu && oldMenu != menu) {
        if (auto oldNavMenu = qobject_cast<NavigableMenu *>(oldMenu)) {
            disconnect(oldNavMenu, &NavigableMenu::hitLeft, this, &AppMenuButtonGroup::onHitLeft);
            disconnect(oldNavMenu, &NavigableMenu::hitRight, this, &AppMenuButtonGroup::onHitRight);
        }
        disconnect(oldMenu, &QMenu::aboutToHide, this, &AppMenuButtonGroup::onMenuAboutToHide);
        if (m_searchMenu && oldMenu == m_searchMenu) {
            m_searchUiVisible = false;
        }
        oldMenu->hide();
    }
    if (oldButton && oldButton != button) {
        oldButton->setChecked(false);
    }

    // After successfully showing a menu, predictively pre-fetch its children
    // after a short delay to keep the UI smooth.
    m_delayedCacheTimer->stop();
    m_menuToCache = menu;
    m_buttonIndexOfMenuToCache = buttonIndex;
    m_delayedCacheTimer->start();
}

void AppMenuButtonGroup::handleMenuButtonTrigger(int buttonIndex)
{
    if (!m_appMenuModel || !m_appMenuModel->menu() || buttonIndex >= m_appMenuModel->menu()->actions().count()) {
        return; // Index out of bounds
    }

    QAction *itemAction = m_appMenuModel->menu()->actions().at(buttonIndex);
    if (itemAction && itemAction->menu()) {
        QMenu *actionMenu = itemAction->menu();
        // If the menu is empty, we need to load it just-in-time.
        if (actionMenu->actions().isEmpty()) {
            // If we are already waiting for a different menu, cancel the old one.
            if (m_buttonIndexWaitingForPopup != -1 && m_buttonIndexWaitingForPopup != buttonIndex) {
                m_buttonIndexWaitingForPopup = -1;
            }

            // If we are already waiting for this menu, do nothing.
            if (m_buttonIndexWaitingForPopup == buttonIndex) {
                return;
            }

            m_buttonIndexWaitingForPopup = buttonIndex;
            m_appMenuModel->loadSubMenu(actionMenu);
            return; // Abort the trigger; the onSubMenuReady slot will re-trigger later.
        } else {
            // Menu is already loaded, show it.
            popupMenu(actionMenu, buttonIndex);
        }
    }
}

void AppMenuButtonGroup::handleSearchTrigger()
{
    if (m_currentIndex == m_searchIndex) {
        if (m_searchMenu) m_searchMenu->hide();
        return;
    }
    if (!m_searchMenu) {
        setupSearchMenu();
    }
    popupMenu(m_searchMenu, m_searchIndex);
}

void AppMenuButtonGroup::handleOverflowTrigger()
{
    // A latent bug would cause this to show a menu with all items if triggered
    // while the overflow button is invisible. This guard prevents that.
    if (!overflowing()) {
        return;
    }
    auto *actionMenu = new QMenu();
    actionMenu->setAttribute(Qt::WA_DeleteOnClose);

    if (m_appMenuModel && m_appMenuModel->menu()) {
        int overflowStartsAt = 0;
        // Find the first non-visible button to determine where the overflow starts
        for (KDecoration3::DecorationButton *b : buttons()) {
            auto *textButton = qobject_cast<TextButton *>(b);
            if (textButton && textButton->isEnabled() && !textButton->isVisible()) {
                overflowStartsAt = textButton->buttonIndex();
                break;
            }
        }

        const auto actions = m_appMenuModel->menu()->actions();
        for (int i = overflowStartsAt; i < actions.count(); ++i) {
            actionMenu->addAction(actions.at(i));
        }
    }

    popupMenu(actionMenu, m_overflowIndex);
}

void AppMenuButtonGroup::trigger(int buttonIndex)
{
    // The button is checked in popupMenu, but we need to check it here
    // for the case where the menu is not yet loaded.
    KDecoration3::DecorationButton *button = buttons().value(buttonIndex);
    if (!button) {
        return;
    }

    if (buttonIndex == m_searchIndex) {
        handleSearchTrigger();
    } else if (buttonIndex == m_overflowIndex) {
        handleOverflowTrigger();
    } else {
        handleMenuButtonTrigger(buttonIndex);
    }
}

void AppMenuButtonGroup::triggerOverflow()
{
    trigger(m_overflowIndex);
}

// FIXME TODO doesn't work on submenu
bool AppMenuButtonGroup::eventFilter(QObject *watched, QEvent *event)
{
    // Event handling for the search bar's QLineEdit
     if (watched == m_searchLineEdit) {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);

            // Forward Left/Right key events to m_searchMenu when at the line boundaries
            if ((keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right) &&
                keyEvent->modifiers() == Qt::NoModifier)
            {
                bool atBoundary = (keyEvent->key() == Qt::Key_Left && m_searchLineEdit->cursorPosition() == 0)
                || (keyEvent->key() == Qt::Key_Right && m_searchLineEdit->cursorPosition() == m_searchLineEdit->text().length());
                
                if (atBoundary) {
                    QApplication::sendEvent(m_searchMenu, keyEvent);
                    return true;
                }
                return false;
            }
        
        }
        return false;
    }

    auto *menu = qobject_cast<QMenu *>(watched);

    if (!menu) {
        return KDecoration3::DecorationButtonGroup::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseMove) {
        if (KWindowSystem::isPlatformX11()) {
            auto *e = static_cast<QMouseEvent *>(event);
            auto *deco = const_cast<Decoration*>(qobject_cast<const Decoration *>(decoration()));
            if (deco) {
                // Forward a constructed HoverEvent to the decoration to handle.
                // This is a workaround for X11 where the decoration does not receive
                // hover events while a menu is open.
                QPointF localPos = e->globalPosition() - deco->windowPos();
                localPos.setY(localPos.y() + deco->titleBarHeight());
                
                QHoverEvent hoverEvent(QEvent::HoverMove, localPos, e->globalPosition(), localPos, e->modifiers(), static_cast<const QPointingDevice *>(e->device()));
                QApplication::sendEvent(deco, &hoverEvent);
            }
        }
        return false;
    } 

    return KDecoration3::DecorationButtonGroup::eventFilter(watched, event);
}

bool AppMenuButtonGroup::isMenuOpen() const
{
    return 0 <= m_currentIndex;
}

void AppMenuButtonGroup::unPressAllButtons()
{
    for (auto *decoButton : buttons()) {
        if (auto *button = qobject_cast<Button *>(decoButton)) {
            button->forceUnpress();
        }
    }
}

void AppMenuButtonGroup::updateShowing()
{
    setShowing(m_alwaysShow || m_hovered || isMenuOpen());
}

void AppMenuButtonGroup::onMenuAboutToHide()
{
    if (auto navMenu = qobject_cast<NavigableMenu *>(sender())) {
        disconnect(navMenu, &NavigableMenu::hitLeft, this, &AppMenuButtonGroup::onHitLeft);
        disconnect(navMenu, &NavigableMenu::hitRight, this, &AppMenuButtonGroup::onHitRight);
    }

    if (m_searchLineEdit) {
        m_searchLineEdit->clear();
        m_searchUiVisible = false;
        m_lastResults.clear();
        m_lastSearchQuery.clear();
    }

    if (0 <= m_currentIndex && m_currentIndex < buttons().length()) {
        buttons().value(m_currentIndex)->setChecked(false);
    }
    setCurrentIndex(-1);
    m_currentMenu = nullptr;
    m_hoveredButton = nullptr;
}

void AppMenuButtonGroup::onHitLeft()
{
    int desiredIndex = findNextVisibleButtonIndex(m_currentIndex, false);
    trigger(desiredIndex);
}

void AppMenuButtonGroup::onHitRight()
{
    int desiredIndex = findNextVisibleButtonIndex(m_currentIndex, true);
    trigger(desiredIndex);
}

void AppMenuButtonGroup::onShowingChanged(bool showing)
{
    if (m_animationEnabled) {
        QAbstractAnimation::Direction dir = showing ? QAbstractAnimation::Forward : QAbstractAnimation::Backward;
        if (m_animation->state() == QAbstractAnimation::Running && m_animation->direction() != dir) {
            m_animation->stop();
        }
        m_animation->setDirection(dir);
        if (m_animation->state() != QAbstractAnimation::Running) {
            m_animation->start();
        }
    } else {
        setOpacity(showing ? 1 : 0);
    }
}

void AppMenuButtonGroup::filterMenu(const QString &text)
{
    m_lastSearchQuery = text;

    // Clear results if search text is too short
    if (text.length() < 3) {
        const auto actions = m_searchMenu->actions();
        if (actions.count() > 2) {
            for (int i = actions.count() - 1; i >= 2; --i) {
                m_searchMenu->removeAction(actions.at(i));
            }
        }
        m_lastResults.clear();
        repositionSearchMenu();

        if (text.isEmpty()) {
            m_searchLineEdit->setClearButtonEnabled(false);
            m_searchLineEdit->setPlaceholderText(i18nd("plasma_applet_org.kde.plasma.appmenu", "Search") + QStringLiteral("…"));
            return;
        }
        m_searchLineEdit->setClearButtonEnabled(true);
        return;
    } else {
        m_searchLineEdit->setClearButtonEnabled(true);
    }

    if (!m_appMenuModel || !m_menuReadyForSearch) {
        // Menu is not ready yet, search will be re-triggered later
        return;
    }

    // Find results
    QList<QAction *> results;
    if (m_appMenuModel) {
        QMenu *rootMenu = m_appMenuModel->menu();
        if (rootMenu) {
            searchMenu(rootMenu, text, results);
        }
    }

    // If results are the same as last time, do nothing to prevent the freeze.
    if (m_lastResults == results) {
        return;
    }

    m_searchMenu->setUpdatesEnabled(false);

    m_lastResults = results;

    // Clear previous results
    const auto actions = m_searchMenu->actions();
    for (int i = actions.count() - 1; i >= 2; --i) {
        m_searchMenu->removeAction(actions.at(i));
    }

    // Add new results
    const auto *deco = qobject_cast<const Decoration *>(decoration());
    if (!deco) {
        m_searchMenu->setUpdatesEnabled(true);
        return;
    }

    for (QAction *action : results) {
        const ActionInfo info = getActionPath(action);
        if (!info.isEffectivelyEnabled && !deco->showDisabledActions()) {
            continue;
        }
        QAction *newAction = new QAction(action->icon(), info.path, m_searchMenu);
        newAction->setEnabled(info.isEffectivelyEnabled);
        newAction->setCheckable(action->isCheckable());
        newAction->setChecked(action->isChecked());
        connect(newAction, &QAction::triggered, this, [action, this]() {
            action->trigger();
            m_searchMenu->hide();
        });
        m_searchMenu->addAction(newAction);
    }

    repositionSearchMenu();
    m_searchMenu->setUpdatesEnabled(true);
    qCDebug(category) << "[AppMenuButtonGroup] filterMenu(" << text << ") ended";
}

void AppMenuButtonGroup::onSubMenuReady(QMenu *menu)
{
    if (m_buttonIndexWaitingForPopup == -1 || !m_appMenuModel || !m_appMenuModel->menu()) {
        return;
    }

    const auto actions = m_appMenuModel->menu()->actions();
    if (m_buttonIndexWaitingForPopup >= actions.count()) {
        m_buttonIndexWaitingForPopup = -1;
        return;
    }

    QAction *action = actions.at(m_buttonIndexWaitingForPopup);
    if (action) {
        // The menu we were waiting for is now ready.
        // We must replace the action's menu with the new, populated one.
        action->setMenu(menu);
        // We can now trigger the button again to pop it up.
        // It is crucial to reset the waiting index *before* calling trigger
        // to prevent an infinite loop.
        const int buttonIndex = m_buttonIndexWaitingForPopup;
        m_buttonIndexWaitingForPopup = -1;
        trigger(buttonIndex);
    }
}

void AppMenuButtonGroup::onSearchTimerTimeout()
{
    if (m_searchLineEdit) {
        filterMenu(m_searchLineEdit->text());
    }
}

void AppMenuButtonGroup::searchMenu(QMenu *menu, const QString &text, QList<QAction *> &results)
{
    for (QAction *action : menu->actions()) {
        if (action->isSeparator()) {
            continue;
        }
        if (action->menu()) {
            searchMenu(action->menu(), text, results);
        } else {
            const ActionInfo info = getActionPath(action);
            if (info.path.contains(text, Qt::CaseInsensitive)) {
                results.append(action);
            }
        }
    }
}

int AppMenuButtonGroup::findNextVisibleButtonIndex(int currentIndex, bool forward) const
{
    const auto buttonList = buttons();
    if (buttonList.isEmpty()) {
        return -1;
    }

    int step = forward ? 1 : -1;
    // Start from the next button, not the current one
    int newIndex = currentIndex + step;

    for (int i = 0; i < buttonList.length(); ++i) {
        // Wrap around logic
        if (newIndex < 0) {
            newIndex = buttonList.length() - 1;
        } else if (newIndex >= buttonList.length()) {
            newIndex = 0;
        }

        const auto *button = buttonList.value(newIndex);
        if (button && button->isVisible() && button->isEnabled()) {
            return newIndex;
        }

        newIndex += step;
    }

    return currentIndex; // Fallback to current index if no other visible button is found
}

void AppMenuButtonGroup::handleHoverMove(const QPointF &pos)
{
    if (!isMenuOpen()) {
        return;
    }

    KDecoration3::DecorationButton *newHoveredButton = buttonAt(pos.toPoint());

    if (m_hoveredButton != newHoveredButton) {
        m_hoveredButton = newHoveredButton;

        if (m_hoveredButton) {
            auto *appMenuButton = qobject_cast<AppMenuButton *>(m_hoveredButton.data());
            if (appMenuButton) {
                if (m_currentIndex != appMenuButton->buttonIndex()
                    && appMenuButton->isVisible()
                    && appMenuButton->isEnabled()
                ) {
                    trigger(appMenuButton->buttonIndex());
                }
            }
        }
    }
}

} // namespace Material
