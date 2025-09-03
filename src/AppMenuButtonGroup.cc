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
#include "AppMenuButton.h"
#include "TextButton.h"
#include "MenuOverflowButton.h"
#include "SearchButton.h"

// KDecoration
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/DecorationButton>
#include <KDecoration3/DecorationButtonGroup>

// KF
#include <KWindowSystem>
#include <KLocalizedString>

// KWIN
#include <kwin-x11/x11window.h>

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
    , m_appMenuModel(nullptr)
    , m_currentIndex(-1)
    , m_overflowIndex(-1)
    , m_searchIndex(-1)
    , m_hovered(false)
    , m_showing(true)
    , m_alwaysShow(true)
    , m_animationEnabled(false)
    , m_animation(new QVariantAnimation(this))
    , m_opacity(1)
    , m_searchMenu(nullptr)
    , m_searchLineEdit(nullptr)
    , m_searchDebounceTimer(nullptr)
    , m_searchUiVisible(false)
{
    m_searchDebounceTimer = new QTimer(this);
    m_searchDebounceTimer->setInterval(200);
    m_searchDebounceTimer->setSingleShot(true);
    connect(m_searchDebounceTimer, &QTimer::timeout, this, &AppMenuButtonGroup::onSearchTimerTimeout);
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
    connect(this, &AppMenuButtonGroup::opacityChanged, this, [this]() {
        // update();
    });

    auto decoratedClient = decoration->window();
    connect(decoratedClient, &KDecoration3::DecoratedWindow::hasApplicationMenuChanged,
            this, &AppMenuButtonGroup::updateAppMenuModel);
    connect(this, &AppMenuButtonGroup::requestActivateIndex,
            this, &AppMenuButtonGroup::trigger);
    connect(this, &AppMenuButtonGroup::requestActivateOverflow,
            this, &AppMenuButtonGroup::triggerOverflow);

}

AppMenuButtonGroup::~AppMenuButtonGroup() = default;

void AppMenuButtonGroup::setupSearchMenu()
{
    m_searchMenu = new QMenu(nullptr);
    m_searchLineEdit = new QLineEdit(m_searchMenu);
    m_searchLineEdit->setMinimumWidth(200);

    auto *searchAction = new QWidgetAction(m_searchMenu);
    searchAction->setDefaultWidget(m_searchLineEdit);
    m_searchMenu->addAction(searchAction);
    m_searchMenu->addSeparator();

    m_searchMenu->installEventFilter(this);

    connect(m_searchLineEdit, &QLineEdit::textChanged, m_searchDebounceTimer, qOverload<>(&QTimer::start));
    connect(m_searchLineEdit, &QLineEdit::returnPressed, this, &AppMenuButtonGroup::onSearchReturnPressed);

    m_searchLineEdit->installEventFilter(this);
    m_searchLineEdit->setFocusPolicy(Qt::StrongFocus);
    m_searchLineEdit->setPlaceholderText(i18nd("plasma_applet_org.kde.plasma.appmenu","Search")+QStringLiteral("…"));
    m_searchLineEdit->setClearButtonEnabled(false);
}

int AppMenuButtonGroup::currentIndex() const
{
    return m_currentIndex;
}

void AppMenuButtonGroup::setCurrentIndex(int set)
{
    if (m_currentIndex != set) {
        m_currentIndex = set;
        // qCDebug(category) << this << "setCurrentIndex" << m_currentIndex;
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
        // qCDebug(category) << this << "setOverflowing" << m_overflowing;
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
        // qCDebug(category) << this << "setHovered" << m_hovered;
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
        // qCDebug(category) << this << "setShowing" << m_showing << "alwaysShow" << m_alwaysShow << "currentIndex" << m_currentIndex << "opacity" << m_opacity;
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
        // qCDebug(category) << this << "setAlwaysShow" << m_alwaysShow;
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

        for (int i = 0; i < buttons().length(); i++) {
            KDecoration3::DecorationButton* decoButton = buttons().value(i);
            auto *button = qobject_cast<Button *>(decoButton);
            if (button) {
                button->setOpacity(m_opacity);
            }
        }

        emit opacityChanged(value);
    }
}

KDecoration3::DecorationButton* AppMenuButtonGroup::buttonAt(int x, int y) const
{
    for (int i = 0; i < buttons().length(); i++) {
        KDecoration3::DecorationButton* button = buttons().value(i);
        if (!button->isVisible()) {
            continue;
        }
        if (button->geometry().contains(x, y)) {
            return button;
        }
    }
    return nullptr;
}

void AppMenuButtonGroup::resetButtons()
{
    // qCDebug(category) << "    resetButtons";
    const auto currentButtons = buttons();
    if (currentButtons.isEmpty()) {
        return;
    }

    // This removes all buttons with the "Custom" type from the group's list,
    // but does not delete the button widgets themselves.
    removeButton(KDecoration3::DecorationButtonType::Custom);

    // Now we can safely delete the buttons we took ownership of.
    qDeleteAll(currentButtons);

    emit menuUpdated();
}

void AppMenuButtonGroup::onMenuReadyForSearch()
{
    m_menuReadyForSearch = true;
    if (!m_lastSearchQuery.isEmpty() && m_searchUiVisible) {
        filterMenu(m_lastSearchQuery);
    }
}

void AppMenuButtonGroup::initAppMenuModel()
{
    m_appMenuModel = new AppMenuModel(this);
    connect(m_appMenuModel, &AppMenuModel::modelReset,
        this, &AppMenuButtonGroup::updateAppMenuModel);
    connect(m_appMenuModel, &AppMenuModel::menuReadyForSearch,
        this, &AppMenuButtonGroup::onMenuReadyForSearch);
    // qCDebug(category) << "AppMenuModel" << m_appMenuModel;
}

void AppMenuButtonGroup::updateAppMenuModel()
{
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
            emit menuUpdated();
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

    } else {
        // Init AppMenuModel
        // qCDebug(category) << "windowId" << decoratedClient->windowId();
        if (KWindowSystem::isPlatformX11()) {
#if HAVE_X11
            const WId windowId = deco->safeWindowId();
            if (windowId != 0) {
                initAppMenuModel();
                m_appMenuModel->setWinId(windowId);
                // qCDebug(category) << "AppMenuModel" << m_appMenuModel;
            }
#endif
        } else if (KWindowSystem::isPlatformWayland()) {
#if HAVE_Wayland
            // TODO
#endif
        }
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

void AppMenuButtonGroup::updateOverflow(QRectF availableRect)
{
    // qCDebug(category) << "updateOverflow" << availableRect;
    bool showOverflow = false;
    for (KDecoration3::DecorationButton *button : buttons()) {
        // qCDebug(category) << "    " << button->geometry() << button;
        if (qobject_cast<MenuOverflowButton *>(button)) {
            button->setVisible(showOverflow);
            // qCDebug(category) << "    showOverflow" << showOverflow;
        } else if (qobject_cast<TextButton *>(button)) {
            if (button->isEnabled()) {
                if (availableRect.contains(button->geometry())) {
                    button->setVisible(true);
                } else {
                    button->setVisible(false);
                    showOverflow = true;
                }
            }
        }
    }
    setOverflowing(showOverflow);
}

void AppMenuButtonGroup::trigger(int buttonIndex)
{
    KDecoration3::DecorationButton *button = buttons().value(buttonIndex);
    if (!button) {
        return;
    }

    QMenu *actionMenu = nullptr;

    if (buttonIndex == m_searchIndex) {
        if (m_currentIndex == m_searchIndex) {
            if (m_searchMenu) m_searchMenu->hide();
            return;
        }
        if (!m_searchMenu) {
            setupSearchMenu();
        }
        actionMenu = m_searchMenu;
    } else if (buttonIndex == m_overflowIndex) {
        // A latent bug would cause this to show a menu with all items if triggered
        // while the overflow button is invisible. This guard prevents that.
        if (!overflowing()) {
            return;
        }
        actionMenu = new QMenu();
        actionMenu->setAttribute(Qt::WA_DeleteOnClose);

        if (m_appMenuModel && m_appMenuModel->menu()) {
            int overflowStartsAt = 0;
            // Find the first non-visible button to determine where the overflow starts
            for (KDecoration3::DecorationButton *b : buttons()) {
                TextButton *textButton = qobject_cast<TextButton *>(b);
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
    } else {
        // Regular menu button
        if (!m_appMenuModel || !m_appMenuModel->menu() || buttonIndex >= m_appMenuModel->menu()->actions().count()) {
            return; // Index out of bounds for regular actions
        }
        QAction *itemAction = m_appMenuModel->menu()->actions().at(buttonIndex);
        if (itemAction) {
            actionMenu = itemAction->menu();
        }
    }

    const auto *deco = qobject_cast<Decoration *>(decoration());

    if (actionMenu && deco) {
       
        QMenu *oldMenu = m_currentMenu;
        KDecoration3::DecorationButton *oldButton = (0 <= m_currentIndex && m_currentIndex < buttons().length()) ? buttons().value(m_currentIndex) : nullptr;

        // 1. Set the new internal state. This must happen before popup for positioning.
        setCurrentIndex(buttonIndex);
        button->setChecked(true);
        m_currentMenu = actionMenu;

        // 2. Calculate position and show the new menu. This must happen before hiding the old one to prevent flicker.
        const QRectF buttonRect = button->geometry();
        const QPoint position = buttonRect.topLeft().toPoint();
        QPoint rootPosition(position);
        rootPosition += deco->windowPos();

        actionMenu->installEventFilter(this);
        actionMenu->popup(rootPosition);
        clampToScreen(actionMenu);

        if (buttonIndex == m_searchIndex) {
            m_searchLineEdit->activateWindow();
            m_searchLineEdit->setFocus();
            m_searchUiVisible = true;
        }

        // 3. Connect the hide signal for the new menu.
        connect(actionMenu, &QMenu::aboutToHide, this, &AppMenuButtonGroup::onMenuAboutToHide, Qt::UniqueConnection);

        // 4. Clean up the old menu and button state.
        if (oldMenu && oldMenu != actionMenu) {
            disconnect(oldMenu, &QMenu::aboutToHide, this, &AppMenuButtonGroup::onMenuAboutToHide);
            if (m_searchMenu && oldMenu == m_searchMenu) {
                m_searchUiVisible = false;
            }
            oldMenu->hide();
        }
        if (oldButton && oldButton != button) {
            oldButton->setChecked(false);
        }
    }
}

void AppMenuButtonGroup::triggerOverflow()
{
    // qCDebug(category) << "AppMenuButtonGroup::triggerOverflow" << m_overflowIndex;
    trigger(m_overflowIndex);
}

// FIXME TODO doesn't work on submenu
bool AppMenuButtonGroup::eventFilter(QObject *watched, QEvent *event)
{
    // Event handling for the search bar's QLineEdit
    if (watched == m_searchLineEdit) {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);

            // On Key_Down, jump from the search bar to the first result in the menu
            if (keyEvent->key() == Qt::Key_Down) {
                const auto actions = m_searchMenu->actions();
                for (int i = 2; i < actions.count(); ++i) {
                    if (actions.at(i)->isEnabled()) {
                        m_searchMenu->setFocus();
                        m_searchMenu->setActiveAction(actions.at(i));
                        return true; // Event handled
                    }
                }
                return true; // Consume the event even if no action is enabled
            }

            // On Key_Up, let's QT manage the menu (default: go to the last active action)
            if (keyEvent->key() == Qt::Key_Up) {
                if (m_searchMenu->actions().count() > 2) {
                    m_searchLineEdit->clearFocus();
                    m_searchMenu->setFocus();
                    return false;
                }
            }

            // On Key_Left at the beginning of the line, navigate to the previous visible menu button.
            if (keyEvent->key() == Qt::Key_Left) {
                if (m_searchLineEdit->cursorPosition() == 0) {
                    const int desiredIndex = findNextVisibleButtonIndex(m_searchIndex, false);
                    if (desiredIndex != m_searchIndex) {
                        trigger(desiredIndex);
                    }
                    return true;
                }
            }
        }
    }

    // Jump to searchLineEdit when the user press Key_Up in menu
    if (watched == m_searchMenu && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Up) {
            const auto actions = m_searchMenu->actions();
            QAction *activeAction = m_searchMenu->activeAction();
            if (actions.count() > 2 && activeAction == actions.at(2)) {
                m_searchLineEdit->setFocus();
                m_searchMenu->setActiveAction(nullptr);
                return true; // Event handled
            }
        }
    }

    auto *menu = qobject_cast<QMenu *>(watched);

    if (!menu) {
        return KDecoration3::DecorationButtonGroup::eventFilter(watched, event);
    }
    
    

    if (event->type() == QEvent::KeyPress) {
        auto *e = static_cast<QKeyEvent *>(event);

        // TODO right to left languages
        if (e->key() == Qt::Key_Left) {
            int desiredIndex = findNextVisibleButtonIndex(m_currentIndex, false);
            emit requestActivateIndex(desiredIndex);
            return true;
        } else if (e->key() == Qt::Key_Right) {
            if (menu->activeAction() && menu->activeAction()->menu()) {
                return false;
            }

            int desiredIndex = findNextVisibleButtonIndex(m_currentIndex, true);
            emit requestActivateIndex(desiredIndex);
            return true;
        }

    } else if (event->type() == QEvent::MouseMove) {
        auto *e = static_cast<QMouseEvent *>(event);

        const auto *deco = qobject_cast<Decoration *>(decoration());

        QPoint decoPos(e->globalPosition().toPoint());
        decoPos -= deco->windowPos();
        decoPos.ry() += deco->titleBarHeight();
        // qCDebug(category) << "MouseMove";
        // qCDebug(category) << "       globalPos" << e->globalPos();
        // qCDebug(category) << "       windowPos" << deco->windowPos();
        // qCDebug(category) << "  titleBarHeight" << deco->titleBarHeight();

        KDecoration3::DecorationButton* item = buttonAt(decoPos.x(), decoPos.y());
        if (!item) {
            return false;
        }

        AppMenuButton* appMenuButton = qobject_cast<AppMenuButton *>(item);
        if (appMenuButton) {
            if (m_currentIndex != appMenuButton->buttonIndex()
                && appMenuButton->isVisible()
                && appMenuButton->isEnabled()
            ) {
                emit requestActivateIndex(appMenuButton->buttonIndex());
            }
            return false;
        }
    }
    
    return KDecoration3::DecorationButtonGroup::eventFilter(watched, event);
}

bool AppMenuButtonGroup::isMenuOpen() const
{
    return 0 <= m_currentIndex;
}

void AppMenuButtonGroup::unPressAllButtons()
{
    // qCDebug(category) << "AppMenuButtonGroup::unPressAllButtons";
    for (int i = 0; i < buttons().length(); i++) {
        KDecoration3::DecorationButton* button = buttons().value(i);

        // Hack to setPressed(false)
        button->setEnabled(!button->isEnabled());
        button->setEnabled(!button->isEnabled());
    }
}

void AppMenuButtonGroup::updateShowing()
{
    setShowing(m_alwaysShow || m_hovered || isMenuOpen());
}

void AppMenuButtonGroup::onMenuAboutToHide()
{
    qCDebug(category) << "[onMenuAboutToHide] started";
    if (m_searchLineEdit) {
        m_searchLineEdit->clear();
        m_searchUiVisible = false;
        m_lastResults.clear();
        qCDebug(category) << "[onMenuAboutToHide] search cleared";
    }

    if (0 <= m_currentIndex && m_currentIndex < buttons().length()) {
        buttons().value(m_currentIndex)->setChecked(false);
    }
    setCurrentIndex(-1);
    m_currentMenu = nullptr;
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
    qCDebug(category) << "[filtermenu]";
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
        //HACK To get the scrollbars to disapper, we force the popup again 
        if (m_searchMenu->isVisible()) {
            const QPoint pos = m_searchMenu->pos();
            m_searchMenu->popup(pos);
            clampToScreen(m_searchMenu);
            qCDebug(category) << "popup()";
            //m_searchLineEdit->setFocus();
        }        
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

    //HACK To get the scrollbars to appear, we force the popup again
    if (m_searchMenu->isVisible()) {
        const QPoint pos = m_searchMenu->pos();
        m_searchMenu->popup(pos);
        clampToScreen(m_searchMenu);
    }
    m_searchMenu->setUpdatesEnabled(true);
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

void AppMenuButtonGroup::onSearchReturnPressed()
{
    // Trigger the first "real" action in the menu
    const auto actions = m_searchMenu->actions();
    if (actions.count() > 2) { // 0 is search bar, 1 is separator
        actions.at(2)->trigger();
    }
}

void AppMenuButtonGroup::clampToScreen(QMenu* menu)
{
    qCDebug(category) << "[clampToScreen]";
    //qCDebug(category) << " m_currentIndex  =" << m_currentIndex;
    //qCDebug(category) << " m_overflowIndex =" << m_overflowIndex;
    //qCDebug(category) << " m_searchIndex   =" << m_searchIndex;
    //qCDebug(category) << " buttons length  =" << buttons().length();
    
    
    const auto *deco = qobject_cast<Decoration *>(decoration());
    if (!deco) {
        return;
    }

    KDecoration3::DecorationButton* anchorButton = buttons().value(m_currentIndex);

    QPoint idealPos;
    if (anchorButton) {
        const QRectF buttonGeometry = anchorButton->geometry();
        idealPos = buttonGeometry.topLeft().toPoint();
        idealPos += deco->windowPos();
    } else {
        idealPos = menu->pos();
    }

    QScreen *screen = QGuiApplication::screenAt(idealPos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    const QRect bounds = screen->availableGeometry();

    // Set max size 
    menu->setMaximumHeight(bounds.height());
    menu->setMaximumWidth(static_cast<int>(bounds.width() * 0.8));

    int w = menu->width();
    int h = menu->height();

    int minX = bounds.left();
    int maxX = bounds.left() + bounds.width()  - w;
    int minY = bounds.top();
    int maxY =
        bounds.top()  + bounds.height() - h;

    if (w > bounds.width())  { minX = maxX = bounds.left(); }
    if (h > bounds.height()) { minY = maxY = bounds.top();  }

    idealPos.setX(qBound(minX, idealPos.x(), maxX));
    idealPos.setY(qBound(minY, idealPos.y(), maxY));

    if (menu->pos() != idealPos) {
        menu->move(idealPos);
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

} // namespace Material
