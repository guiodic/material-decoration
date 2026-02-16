/*
 * Copyright (C) 2025 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
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
#include "IconProvider.h"

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
#include <KDecoration3/ScaleHelpers>

// KF
#include <KColorUtils>

// Qt
#include <QCoreApplication>
#include <QDebug>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QSvgRenderer>
#include <QVariantAnimation>
#include <QTimer>
#include <QDBusConnection>
#include <QDBusMessage>

#define UPDATE_GEOM() update(geometry().adjusted(-1, -1, 1, 1))


namespace Material
{

Button::Button(KDecoration3::DecorationButtonType type, Decoration *decoration, QObject *parent)
    : DecorationButton(type, decoration, parent)
    , m_animationEnabled(true)
    , m_animation(new QVariantAnimation(this))
    , m_opacity(1)
    , m_transitionValue(0)
    , m_padding()
    , m_isGtkButton(false)
    , m_holdTimer(new QTimer(this))
{
    m_holdTimer->setSingleShot(true);
    connect(m_holdTimer, &QTimer::timeout, this, &Button::handleHoldTimeout);

    if (QCoreApplication::applicationName() == QStringLiteral("kded6")) {
        // See: https://github.com/Zren/material-decoration/issues/22
        // kde-gtk-config has a kded module which renders the buttons to svgs for gtk.
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
    m_animation->setEasingCurve(QEasingCurve::InOutCubic);
    
    
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setTransitionValue(value.toReal());
    });
    
   
    connect(this, &Button::hoveredChanged, this,
        [this](bool hovered) {
            updateAnimationState(hovered);
            UPDATE_GEOM();
        });

    
    connect(this, &Button::transitionValueChanged, this, [this]() {
        UPDATE_GEOM();
    });
    
    connect(this, &Button::opacityChanged, this, [this]() {
        UPDATE_GEOM();
    });
    
    connect(this, &Button::checkedChanged, this, [this]() {
        UPDATE_GEOM();
    }); 
    
    
    connect(this, &Button::pressedChanged, this, [this]() {
        const auto *deco = qobject_cast<Decoration *>(this->decoration());
        if (deco && deco->m_internalSettings->longPressEnabled() &&
            (this->type() == KDecoration3::DecorationButtonType::Close ||
            this->type() == KDecoration3::DecorationButtonType::Maximize ||
            this->type() == KDecoration3::DecorationButtonType::Minimize)) {
            if (isPressed()) {
                m_longPressTriggered = false;
                m_holdTimer->start(deco->m_internalSettings->longPressDuration());
            } else {
                m_holdTimer->stop();
            }
        }
        UPDATE_GEOM();
    }); 
    
    connect(this, &Button::enabledChanged, this, [this]() {
        UPDATE_GEOM();
    }); 
    
    connect(this, &Button::geometryChanged, this, [this]() {
        UPDATE_GEOM();
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

void Button::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_longPressTriggered) {
        m_longPressTriggered = false;
        forceUnpress();

        // Refresh hover state, simulate a mouse HoverMove.
        const QPointF decoPos = event->position();
        QHoverEvent hoverEvent(QEvent::HoverMove, decoPos, event->globalPosition(), QPointF(-1, -1), event->modifiers());
        QCoreApplication::sendEvent(decoration(), &hoverEvent);

        return;
    }
    KDecoration3::DecorationButton::mouseReleaseEvent(event);
}

void Button::paint(QPainter *painter, const QRectF &repaintRegion)
{
    Q_UNUSED(repaintRegion)
    
    const auto *deco = qobject_cast<Decoration *>(decoration());
       
    if (!deco) {
        return;
    }    
    
    painter->save();

    // Opacity
    painter->setOpacity(m_opacity);

    // Background
    const QColor bgColor = backgroundColor();
    painter->setRenderHints(QPainter::Antialiasing, m_isRightmost || m_isLeftmost);
    painter->setPen(Qt::NoPen);
    painter->setBrush(bgColor);
    const qreal radius = deco->cornerRadius();
    const QRectF snappedGeometry = KDecoration3::snapToPixelGrid(geometry(), deco->window()->scale());

    //const qreal offset = (static_cast<int>(m_isRightmost) - static_cast<int>(m_isLeftmost));   // -0.5 for left; +0.5 for right

    // Smart way to draw a rectangle with the right rounded/squared corner
    painter->drawPath(deco->getRoundedPath(snappedGeometry, radius-Material::cornerRadiusAdjustment, m_isLeftmost && deco->leftBorderVisible(), m_isRightmost && deco->rightBorderVisible(), false, false));
    //painter->fillRect(geometry().toAlignedRect(), bgColor); //.adjusted(-1, -1, 1, 1)

    // Foreground.
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);

    const QRectF contentRect = contentArea();

    // TextButton and AppIconButton are special, so we don't scale the painter
    if (auto textButton = qobject_cast<TextButton*>(this)) {
        textButton->paintIcon(painter, contentRect, 0);
    } else if (type() == KDecoration3::DecorationButtonType::Menu) {
        AppIconButton::paintIcon(this, painter, contentRect, 0);
    } else {
        // All further rendering is performed inside a 18x18 square
        const qreal width = contentRect.width();
        const qreal height = contentRect.height();

        //Calculate scale for button icons
        qreal size;
        if (m_isGtkButton) {
            // See: https://github.com/Zren/material-decoration/issues/22
            // kde-gtk-config has a kded5 module which renders the buttons to svgs for gtk.

            // The svgs are 50x50, located at ~/.config/gtk-3.0/assets/
            // They are usually scaled down to just 18x18 when drawn in gtk headerbars.
            // The Gtk theme already has a fairly large amount of padding, as
            // the Breeze theme doesn't currently follow fitt's law. So use different
            // scale so that the icon is not a very tiny 8px.
            size = qMin(width, height) * 1.15; // 115% for GTK
            painter->setRenderHint(QPainter::Antialiasing, false); //do not antialias gtk buttons, gtk will aliases them
        } else {
            size = qMin(width, height) * 0.6; // 60% of the Kwin Deco
        }

        // For a sharper image, we use integer-based positioning
        const int iconSize = qRound(size);

        // Translate to a rounded center
        const QPointF center = contentRect.center();
        painter->translate(qRound(center.x()), qRound(center.y()));

        // Scale by an integer-based factor
        painter->scale(static_cast<qreal>(iconSize) / 18.0, static_cast<qreal>(iconSize) / 18.0);
        
        setPenWidth(painter, KDecoration3::pixelSize(deco->window()->scale()));

        // Icons
        const QRectF iconRect(-9, -9, 18, 18);

        if (!paintSvgIcon(painter, iconRect)) {
            switch (type()) {
            // NOTE: Menu and ApplicationMenu are handled above
            case KDecoration3::DecorationButtonType::ApplicationMenu:
                ApplicationMenuButton::paintIcon(this, painter, iconRect, 0);
                break;

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
    }

    painter->restore();
}

void Button::paintIcon(QPainter *painter, const QRectF &iconRect, const qreal)
{
    Q_UNUSED(painter)
    Q_UNUSED(iconRect)
}

QString Button::iconName() const
{
    switch (type()) {
    case KDecoration3::DecorationButtonType::Close:
        return QStringLiteral("Close");
    case KDecoration3::DecorationButtonType::Maximize:
        return isChecked() ? QStringLiteral("UnMaximize") : QStringLiteral("Maximize");
    case KDecoration3::DecorationButtonType::Minimize:
        return QStringLiteral("Minimize");
    case KDecoration3::DecorationButtonType::OnAllDesktops:
        return QStringLiteral("OnAllDesktops");
    case KDecoration3::DecorationButtonType::ContextHelp:
        return QStringLiteral("Help");
    case KDecoration3::DecorationButtonType::Shade:
        return QStringLiteral("Shade");
    case KDecoration3::DecorationButtonType::KeepAbove:
        return QStringLiteral("KeepAbove");
    case KDecoration3::DecorationButtonType::KeepBelow:
        return QStringLiteral("KeepBelow");
    case KDecoration3::DecorationButtonType::ApplicationMenu:
        return QStringLiteral("ApplicationMenu");
    default:
        return QString();
    }
}

bool Button::paintSvgIcon(QPainter *painter, const QRectF &iconRect)
{
    const QString baseName = iconName();
    if (baseName.isEmpty()) {
        return false;
    }

    QSharedPointer<QSvgRenderer> renderer = IconProvider::getRenderer(baseName);
    if (!renderer) {
        return false;
    }

    const QColor color = foregroundColor();
    
    // Determine the actual pixel size for the current painter transform.
    // This ensures the icon remains sharp across different scales and DPIs.
    const QRectF deviceRect = painter->transform().mapRect(iconRect);
    const QSize pixelSize = deviceRect.size().toSize();

    if (!m_cachedIcon.isNull() && 
        m_cachedIcon.size() == pixelSize && 
        m_cachedIconColor == color && 
        m_cachedIconName == baseName) {
        
        painter->save();
        painter->resetTransform();
        painter->drawImage(deviceRect.topLeft(), m_cachedIcon);
        painter->restore();
        return true;
    }

    QImage img(pixelSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter imgPainter(&img);
    // Render SVG into the pixel-aligned buffer
    renderer->render(&imgPainter, QRectF(QPointF(0, 0), pixelSize));
    
    // Apply the foreground color using the source-in composition mode
    imgPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    imgPainter.fillRect(img.rect(), color);
    imgPainter.end();

    m_cachedIcon = img;
    m_cachedIconColor = color;
    m_cachedIconName = baseName;

    painter->save();
    painter->resetTransform();
    painter->drawImage(deviceRect.topLeft(), m_cachedIcon);
    painter->restore();
    return true;
}

void Button::updateSize(qreal contentWidth, qreal contentHeight)
{
    const QSizeF size(
        m_padding.left() + contentWidth + m_padding.right(),
        m_padding.top() + contentHeight + m_padding.bottom()
    );
    setGeometry(QRectF(geometry().topLeft(), size));
}

void Button::setHeight(qreal buttonHeight)
{
    // For simplicity, don't count the 1.x:1 scaling in the left/right padding.
    // The left/right padding is mainly for the border offset alignment.
    updateSize(buttonHeight * 1.1, buttonHeight);
}

void Button::setPenWidth(QPainter *painter, const qreal scale)
{
    QPen pen(foregroundColor());
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::MiterJoin);
    // Set the base pen width. This will be correctly scaled by the painter's
    // transformation in the paint() method, preserving the behavior of
    // lines getting thicker as the icon scales up.
    pen.setWidthF(PenWidth::Symbol * scale);
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
            deco->titleBarOpaqueBackgroundColor(),
            deco->titleBarForegroundColor(),
            0.2);

        if (isPressed() || isHovered()) {
            return KColorUtils::mix(
                activeColor,
                deco->titleBarOpaqueBackgroundColor(),
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
        } else if (m_isGtkButton && (type() == KDecoration3::DecorationButtonType::Maximize || type() == KDecoration3::DecorationButtonType::Minimize)) {
            const int grayValue = qGray(deco->titleBarBackgroundColor().rgb());
            if (grayValue < 128) { // Dark Bg
                hoveredColor = KColorUtils::mix(deco->titleBarForegroundColor(), Qt::black, 0.5); 
            } else { // Light Bg
                hoveredColor = KColorUtils::mix(deco->titleBarForegroundColor(), Qt::white, 0.6); 
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
        Q_EMIT animationEnabledChanged();
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
        Q_EMIT animationDurationChanged();
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
        Q_EMIT opacityChanged();
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
        Q_EMIT transitionValueChanged(value);
    }
}

QMarginsF &Button::padding()
{
    return m_padding;
}

void Button::setHorzPadding(qreal value)
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

void Button::handleHoldTimeout()
{
    m_longPressTriggered = true;
    switch (type()) {
    case KDecoration3::DecorationButtonType::Close:
        onCloseHold();
        break;
    case KDecoration3::DecorationButtonType::Maximize:
        onMaximizeHold();
        break;
    case KDecoration3::DecorationButtonType::Minimize:
        onMinimizeHold();
        break;
    default:
        break;
    }
}

void Button::onCloseHold()
{
    qCDebug(category) << "onCloseHold triggered";
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                         QStringLiteral("/component/kwin"),
                                                         QStringLiteral("org.kde.kglobalaccel.Component"),
                                                         QStringLiteral("invokeShortcut"));
    message << QStringLiteral("Minimize to tray");
    QDBusConnection::sessionBus().send(message);
}

void Button::onMinimizeHold()
{
    qCDebug(category) << "onMinimizeHold triggered";
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                         QStringLiteral("/component/kwin"),
                                                         QStringLiteral("org.kde.kglobalaccel.Component"),
                                                         QStringLiteral("invokeShortcut"));
    message << QStringLiteral("minimizeAllOthersActiveScreen");
    QDBusConnection::sessionBus().send(message);
}

void Button::onMaximizeHold()
{
    qCDebug(category) << "onMaximizeHold triggered";
}

} // namespace Material
