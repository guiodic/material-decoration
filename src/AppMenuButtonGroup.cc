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
//#include "InternalSettings.h"

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
    , m_searchButton(nullptr)
    , m_searchMenu(nullptr)
    , m_searchLineEdit(nullptr)
    , m_searchUiVisible(false)
{
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

    auto *searchAction = new QWidgetAction(m_searchMenu);
    searchAction->setDefaultWidget(m_searchLineEdit);
    m_searchMenu->addAction(searchAction);
    m_searchMenu->addSeparator();

    m_searchMenu->installEventFilter(this);

    connect(m_searchLineEdit, &QLineEdit::textChanged, this, &AppMenuButtonGroup::filterMenu);
    connect(m_searchLineEdit, &QLineEdit::returnPressed, this, &AppMenuButtonGroup::onSearchReturnPressed);
    connect(m_searchMenu, &QMenu::aboutToHide, this, &AppMenuButtonGroup::onSearchMenuHidden);

    m_searchLineEdit->installEventFilter(this);
    m_searchLineEdit->setFocusPolicy(Qt::StrongFocus);
    m_searchLineEdit->setPlaceholderText(i18nd("plasma_applet_org.kde.plasma.appmenu","Search")+QStringLiteral("…"));
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
    if (!m_lastSearchQuery.isEmpty()) {
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

        const int modelActionCount = m_appMenuModel->rowCount();
        // The button list contains menu buttons, plus an overflow button and a search button
        const int currentButtonCount = buttons().count() > 0 ? buttons().count() - 2 : 0;

        // If the number of actions is the same, just update the existing buttons
        if (modelActionCount > 0 && modelActionCount == currentButtonCount) {
            for (int row = 0; row < modelActionCount; ++row) {
                TextButton *button = qobject_cast<TextButton*>(buttons().at(row));
                if (button) {
                    const QModelIndex index = m_appMenuModel->index(row, 0);
                    const QString itemLabel = m_appMenuModel->data(index, AppMenuModel::MenuRole).toString();
                    const QVariant data = m_appMenuModel->data(index, AppMenuModel::ActionRole);
                    QAction *itemAction = (QAction *)data.value<void *>();

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
            for (int row = 0; row < m_appMenuModel->rowCount(); row++) {
                const QModelIndex index = m_appMenuModel->index(row, 0);
                const QString itemLabel = m_appMenuModel->data(index, AppMenuModel::MenuRole).toString();

                const QVariant data = m_appMenuModel->data(index, AppMenuModel::ActionRole);
                QAction *itemAction = (QAction *)data.value<void *>();

                TextButton *b = new TextButton(deco, row, this);
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

            if (m_appMenuModel->rowCount() > 0) {
                m_overflowIndex = m_appMenuModel->rowCount();
                addButton(new MenuOverflowButton(deco, m_overflowIndex, this));
                
                if (deco->searchEnabled()) {
                    m_searchIndex = m_appMenuModel->rowCount() + 1;
                    m_searchButton = new SearchButton(deco, this);
                    connect(m_searchButton, &SearchButton::clicked, this, [this] {
                        trigger(m_searchIndex);
                    });
                    addButton(m_searchButton);
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

void AppMenuButtonGroup::trigger(int buttonIndex) {
    if (buttonIndex == m_searchIndex) {
        if (!m_searchMenu) {
            setupSearchMenu();
        }
        if (m_searchUiVisible) {
            m_searchMenu->hide();
            // onSearchMenuHidden will do the rest
        } else {
            const auto *deco = qobject_cast<Decoration *>(decoration());
            if (!deco || !m_searchButton) {
                return;
            }
            const QRectF searchButtonGeometry = m_searchButton->geometry();
            const QPoint position = searchButtonGeometry.topLeft().toPoint();
            QPoint globalPos(position);
            globalPos += deco->windowPos();

            m_searchMenu->popup(globalPos);

            m_searchLineEdit->activateWindow();
            m_searchLineEdit->setFocus();
            m_searchUiVisible = true;
        }
        return;
    }

    if (!m_appMenuModel || buttonIndex > m_appMenuModel->rowCount()) {
        return; // Overflow button clicked or model not ready
    }
    // qCDebug(category) << "AppMenuButtonGroup::trigger" << buttonIndex;
    KDecoration3::DecorationButton* button = buttons().value(buttonIndex);

    // https://github.com/psifidotos/applet-window-appmenu/blob/908e60831d7d68ee56a56f9c24017a71822fc02d/lib/appmenuapplet.cpp#L167
    QMenu *actionMenu = nullptr;

    if (buttonIndex == m_appMenuModel->rowCount()) {
        // Overflow Menu
        actionMenu = new QMenu();
        actionMenu->setAttribute(Qt::WA_DeleteOnClose);

        int overflowStartsAt = 0;
        for (KDecoration3::DecorationButton *b : buttons()) {
            TextButton* textButton = qobject_cast<TextButton *>(b);
            if (textButton && textButton->isEnabled() && !textButton->isVisible()) {
                overflowStartsAt = textButton->buttonIndex();
                break;
            }
        }

        QAction *action = nullptr;
        for (int i = overflowStartsAt; i < m_appMenuModel->rowCount(); i++) {
            const QModelIndex index = m_appMenuModel->index(i, 0);
            const QVariant data = m_appMenuModel->data(index, AppMenuModel::ActionRole);
            action = (QAction *)data.value<void *>();
            actionMenu->addAction(action);
        }

    } else {
        const QModelIndex modelIndex = m_appMenuModel->index(buttonIndex, 0);
        const QVariant data = m_appMenuModel->data(modelIndex, AppMenuModel::ActionRole);
        QAction *itemAction = (QAction *)data.value<void *>();
        // qCDebug(category) << "    action" << itemAction;

        if (itemAction) {
            actionMenu = itemAction->menu();
            // qCDebug(category) << "    menu" << actionMenu;
        }
    }

    const auto *deco = qobject_cast<Decoration *>(decoration());
    // if (actionMenu && deco) {
    //     auto *decoratedClient = deco->window();
    //     actionMenu->setPalette(decoratedClient->palette());
    // }

    if (actionMenu && deco) {
        QRectF buttonRect = button->geometry();
        QPoint position = buttonRect.topLeft().toPoint();
        QPoint rootPosition(position);
        rootPosition += deco->windowPos();
        // qCDebug(category) << "    windowPos" << windowPos;

        // auto connection( QX11Info::connection() );

        // button release event
        // xcb_button_release_event_t releaseEvent;
        // memset(&releaseEvent, 0, sizeof(releaseEvent));

        // releaseEvent.response_type = XCB_BUTTON_RELEASE;
        // releaseEvent.event =  windowId;
        // releaseEvent.child = XCB_WINDOW_NONE;
        // releaseEvent.root = QX11Info::appRootWindow();
        // releaseEvent.event_x = position.x();
        // releaseEvent.event_y = position.y();
        // releaseEvent.root_x = rootPosition.x();
        // releaseEvent.root_y = rootPosition.y();
        // releaseEvent.detail = XCB_BUTTON_INDEX_1;
        // releaseEvent.state = XCB_BUTTON_MASK_1;
        // releaseEvent.time = XCB_CURRENT_TIME;
        // releaseEvent.same_screen = true;
        // xcb_send_event( connection, false, windowId, XCB_EVENT_MASK_BUTTON_RELEASE, reinterpret_cast<const char*>(&releaseEvent));

        // xcb_ungrab_pointer( connection, XCB_TIME_CURRENT_TIME );
        //---

        actionMenu->installEventFilter(this);

        if (!KWindowSystem::isPlatformWayland()) {
            actionMenu->popup(rootPosition);
        }

        QMenu *oldMenu = m_currentMenu;
        m_currentMenu = actionMenu;

        if (oldMenu && oldMenu != actionMenu) {
            // Don't reset the currentIndex when another menu is already shown
            disconnect(oldMenu, &QMenu::aboutToHide, this, &AppMenuButtonGroup::onMenuAboutToHide);
            oldMenu->hide();
        }
        if (0 <= m_currentIndex && m_currentIndex < buttons().length()) {
            buttons().value(m_currentIndex)->setChecked(false);
        }

        if (KWindowSystem::isPlatformWayland()) {
            actionMenu->popup(rootPosition);
        }

        setCurrentIndex(buttonIndex);
        button->setChecked(true);

        // FIXME TODO connect only once
        connect(actionMenu, &QMenu::aboutToHide, this, &AppMenuButtonGroup::onMenuAboutToHide, Qt::UniqueConnection);
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
    if (watched == m_searchLineEdit) {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Down) {
                const auto actions = m_searchMenu->actions();
                if (actions.count() > 2) {
                    m_searchMenu->setFocus();
                    m_searchMenu->setActiveAction(actions.at(2));
                    return true; // Event handled
                }
            }
        }
    }

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
            int desiredIndex = m_currentIndex - 1;
            emit requestActivateIndex(desiredIndex);
            return true;
        } else if (e->key() == Qt::Key_Right) {
            if (menu->activeAction() && menu->activeAction()->menu()) {
                return false;
            }

            int desiredIndex = m_currentIndex + 1;
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
    if (0 <= m_currentIndex && m_currentIndex < buttons().length()) {
        buttons().value(m_currentIndex)->setChecked(false);
    }
    setCurrentIndex(-1);
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

    // Clear previous results, keeping the search bar and separator
    const auto actions = m_searchMenu->actions();
    for (int i = actions.count() - 1; i >= 2; --i) {
        m_searchMenu->removeAction(actions.at(i));
    }

    if (text.length() < 3) {
        if (text.isEmpty()) {
            m_searchLineEdit->setPlaceholderText(i18nd("plasma_applet_org.kde.plasma.appmenu", "Search") + QStringLiteral("…"));
        }
        return;
    }

    if (!m_appMenuModel || !m_menuReadyForSearch) {
        // Menu is not ready yet, search will be re-triggered later
        return;
    }

    QList<QAction *> results;
    for (int row = 0; row < m_appMenuModel->rowCount(); ++row) {
        const QModelIndex menuIndex = m_appMenuModel->index(row, 0);
        QAction *action = (QAction *)m_appMenuModel->data(menuIndex, AppMenuModel::ActionRole).value<void *>();
        if (action) {
            if (action->menu()) {
                searchMenu(action->menu(), text, results);
            } else {
                if (getActionPath(action).path.contains(text, Qt::CaseInsensitive)) {
                    results.append(action);
                }
            }
        }
    }

    for (QAction *action : results) {
        const ActionInfo info = getActionPath(action);
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
}

void AppMenuButtonGroup::onSearchMenuHidden()
{
    if (m_searchButton) {
        m_searchButton->setChecked(false);
    }
    m_searchLineEdit->clear();
    m_searchUiVisible = false;
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

QPoint AppMenuButtonGroup::clampToScreen(QPoint globalPos) const
{
    // This function is no longer used by the search popup, as QMenu handles
    // screen clamping automatically. We leave it for other potential uses.
    QScreen *screen = QGuiApplication::screenAt(globalPos);
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return globalPos; // fallback

    const QRect bounds = screen->availableGeometry();
    int w = 100; // dummy value
    int h = 100; // dummy value

    int minX = bounds.left();
    int maxX = bounds.left() + bounds.width()  - w;
    int minY = bounds.top();
    int maxY = bounds.top()  + bounds.height() - h;

    if (w > bounds.width())  { minX = maxX = bounds.left(); }
    if (h > bounds.height()) { minY = maxY = bounds.top();  }

    globalPos.setX(qBound(minX, globalPos.x(), maxX));
    globalPos.setY(qBound(minY, globalPos.y(), maxY));

    return globalPos;
}

} // namespace Material
