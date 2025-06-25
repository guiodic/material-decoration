/*
 * Copyright (C) 2020 Chris Holland <zrenfire@gmail.com>
 * Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>
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
#include "Button.h"
#include "Material.h"
#include "Decoration.h"

#include "AppIconButton.h"
#include "ApplicationMenuButton.h"
#include "OnAllDesktopsButton.h"
#include "ContextHelpButton.h"
#include "ShadeButton.h"
#include "KeepAboveButton.h"
#include "KeepBelowButton.h"
#include "CloseButton.h"
#include "MaximizeButton.h"
#include "MinimizeButton.h"

// KDecoration
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationButton>

// KF
#include <KColorUtils>

// Qt
#include <QDebug>
#include <QMargins>
#include <QPainter>
#include <QVariantAnimation>
#include <QtMath> // qFloor

namespace Material
{

//for rounded corners
static QPainterPath roundedRectSelective(const QRectF &rect, qreal radius, bool topLeft, bool topRight, bool bottomRight, bool bottomLeft)
{
    QPainterPath path;
    path.moveTo(rect.left() + (topLeft ? radius : 0), rect.top());

    // Top edge
    path.lineTo(rect.right() - (topRight ? radius : 0), rect.top());
    if (topRight)
        path.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + radius);

    // Right edge
    path.lineTo(rect.right(), rect.bottom() - (bottomRight ? radius : 0));
    if (bottomRight)
        path.quadTo(rect.right(), rect.bottom(), rect.right() - radius, rect.bottom());

    // Bottom edge
    path.lineTo(rect.left() + (bottomLeft ? radius : 0), rect.bottom());
    if (bottomLeft)
        path.quadTo(rect.left(), rect.bottom(), rect.left(), rect.bottom() - radius);

    // Left edge
    path.lineTo(rect.left(), rect.top() + (topLeft ? radius : 0));
    if (topLeft)
        path.quadTo(rect.left(), rect.top(), rect.left() + radius, rect.top());

    path.closeSubpath();
    return path;
}

Button::Button(KDecoration3::DecorationButtonType type, Decoration *decoration, QObject *parent)
    : DecorationButton(type, decoration, parent)
    , m_animationEnabled(true)
    , m_animation(new QVariantAnimation(this))
    , m_opacity(1)
    , m_transitionValue(0)
    , m_padding(new QMargins())
    , m_isGtkButton(false)
{
    connect(this, &Button::hoveredChanged, this,
        [this](bool hovered) {
            updateAnimationState(hovered);
            update();
        });

    if (QCoreApplication::applicationName() == QStringLiteral("kded6")) {
        // See: https://github.com/Zren/material-decoration/issues/22
        // kde-gtk-config has a kded5 module which renders the buttons to svgs for gtk.
        m_isGtkButton = true;
    }
    this->installEventFilter(this);
    // Animation based on SierraBreezeEnhanced
    // https://github.com/kupiqu/SierraBreezeEnhanced/blob/master/breezebutton.cpp#L45
    // The GTK bridge needs animations disabled to render hover states. See Issue #50.
    // https://invent.kde.org/plasma/kde-gtk-config/-/blob/master/kded/kwin_bridge/dummydecorationbridge.cpp#L35
    m_animationEnabled = !m_isGtkButton && decoration->animationsEnabled();
    m_animation->setDuration(decoration->animationsDuration());
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setTransitionValue(value.toReal());
    });
    connect(this, &Button::transitionValueChanged, this, [this]() {
        update();
    });

    connect(this, &Button::opacityChanged, this, [this]() {
        update();
    });

    setHeight(decoration->titleBarHeight());

    auto *decoratedClient = decoration->window();

    switch (type) {
    case KDecoration3::DecorationButtonType::Menu:
        AppIconButton::init(this, decoratedClient);
        break;

    case KDecoration3::DecorationButtonType::ApplicationMenu:
        ApplicationMenuButton::init(this, decoratedClient);
        break;

    case KDecoration3::DecorationButtonType::OnAllDesktops:
        OnAllDesktopsButton::init(this, decoratedClient);
        break;

    case KDecoration3::DecorationButtonType::ContextHelp:
        ContextHelpButton::init(this, decoratedClient);
        break;

    case KDecoration3::DecorationButtonType::Shade:
        ShadeButton::init(this, decoratedClient);
        break;

    case KDecoration3::DecorationButtonType::KeepAbove:
        KeepAboveButton::init(this, decoratedClient);
        break;

    case KDecoration3::DecorationButtonType::KeepBelow:
        KeepBelowButton::init(this, decoratedClient);
        break;

    case KDecoration3::DecorationButtonType::Close:
        CloseButton::init(this, decoratedClient);
        break;

    case KDecoration3::DecorationButtonType::Maximize:
        MaximizeButton::init(this, decoratedClient);
        break;

    case KDecoration3::DecorationButtonType::Minimize:
        MinimizeButton::init(this, decoratedClient);
        break;

    default:
        break;
    }
}

Button::~Button()
{
}

KDecoration3::DecorationButton* Button::create(KDecoration3::DecorationButtonType type, KDecoration3::Decoration *decoration, QObject *parent)
{
    auto deco = qobject_cast<Decoration*>(decoration);
    if (!deco) {
        return nullptr;
    }

    switch (type) {
    case KDecoration3::DecorationButtonType::Menu:
    case KDecoration3::DecorationButtonType::OnAllDesktops:
    case KDecoration3::DecorationButtonType::ContextHelp:
    case KDecoration3::DecorationButtonType::Shade:
    case KDecoration3::DecorationButtonType::KeepAbove:
    case KDecoration3::DecorationButtonType::KeepBelow:
    case KDecoration3::DecorationButtonType::Close:
    case KDecoration3::DecorationButtonType::Maximize:
    case KDecoration3::DecorationButtonType::Minimize:
        return new Button(type, deco, parent);

    default:
        return nullptr;
    }
}

Button::Button(QObject *parent, const QVariantList &args)
    : Button(args.at(0).value<KDecoration3::DecorationButtonType>(), args.at(1).value<Decoration*>(), parent)
{
}

void Button::paint(QPainter *painter, const QRectF &repaintRegion)
{
    Q_UNUSED(repaintRegion)
    // qCDebug(category) << "BUTTON geometry:" << geometry();
    // Buttons are coded assuming 24 units in size.
    const QRectF buttonRect = geometry();
    const QRectF contentRect = contentArea();

    const qreal iconScale = contentRect.height() / 24;
    int iconSize;
    if (m_isGtkButton) {
        iconSize = qRound(iconScale * 17);
    } else {
        iconSize = qRound(iconScale * 10);
    }
    QRectF iconRect = QRectF(0, 0, iconSize, iconSize);
    iconRect.moveCenter(contentRect.center().toPoint());

    const qreal gridUnit = iconRect.height() / 10;

    painter->save();
    painter->setRenderHints(QPainter::Antialiasing, true);
    painter->setOpacity(m_opacity);

    // Calcolo angoli arrotondati
    const Decoration *deco = qobject_cast<const Decoration *>(decoration());
    auto *buttonGroup = qobject_cast<KDecoration3::DecorationButtonGroup *>(parent());
    bool roundTopLeft = false;
    bool roundTopRight = false;
    if (deco && buttonGroup) {
        auto buttons = buttonGroup->buttons();
        int idx = buttons.indexOf(this);
        bool isMaximized = deco->window() && deco->window()->isMaximized();
        if (!isMaximized) {
            if (buttonGroup == deco->leftButtons() && idx == 0) {
                roundTopLeft = true;
            }
            if (buttonGroup == deco->rightButtons() && idx == buttons.count() - 1) {
                roundTopRight = true;
            }
        }
    }

    //Solo path arrotondato. Niente drawRect: gli angoli restano arrotondati.
    const qreal radius = Material::kDecorationRadius;
    QColor btnBg = backgroundColor();
    btnBg.setAlpha(255);
    
    QRectF bigRect = buttonRect.adjusted(0, -0.5, 0.5, 0.5);
    QPainterPath path = roundedRectSelective(bigRect, radius + 0.5, roundTopLeft, roundTopRight, false, false);
    painter->setPen(Qt::NoPen);
    painter->setBrush(btnBg);
    painter->drawPath(path);
    // cleaning
    //painter->setPen(Qt::NoPen);
    //painter->setBrush(backgroundColor()); // o colore della titlebar
    //painter->drawRect(QRectF(buttonRect.left(), buttonRect.top(), 1, buttonRect.height()));

    // Foreground e icona
    setPenWidth(painter, gridUnit, 1);
    painter->setBrush(Qt::NoBrush);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    switch (type()) {
    case KDecoration3::DecorationButtonType::Menu:
        AppIconButton::paintIcon(this, painter, iconRect, gridUnit);
        break;
    case KDecoration3::DecorationButtonType::ApplicationMenu:
        ApplicationMenuButton::paintIcon(this, painter, iconRect, gridUnit);
        break;
    case KDecoration3::DecorationButtonType::OnAllDesktops:
        OnAllDesktopsButton::paintIcon(this, painter, iconRect, gridUnit);
        break;
    case KDecoration3::DecorationButtonType::ContextHelp:
        ContextHelpButton::paintIcon(this, painter, iconRect, gridUnit);
        break;
    case KDecoration3::DecorationButtonType::Shade:
        ShadeButton::paintIcon(this, painter, iconRect, gridUnit);
        break;
    case KDecoration3::DecorationButtonType::KeepAbove:
        KeepAboveButton::paintIcon(this, painter, iconRect, gridUnit);
        break;
    case KDecoration3::DecorationButtonType::KeepBelow:
        KeepBelowButton::paintIcon(this, painter, iconRect, gridUnit);
        break;
    case KDecoration3::DecorationButtonType::Close:
        CloseButton::paintIcon(this, painter, iconRect, gridUnit);
        break;
    case KDecoration3::DecorationButtonType::Maximize:
        MaximizeButton::paintIcon(this, painter, iconRect, gridUnit);
        break;
    case KDecoration3::DecorationButtonType::Minimize:
        MinimizeButton::paintIcon(this, painter, iconRect, gridUnit);
        break;
    default:
        paintIcon(painter, iconRect, gridUnit);
        break;
    }
    
    painter->restore();
}


void Button::paintIcon(QPainter *painter, const QRectF &iconRect, const qreal gridUnit)
{
    Q_UNUSED(painter)
    Q_UNUSED(iconRect)
    Q_UNUSED(gridUnit)
}

void Button::updateSize(int contentWidth, int contentHeight)
{
    const QSize size(
        m_padding->left() + contentWidth + m_padding->right(),
        m_padding->top() + contentHeight + m_padding->bottom()
    );
    setGeometry(QRect(geometry().topLeft().toPoint(), size));
}

void Button::setHeight(int buttonHeight)
{
    // For simplicity, don't count the 1.33:1 scaling in the left/right padding.
    // The left/right padding is mainly for the border offset alignment.
    updateSize(qRound(buttonHeight * 1.33), buttonHeight);
}

qreal Button::iconLineWidth(const qreal gridUnit) const
{
    return PenWidth::Symbol * qMax(1.0, gridUnit);
}

void Button::setPenWidth(QPainter *painter, const qreal gridUnit, const qreal scale)
{
    QPen pen(foregroundColor());
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::MiterJoin);
    pen.setWidthF(iconLineWidth(gridUnit) * scale);
    painter->setPen(pen);
}

QColor Button::backgroundColor() const
{
    const auto *deco = qobject_cast<Decoration *>(decoration());
    if (!deco) {
        return {};
    }

    if (m_isGtkButton) {
        return deco->titleBarBackgroundColor();
    }

    //--- CloseButton
    if (type() == KDecoration3::DecorationButtonType::Close) {
        auto *decoratedClient = deco->window();
        const QColor hoveredColor = decoratedClient->color(
            KDecoration3::ColorGroup::Warning,
            KDecoration3::ColorRole::Foreground
        );
        QColor normalColor = deco->titleBarBackgroundColor(); // <-- Cambiato qui!
        if (isPressed()) {
            const QColor pressedColor = decoratedClient->color(
                KDecoration3::ColorGroup::Warning,
                KDecoration3::ColorRole::Foreground
            ).lighter();
            return KColorUtils::mix(normalColor, pressedColor, m_transitionValue);
        }
        if (isHovered()) {
            return KColorUtils::mix(normalColor, hoveredColor, m_transitionValue);
        }
        return normalColor;
    }

    //--- Checked
    if (isChecked() && type() != KDecoration3::DecorationButtonType::Maximize) {
        const QColor normalColor = deco->titleBarForegroundColor();
        if (isPressed()) {
            const QColor pressedColor = KColorUtils::mix(
                deco->titleBarBackgroundColor(),
                deco->titleBarForegroundColor(),
                0.7);
            return KColorUtils::mix(normalColor, pressedColor, m_transitionValue);
        }
        if (isHovered()) {
            const QColor hoveredColor = KColorUtils::mix(
                deco->titleBarBackgroundColor(),
                deco->titleBarForegroundColor(),
                0.8);
            return KColorUtils::mix(normalColor, hoveredColor, m_transitionValue);
        }
        return normalColor;
    }

    //--- Normal
    QColor normalColor = deco->titleBarBackgroundColor(); // <-- Cambiato qui!
    if (isPressed()) {
        const QColor pressedColor = KColorUtils::mix(
            deco->titleBarBackgroundColor(),
            deco->titleBarForegroundColor(),
            0.3);
        return KColorUtils::mix(normalColor, pressedColor, m_transitionValue);
    }
    if (isHovered()) {
        const QColor hoveredColor = KColorUtils::mix(
            deco->titleBarBackgroundColor(),
            deco->titleBarForegroundColor(),
            0.2);
        return KColorUtils::mix(normalColor, hoveredColor, m_transitionValue);
    }
    return normalColor;
}

QColor Button::foregroundColor() const
{
    const auto *deco = qobject_cast<Decoration *>(decoration());
    if (!deco) {
        return {};
    }

    //--- Checked
    if (isChecked() && type() != KDecoration3::DecorationButtonType::Maximize) {
        const QColor activeColor = KColorUtils::mix(
            deco->titleBarBackgroundColor(),
            deco->titleBarForegroundColor(),
            0.2);

        if (isPressed() || isHovered()) {
            return KColorUtils::mix(
                activeColor,
                deco->titleBarBackgroundColor(),
                m_transitionValue);
        }
        return activeColor;
    }

    //--- Normal
    const QColor normalColor = KColorUtils::mix(
        deco->titleBarBackgroundColor(),
        deco->titleBarForegroundColor(),
        0.8);

    if (isPressed() || isHovered()) {
        QColor hoveredColor;
        if (m_isGtkButton && type() == KDecoration3::DecorationButtonType::Close) {
            auto *decoratedClient = deco->window();
            hoveredColor = decoratedClient->color(
                KDecoration3::ColorGroup::Warning,
                KDecoration3::ColorRole::Foreground
            );
        } else if (m_isGtkButton && type() == KDecoration3::DecorationButtonType::Maximize) {
            const int grayValue = qGray(deco->titleBarBackgroundColor().rgb());
            if (grayValue < 128) {
                hoveredColor = QColor(100, 196, 86); // Dark Bg
            } else {
                hoveredColor = QColor(40, 200, 64); // Light Bg
            }
        } else if (m_isGtkButton && type() == KDecoration3::DecorationButtonType::Minimize) {
            const int grayValue = qGray(deco->titleBarBackgroundColor().rgb());
            if (grayValue < 128) {
                hoveredColor = QColor(223, 192, 76); // Dark Bg
            } else {
                hoveredColor = QColor(255, 188, 48); // Light Bg
            }
        } else {
            hoveredColor = deco->titleBarForegroundColor();
        }

        return KColorUtils::mix(
            normalColor,
            hoveredColor,
            m_transitionValue);
    }

    return normalColor;
}

QRectF Button::contentArea() const
{
    return geometry().adjusted(
        m_padding->left(),
        m_padding->top(),
        -m_padding->right(),
        -m_padding->bottom()
    );
}

bool Button::animationEnabled() const
{
    return m_animationEnabled;
}

void Button::setAnimationEnabled(bool value)
{
    if (m_animationEnabled != value) {
        m_animationEnabled = value;
        emit animationEnabledChanged();
    }
}

int Button::animationDuration() const
{
    return m_animation->duration();
}

void Button::setAnimationDuration(int value)
{
    if (m_animation->duration() != value) {
        m_animation->setDuration(value);
        emit animationDurationChanged();
    }
}

qreal Button::opacity() const
{
    return m_opacity;
}

void Button::setOpacity(qreal value)
{
    if (m_opacity != value) {
        m_opacity = value;
        emit opacityChanged();
    }
}

qreal Button::transitionValue() const
{
    return m_transitionValue;
}

void Button::setTransitionValue(qreal value)
{
    if (m_transitionValue != value) {
        m_transitionValue = value;
        emit transitionValueChanged(value);
    }
}

QMargins* Button::padding()
{
    return m_padding;
}

void Button::setHorzPadding(int value)
{
    padding()->setLeft(value);
    padding()->setRight(value);
}

void Button::setVertPadding(int value)
{
    padding()->setTop(value);
    padding()->setBottom(value);
}

void Button::updateAnimationState(bool hovered)
{
    if (m_animationEnabled) {
        QAbstractAnimation::Direction dir = hovered ? QAbstractAnimation::Forward : QAbstractAnimation::Backward;
        if (m_animation->state() == QAbstractAnimation::Running && m_animation->direction() != dir) {
            m_animation->stop();
        }
        m_animation->setDirection(dir);
        if (m_animation->state() != QAbstractAnimation::Running) {
            m_animation->start();
        }
    } else {
        setTransitionValue(1);
    }
}

/* bool Material::Button::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Leave) {
        setTransitionValue(0);
        update();
    }
    return KDecoration3::DecorationButton::eventFilter(watched, event);
} */

} // namespace Material
