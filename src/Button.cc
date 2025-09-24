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
#include "TextButton.h"

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

namespace {
QPainterPath getButtonPath(bool isLeftmost, bool isRightmost, bool windowIsMaximized, const QRectF &rect, qreal radius) {
    QPainterPath path;

    if (radius > 0 && !windowIsMaximized) {
        if (isLeftmost) {
            path.moveTo(rect.bottomRight());
            path.lineTo(rect.bottomLeft());
            path.lineTo(rect.topLeft() + QPointF(0, radius));
            path.arcTo(QRectF(rect.topLeft(), QSizeF(radius*2, radius*2)), 180, -90);
            path.lineTo(rect.topRight());
            path.lineTo(rect.bottomRight());
            path.closeSubpath();
            return path;
        } else if (isRightmost) {
            path.moveTo(rect.bottomLeft());
            path.lineTo(rect.topLeft());
            path.lineTo(rect.topRight() - QPointF(radius, 0));
            path.arcTo(QRectF(rect.topRight() - QPointF(radius*2, 0), QSizeF(radius*2, radius*2)), 90, -90);
            path.lineTo(rect.bottomRight());
            path.lineTo(rect.bottomLeft());
            path.closeSubpath();
            return path;
        }
    }
    
    path.addRect(rect);
    return path;
}
} // end anonymous namespace

Button::Button(KDecoration3::DecorationButtonType type, Decoration *decoration, QObject *parent)
    : DecorationButton(type, decoration, parent)
    , m_animationEnabled(true)
    , m_animation(new QVariantAnimation(this))
    , m_opacity(1)
    , m_transitionValue(0)
    , m_padding()
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
    // case KDecoration3::DecorationButtonType::ApplicationMenu:
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

    painter->save();

    // Opacity
    painter->setOpacity(m_opacity);

    // Background.
    painter->setPen(Qt::NoPen);
    painter->setBrush(backgroundColor());

    const auto *deco = qobject_cast<Decoration *>(decoration());
    bool do_round = deco && (isHovered() || isPressed()) && deco->cornerRadius() > 0;

    painter->setRenderHint(QPainter::Antialiasing, do_round); //Antialiasing if rounded corner

    if (do_round) {
        qreal radius = deco->cornerRadius();
        QPainterPath path = getButtonPath(m_isLeftmost, m_isRightmost, windowIsMaximized(), geometry(), radius);
        painter->drawPath(path);
    } else {
        painter->drawRect(geometry());
    }

    // Foreground.
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);

    const QRectF contentRect = contentArea();

    // TextButton and AppIconButton are special, they don't have a vector icon, so we don't scale the painter
    if (auto textButton = qobject_cast<TextButton*>(this)) {
        textButton->paintIcon(painter, contentRect, 0);
    } else if (type() == KDecoration3::DecorationButtonType::Menu) {
        AppIconButton::paintIcon(this, painter, contentRect, 0);
    } else {
        // All further rendering is performed inside a 20x20 square, but we want the
        // final icon to be 70% of the height, and our icons are drawn in an 18x18 box.
        const qreal height = contentRect.height();
        const qreal scale = (height * 0.7) / 18.0;
        painter->translate(contentRect.center());
        painter->scale(scale, scale);

        setPenWidth(painter, 1);

        // Icon
        // All icons are drawn inside a 18x18 square, centered in the 20x20 square
        const QRectF iconRect(-9, -9, 18, 18);
        switch (type()) {
        // NOTE: Menu and ApplicationMenu are handled above
        case KDecoration3::DecorationButtonType::OnAllDesktops:
            OnAllDesktopsButton::paintIcon(this, painter, iconRect, 0);
            break;

        case KDecoration3::DecorationButtonType::ContextHelp:
            ContextHelpButton::paintIcon(this, painter, iconRect, 0);
            break;

        case KDecoration3::DecorationButtonType::Shade:
            ShadeButton::paintIcon(this, painter, iconRect, 0);
            break;

        case KDecoration3::DecorationButtonType::KeepAbove:
            KeepAboveButton::paintIcon(this, painter, iconRect, 0);
            break;

        case KDecoration3::DecorationButtonType::KeepBelow:
            KeepBelowButton::paintIcon(this, painter, iconRect, 0);
            break;

        case KDecoration3::DecorationButtonType::Close:
            CloseButton::paintIcon(this, painter, iconRect, 0);
            break;

        case KDecoration3::DecorationButtonType::Maximize:
            MaximizeButton::paintIcon(this, painter, iconRect, 0);
            break;

        case KDecoration3::DecorationButtonType::Minimize:
            MinimizeButton::paintIcon(this, painter, iconRect, 0);
            break;

        default:
            paintIcon(painter, iconRect, 0);
            break;
        }
    }

    painter->restore();
}

void Button::paintIcon(QPainter *painter, const QRectF &iconRect, const qreal)
{
    Q_UNUSED(painter)
    Q_UNUSED(iconRect)
}

void Button::updateSize(int contentWidth, int contentHeight)
{
    const QSize size(
        m_padding.left() + contentWidth + m_padding.right(),
        m_padding.top() + contentHeight + m_padding.bottom()
    );
    setGeometry(QRect(geometry().topLeft().toPoint(), size));
}

void Button::setHeight(int buttonHeight)
{
    // For simplicity, don't count the 1.33:1 scaling in the left/right padding.
    // The left/right padding is mainly for the border offset alignment.
    updateSize(qRound(buttonHeight * 1.33), buttonHeight);
}

qreal Button::iconLineWidth(const qreal height) const
{
    // The painter is scaled by (height * 0.8) / 18.0.
    // We want a final pen width of about 1.01 pixels.
    // So, the pen width in the scaled coordinate system should be 1.01 / scale.
    return PenWidth::Symbol * 18.0 / (height * 0.8);
}

void Button::setPenWidth(QPainter *painter, const qreal scale)
{
    const qreal height = contentArea().height();
    QPen pen(foregroundColor());
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::MiterJoin);
    pen.setWidthF(iconLineWidth(height) * scale);
    painter->setPen(pen);
}

QColor Button::backgroundColor() const
{
    const auto *deco = qobject_cast<Decoration *>(decoration());
    if (!deco) {
        return {};
    }

    if (m_isGtkButton) {
        // Breeze GTK has huge margins around the button. It looks better
        // when we just change the fgColor on hover instead of the bgColor.
        return Qt::transparent;
    }

    //--- CloseButton
    if (type() == KDecoration3::DecorationButtonType::Close) {
        auto *decoratedClient = deco->window();
        const QColor hoveredColor = decoratedClient->color(
            KDecoration3::ColorGroup::Warning,
            KDecoration3::ColorRole::Foreground
        );
        QColor normalColor = QColor(hoveredColor);
        normalColor.setAlphaF(0);

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
    const QColor hoveredColor = KColorUtils::mix(
        deco->titleBarBackgroundColor(),
        deco->titleBarForegroundColor(),
        0.2);
    QColor normalColor = QColor(hoveredColor);
    normalColor.setAlphaF(0);

    if (isPressed()) {
        const QColor pressedColor = KColorUtils::mix(
            deco->titleBarBackgroundColor(),
            deco->titleBarForegroundColor(),
            0.3);
        return KColorUtils::mix(normalColor, pressedColor, m_transitionValue);
    }
    if (isHovered()) {
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
        // Breeze GTK has huge margins around the button. It looks better
        // when we just change the fgColor on hover instead of the bgColor.
        QColor hoveredColor;
        if (m_isGtkButton && type() == KDecoration3::DecorationButtonType::Close) {
            auto *decoratedClient = deco->window();
            hoveredColor = decoratedClient->color(
                KDecoration3::ColorGroup::Warning,
                KDecoration3::ColorRole::Foreground
            );
        } else if (m_isGtkButton && type() == KDecoration3::DecorationButtonType::Maximize) {
            const int grayValue = qGray(deco->titleBarBackgroundColor().rgb());
            if (grayValue < 128) { // Dark Bg
                hoveredColor = QColor(100, 196, 86); // from SierraBreeze
            } else { // Light Bg
                hoveredColor = QColor(40, 200, 64); // from SierraBreeze
            }
        } else if (m_isGtkButton && type() == KDecoration3::DecorationButtonType::Minimize) {
            const int grayValue = qGray(deco->titleBarBackgroundColor().rgb());
            if (grayValue < 128) {
                hoveredColor = QColor(223, 192, 76); // from SierraBreeze
            } else { // Light Bg
                hoveredColor = QColor(255, 188, 48); // from SierraBreeze
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
        m_padding.left(),
        m_padding.top(),
        -m_padding.right(),
        -m_padding.bottom()
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

QMargins &Button::padding()
{
    return m_padding;
}

void Button::setHorzPadding(int value)
{
    padding().setLeft(value);
    padding().setRight(value);
}

void Button::setIsLeftmost(bool isLeftmost)
{
    m_isLeftmost = isLeftmost;
}

void Button::setIsRightmost(bool isRightmost)
{
    m_isRightmost = isRightmost;
}

/*
void Button::setVertPadding(int value)
{
    padding().setTop(value);
    padding().setBottom(value);
}
*/

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

void Button::forceUnpress()
{
    // HACK: There is no public API to set the pressed state of a
    // KDecoration3::DecorationButton. This works around the issue by toggling
    // the enabled state, which has the side effect of resetting the
    // button's internal pressed state.
    const bool wasEnabled = isEnabled();
    setEnabled(!wasEnabled);
    setEnabled(wasEnabled);
}


bool Button::windowIsMaximized() 
{
    const auto *deco = qobject_cast<const Decoration *>(decoration());
    if (deco) {
        const auto *win = deco->window();
        if (win) {
            return win->isMaximized();
        }
    }
    return false;
}


} // namespace Material
