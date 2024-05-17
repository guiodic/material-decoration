/*
 * Copyright (C) 2020 Chris Holland <zrenfire@gmail.com>
 * Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>
 * Copyright (C) 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 * Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>
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
#include "Decoration.h"
#include "Material.h"
#include "BuildConfig.h"
#include "AppMenuButtonGroup.h"
#include "BoxShadowHelper.h"
#include "Button.h"
#include "InternalSettings.h"

// KDecoration
#include <KDecoration2/DecoratedClient>
#include <KDecoration2/DecorationButton>
#include <KDecoration2/DecorationButtonGroup>
#include <KDecoration2/DecorationSettings>
#include <KDecoration2/DecorationShadow>

// KF
#include <KWindowSystem>

// Qt
#include <QApplication>
#include <QDebug>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRegion>
#include <QSharedPointer>
#include <QWheelEvent>

// X11
#if HAVE_X11
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <private/qtx11extras_p.h>
#else
#include <QX11Info>
#endif
#include <xcb/xcb.h>
#endif

namespace Material
{

namespace
{

struct ShadowParams
{
    ShadowParams() = default;

    ShadowParams(const QPoint &offset, int radius, qreal opacity)
        : offset(offset)
        , radius(radius)
        , opacity(opacity) {}

    QPoint offset;
    int radius = 0;
    qreal opacity = 0;
};

struct CompositeShadowParams
{
    CompositeShadowParams() = default;

    CompositeShadowParams(
            const QPoint &offset,
            const ShadowParams &shadow1,
            const ShadowParams &shadow2)
        : offset(offset)
        , shadow1(shadow1)
        , shadow2(shadow2) {}

    bool isNone() const {
        return qMax(shadow1.radius, shadow2.radius) == 0;
    }

    QPoint offset;
    ShadowParams shadow1;
    ShadowParams shadow2;
};

// const CompositeShadowParams s_shadowParams = CompositeShadowParams(
//     QPoint(0, 18),
//     ShadowParams(QPoint(0, 0), 64, 0.8),
//     ShadowParams(QPoint(0, -10), 24, 0.1)
// );
const CompositeShadowParams s_shadowParams[] = {
    // None
    CompositeShadowParams(),
    // Small
    CompositeShadowParams(
        QPoint(0, 4),
        ShadowParams(QPoint(0, 0), 16, 1),
        ShadowParams(QPoint(0, -2), 8, 0.4)),
    // Medium
    CompositeShadowParams(
        QPoint(0, 8),
        ShadowParams(QPoint(0, 0), 32, 0.9),
        ShadowParams(QPoint(0, -4), 16, 0.3)),
    // Large
    CompositeShadowParams(
        QPoint(0, 12),
        ShadowParams(QPoint(0, 0), 48, 0.8),
        ShadowParams(QPoint(0, -6), 24, 0.2)),
    // Very large
    CompositeShadowParams(
        QPoint(0, 16),
        ShadowParams(QPoint(0, 0), 64, 0.7),
        ShadowParams(QPoint(0, -8), 32, 0.1)),
};

inline CompositeShadowParams lookupShadowParams(int size)
{
    switch (size) {
    case InternalSettings::ShadowNone:
        return s_shadowParams[0];
    case InternalSettings::ShadowSmall:
        return s_shadowParams[1];
    case InternalSettings::ShadowMedium:
        return s_shadowParams[2];
    default:
    case InternalSettings::ShadowLarge:
        return s_shadowParams[3];
    case InternalSettings::ShadowVeryLarge:
        return s_shadowParams[4];
    }
}

} // anonymous namespace

static int s_decoCount = 0;
static int s_shadowSizePreset = InternalSettings::ShadowVeryLarge;
static int s_shadowStrength = 255;
static QColor s_shadowColor = QColor(33, 33, 33);
static std::shared_ptr<KDecoration2::DecorationShadow> s_cachedShadow;

Decoration::Decoration(QObject *parent, const QVariantList &args)
    : KDecoration2::Decoration(parent, args)
    , m_internalSettings(nullptr)
{
    ++s_decoCount;
}

Decoration::~Decoration()
{
    if (--s_decoCount == 0) {
        s_cachedShadow.reset();
    }
}

QRect Decoration::titleBarRect() const
{
    return QRect(0, 0, size().width(), titleBarHeight());
}

QRect Decoration::centerRect() const
{
    const bool leftButtonsVisible = !m_leftButtons->buttons().isEmpty();
    const int leftOffset = m_leftButtons->geometry().right()
        + (leftButtonsVisible ? settings()->smallSpacing() : 0);

    const bool rightButtonsVisible = !m_rightButtons->buttons().isEmpty();
    const int rightOffset = m_rightButtons->geometry().width()
        + (rightButtonsVisible ? settings()->smallSpacing() : 0);

    return titleBarRect().adjusted(
        leftOffset,
        0,
        -rightOffset,
        0
    );
}

void Decoration::paint(QPainter *painter, const QRect &repaintRegion)
{
    auto *decoratedClient = client();

    if (!decoratedClient->isShaded()) {
        paintFrameBackground(painter, repaintRegion);
    }

    paintTitleBarBackground(painter, repaintRegion);
    paintButtons(painter, repaintRegion);
    paintCaption(painter, repaintRegion);

    // Don't paint outline for NoBorder, NoSideBorder, or Tiny borders.
    if (settings()->borderSize() >= KDecoration2::BorderSize::Normal) {
        paintOutline(painter, repaintRegion);
    }
    updateBlur();
}

bool Decoration::init()
{
    m_internalSettings = QSharedPointer<InternalSettings>(new InternalSettings());

    auto *decoratedClient = client();

    auto repaintTitleBar = [this] {
        update(titleBar());
    };

    m_leftButtons = new KDecoration2::DecorationButtonGroup(
        KDecoration2::DecorationButtonGroup::Position::Left,
        this,
        &Button::create);

    m_rightButtons = new KDecoration2::DecorationButtonGroup(
        KDecoration2::DecorationButtonGroup::Position::Right,
        this,
        &Button::create);

    m_menuButtons = new AppMenuButtonGroup(this);
    connect(m_menuButtons, &AppMenuButtonGroup::menuUpdated,
            this, &Decoration::updateButtonsGeometry);
    connect(m_menuButtons, &AppMenuButtonGroup::opacityChanged,
            this, repaintTitleBar);
    connect(m_menuButtons, &AppMenuButtonGroup::alwaysShowChanged,
            this, repaintTitleBar);
    m_menuButtons->updateAppMenuModel();


    connect(decoratedClient, &KDecoration2::DecoratedClient::widthChanged,
            this, &Decoration::updateTitleBar);
    connect(decoratedClient, &KDecoration2::DecoratedClient::widthChanged,
            this, &Decoration::updateButtonsGeometry);
    connect(decoratedClient, &KDecoration2::DecoratedClient::maximizedChanged,
            this, &Decoration::updateButtonsGeometry);

    connect(decoratedClient, &KDecoration2::DecoratedClient::adjacentScreenEdgesChanged,
            this, &Decoration::updateBorders);
    connect(decoratedClient, &KDecoration2::DecoratedClient::maximizedHorizontallyChanged,
            this, &Decoration::updateBorders);
    connect(decoratedClient, &KDecoration2::DecoratedClient::maximizedVerticallyChanged,
            this, &Decoration::updateBorders);
    connect(decoratedClient, &KDecoration2::DecoratedClient::shadedChanged,
            this, &Decoration::updateBorders);

    connect(decoratedClient, &KDecoration2::DecoratedClient::captionChanged,
            this, repaintTitleBar);
    connect(decoratedClient, &KDecoration2::DecoratedClient::activeChanged,
            this, repaintTitleBar);

    updateBorders();
    updateResizeBorders();
    updateTitleBar();
    updateButtonsGeometry();

    connect(this, &KDecoration2::Decoration::sectionUnderMouseChanged,
            this, &Decoration::onSectionUnderMouseChanged);
    updateTitleBarHoverState();

    // For some reason, the shadow should be installed the last. Otherwise,
    // the Window Decorations KCM crashes.
    updateShadow();

    connect(settings().get(), &KDecoration2::DecorationSettings::reconfigured,
        this, &Decoration::reconfigure);
    connect(m_internalSettings.data(), &InternalSettings::configChanged,
        this, &Decoration::reconfigure);

    // Window Decoration KCM
    // The reconfigure signal will update active windows, but we need to hook
    // individual signals for the preview in the KCM.
    connect(settings().get(), &KDecoration2::DecorationSettings::borderSizeChanged,
        this, &Decoration::updateBorders);
    connect(settings().get(), &KDecoration2::DecorationSettings::fontChanged,
        this, &Decoration::updateBorders);
    connect(settings().get(), &KDecoration2::DecorationSettings::spacingChanged,
        this, &Decoration::updateBorders);
  return true;
}

void Decoration::reconfigure()
{
    m_internalSettings->load();

    updateBorders();
    updateTitleBar();
    m_menuButtons->setAlwaysShow(m_internalSettings->menuAlwaysShow());
    updateButtonsGeometry();
    updateButtonAnimation();
    updateShadow();
    update();
}

void Decoration::mousePressEvent(QMouseEvent *event)
{
    KDecoration2::Decoration::mousePressEvent(event);
    // qCDebug(category) << "Decoration::mousePressEvent" << event;

    if (m_menuButtons->geometry().contains(event->pos())) {
        if (event->button() == Qt::LeftButton) {
            initDragMove(event->pos());
            event->setAccepted(false);

        // If AppMenuButton's do not handle the button
        } else if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
            // Don't accept the event. KDecoration2 will
            // accept the event even if it doesn't pass
            // button->acceptableButtons()->testFlag(button)
            event->setAccepted(false);
        }
    }
}

void Decoration::hoverEnterEvent(QHoverEvent *event)
{
    KDecoration2::Decoration::hoverEnterEvent(event);
    qCDebug(category) << "Decoration::hoverEnterEvent" << event;
    updateBlur();
    // m_menuButtons->setHovered(true);
}

void Decoration::hoverMoveEvent(QHoverEvent *event)
{
    KDecoration2::Decoration::hoverMoveEvent(event);
    // qCDebug(category) << "Decoration::hoverMoveEvent" << event;

    const bool dragStarted = dragMoveTick(event->pos());
    // qCDebug(category) << "    " << "dragStarted" << dragStarted;
    if (dragStarted) {
        m_menuButtons->unPressAllButtons();
    }

    // const bool wasHovered = m_menuButtons->hovered();
    // const bool contains = m_menuButtons->geometry().contains(event->posF());
    // if (!wasHovered && contains) {
    //     // HoverEnter
    //     m_menuButtons->setHovered(true);
    // } else if (wasHovered && !contains) {
    //     // HoverLeave
    //     m_menuButtons->setHovered(false);
    // } else if (wasHovered && contains) {
    //     // HoverMove
    // }
    updateBlur();
}

void Decoration::mouseReleaseEvent(QMouseEvent *event)
{
    KDecoration2::Decoration::mouseReleaseEvent(event);
    // qCDebug(category) << "Decoration::mouseReleaseEvent" << event;

    resetDragMove();
    updateBlur();
}

void Decoration::hoverLeaveEvent(QHoverEvent *event)
{
    KDecoration2::Decoration::hoverLeaveEvent(event);
    qCDebug(category) << "Decoration::hoverLeaveEvent" << event;

    resetDragMove();
    updateBlur();
    // m_menuButtons->setHovered(false);
}

void Decoration::wheelEvent(QWheelEvent *event)
{
    #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QPointF pos = event->position();
    #else
    const QPointF pos = event->posF();
    #endif

    if (m_menuButtons->geometry().contains(pos)) {
        // Skip
    } else {
        KDecoration2::Decoration::wheelEvent(event);
    }
}

void Decoration::onSectionUnderMouseChanged(const Qt::WindowFrameSection value)
{
    Q_UNUSED(value);
    // qCDebug(category) << "onSectionUnderMouseChanged" << value;
    updateTitleBarHoverState();
}

void Decoration::updateBlur()
{
#if HAVE_KDecoration2_5_25
    setBlurRegion(QRegion(0, 0, size().width(), size().height()));
#endif
}

void Decoration::updateBorders()
{
    const int sideSize = sideBorderSize();
    QMargins borders;
    borders.setTop(titleBarHeight());
    borders.setLeft(leftBorderVisible() ? sideSize : 0);
    borders.setRight(rightBorderVisible() ? sideSize : 0);
    borders.setBottom(bottomBorderVisible() ? bottomBorderSize() : 0);
    setBorders(borders);
}

void Decoration::updateResizeBorders()
{
    QMargins borders;

    const int extender = settings()->largeSpacing();
    borders.setLeft(extender);
    borders.setTop(extender);
    borders.setRight(extender);
    borders.setBottom(extender);

    setResizeOnlyBorders(borders);
}

void Decoration::updateTitleBar()
{
    setTitleBar(titleBarRect());
}

void Decoration::updateTitleBarHoverState()
{
    const bool wasHovered = m_menuButtons->hovered();
    const bool isHovered = titleBarIsHovered();
    if (!wasHovered && isHovered) {
        // HoverEnter
        m_menuButtons->setHovered(true);
    } else if (wasHovered && !isHovered) {
        // HoverLeave
        m_menuButtons->setHovered(false);
    } else if (wasHovered && isHovered) {
        // HoverMove
    }
}

void Decoration::setButtonGroupHeight(KDecoration2::DecorationButtonGroup *buttonGroup, int buttonHeight)
{
    // int vertPadding = buttonPadding();
    for (int i = 0; i < buttonGroup->buttons().length(); i++) {
        KDecoration2::DecorationButton* decoButton = buttonGroup->buttons().value(i);
        auto *button = qobject_cast<Button *>(decoButton);
        if (button) {
            button->setHeight(buttonHeight);
            // button->setVertPadding(vertPadding);
        }
    }
}

void Decoration::setButtonGroupHorzPadding(KDecoration2::DecorationButtonGroup *buttonGroup, int value)
{
    for (int i = 0; i < buttonGroup->buttons().length(); i++) {
        KDecoration2::DecorationButton* decoButton = buttonGroup->buttons().value(i);
        auto *button = qobject_cast<Button *>(decoButton);
        if (button) {
            button->setHorzPadding(value);
        }
    }
}

void Decoration::setButtonGroupVertPadding(KDecoration2::DecorationButtonGroup *buttonGroup, int value)
{
    for (int i = 0; i < buttonGroup->buttons().length(); i++) {
        KDecoration2::DecorationButton* decoButton = buttonGroup->buttons().value(i);
        auto *button = qobject_cast<Button *>(decoButton);
        if (button) {
            button->setVertPadding(value);
        }
    }
}

void Decoration::updateButtonHeight()
{
    const int buttonHeight = titleBarHeight();
    setButtonGroupHeight(m_leftButtons, buttonHeight);
    setButtonGroupHeight(m_rightButtons, buttonHeight);
    setButtonGroupHeight(m_menuButtons, buttonHeight);
}

void Decoration::updateButtonsGeometry()
{
    const int sideSize = sideBorderSize();
    const int leftOffset = leftBorderVisible() ? sideSize : 0;
    const int rightOffset = rightBorderVisible() ? sideSize : 0;

    updateButtonHeight();

    // Left
    m_leftButtons->setPos(QPointF(leftOffset, 0));
    m_leftButtons->setSpacing(0);
    // if (!m_leftButtons->buttons().isEmpty()) {
    //     auto *firstButon = qobject_cast<Button *>(m_leftButtons->buttons().front());
    //     firstButon->padding()->setLeft(leftOffset);
    // }

    // Right
    m_rightButtons->setPos(QPointF(
        size().width() - rightOffset - m_rightButtons->geometry().width(),
        0
    ));
    m_rightButtons->setSpacing(0);
    // if (!m_rightButtons->buttons().isEmpty()) {
    //     auto *lastButton = qobject_cast<Button *>(m_rightButtons->buttons().last());
    //     lastButton->padding()->setRight(rightOffset);
    // }

    // Menu
    if (!m_menuButtons->buttons().isEmpty()) {
        const int captionOffset = captionMinWidth() + settings()->smallSpacing();
        const QRect availableRect = centerRect().adjusted(
            0,
            0,
            -captionOffset,
            0
        );
        setButtonGroupHorzPadding(m_menuButtons, m_internalSettings->menuButtonHorzPadding());
        m_menuButtons->setPos(availableRect.topLeft());
        m_menuButtons->setSpacing(0);
        m_menuButtons->updateOverflow(availableRect);
    }

    update();
}

void Decoration::setButtonGroupAnimation(KDecoration2::DecorationButtonGroup *buttonGroup, bool enabled, int duration)
{
    for (int i = 0; i < buttonGroup->buttons().length(); i++) {
        auto *button = qobject_cast<Button *>(buttonGroup->buttons().value(i));
        button->setAnimationEnabled(enabled);
        button->setAnimationDuration(duration);
    }
}

void Decoration::updateButtonAnimation()
{
    const bool enabled = animationsEnabled();
    const int duration = animationsDuration();
    setButtonGroupAnimation(m_leftButtons, enabled, duration);
    setButtonGroupAnimation(m_rightButtons, enabled, duration);
    setButtonGroupAnimation(m_menuButtons, enabled, duration);

    // Hover Animation
    m_menuButtons->setAnimationEnabled(enabled);
    m_menuButtons->setAnimationDuration(duration);
}

void Decoration::updateShadow()
{
    const QColor shadowColor = m_internalSettings->shadowColor();
    const int shadowStrengthInt = m_internalSettings->shadowStrength();
    const int shadowSizePreset = m_internalSettings->shadowSize();

    if (s_cachedShadow
        && s_shadowColor == shadowColor
        && s_shadowSizePreset == shadowSizePreset
        && s_shadowStrength == shadowStrengthInt
    ) {
        setShadow(s_cachedShadow);
        return;
    }

    s_shadowColor = shadowColor;
    s_shadowStrength = shadowStrengthInt;
    s_shadowSizePreset = shadowSizePreset;

    auto withOpacity = [] (const QColor &color, qreal opacity) -> QColor {
        QColor c(color);
        c.setAlphaF(opacity);
        return c;
    };

    const qreal shadowStrength = static_cast<qreal>(shadowStrengthInt) / 255.0;
    const CompositeShadowParams params = lookupShadowParams(shadowSizePreset);

    if (params.isNone()) { // InternalSettings::ShadowNone
        s_cachedShadow.reset();
        setShadow(s_cachedShadow);
        return;
    }

    // In order to properly render a box shadow with a given radius `shadowSize`,
    // the box size should be at least `2 * QSize(shadowSize, shadowSize)`.
    const int shadowSize = qMax(params.shadow1.radius, params.shadow2.radius);
    const QSize boxSize = QSize(1, 1) + QSize(shadowSize*2, shadowSize*2);
    const QRect box(QPoint(shadowSize, shadowSize), boxSize);
    const QRect rect = box.adjusted(-shadowSize, -shadowSize, shadowSize, shadowSize);

    QImage shadowTexture(rect.size(), QImage::Format_ARGB32_Premultiplied);
    shadowTexture.fill(Qt::transparent);

    QPainter painter(&shadowTexture);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw the "shape" shadow.
    BoxShadowHelper::boxShadow(
        &painter,
        box,
        params.shadow1.offset,
        params.shadow1.radius,
        withOpacity(shadowColor, params.shadow1.opacity * shadowStrength));

    // Draw the "contrast" shadow.
    BoxShadowHelper::boxShadow(
        &painter,
        box,
        params.shadow2.offset,
        params.shadow2.radius,
        withOpacity(shadowColor, params.shadow2.opacity * shadowStrength));

    // Mask out inner rect.
    const QMargins padding = QMargins(
        shadowSize - params.offset.x(),
        shadowSize - params.offset.y(),
        shadowSize + params.offset.x(),
        shadowSize + params.offset.y());
    const QRect innerRect = rect - padding;

    // Mask out window+titlebar from shadow
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.drawRect(innerRect);

    painter.end();

    s_cachedShadow = std::make_shared<KDecoration2::DecorationShadow>();
    s_cachedShadow->setPadding(padding);
    s_cachedShadow->setInnerShadowRect(QRect(shadowTexture.rect().center(), QSize(1, 1)));
    s_cachedShadow->setShadow(shadowTexture);

    setShadow(s_cachedShadow);
}

bool Decoration::menuAlwaysShow() const
{
    return m_internalSettings->menuAlwaysShow();
}

bool Decoration::animationsEnabled() const
{
    return m_internalSettings->animationsEnabled();
}

int Decoration::animationsDuration() const
{
    return m_internalSettings->animationsDuration();
}

int Decoration::buttonPadding() const
{
    const int baseUnit = settings()->gridUnit();
    switch (m_internalSettings->buttonSize()) {
    case InternalSettings::ButtonTiny:
        return qRound(baseUnit * 0.1);
    case InternalSettings::ButtonSmall:
        return qRound(baseUnit * 0.2);
    default:
    case InternalSettings::ButtonDefault:
        return qRound(baseUnit * 0.3);
    case InternalSettings::ButtonLarge:
        return qRound(baseUnit * 0.5);
    case InternalSettings::ButtonVeryLarge:
        return qRound(baseUnit * 0.8);
    }
}

int Decoration::titleBarHeight() const
{
    const QFontMetrics fontMetrics(settings()->font());
    return buttonPadding()*2 + fontMetrics.height();
}

int Decoration::appMenuButtonHorzPadding() const
{
    // smallSpacing is scaled by dpr with a min of 2px.
    // So we need to divide our "pixel units" by 2 before scaling by it.
    return settings()->smallSpacing() * m_internalSettings->menuButtonHorzPadding() / 2;
}

int Decoration::appMenuCaptionSpacing() const
{
    return settings()->largeSpacing() * 4;
}

int Decoration::captionMinWidth() const
{
    return settings()->largeSpacing() * 8;
}

int Decoration::bottomBorderSize() const {
    const int baseSize = settings()->smallSpacing();
    switch (settings()->borderSize()) {
        default:
        case KDecoration2::BorderSize::None:
            return 0;
        case KDecoration2::BorderSize::NoSides:
        case KDecoration2::BorderSize::Tiny:
            return 1; // Breeze: max(4, baseSize)
        case KDecoration2::BorderSize::Normal:
            return baseSize; // Breeze: baseSize*2
        case KDecoration2::BorderSize::Large:
            return baseSize*2; // Breeze: baseSize*3
        case KDecoration2::BorderSize::VeryLarge:
            return baseSize*3; // Breeze: ...
        case KDecoration2::BorderSize::Huge:
            return baseSize*4;
        case KDecoration2::BorderSize::VeryHuge:
            return baseSize*5;
        case KDecoration2::BorderSize::Oversized:
            return baseSize*10; // Same as Breeze
    }
}
int Decoration::sideBorderSize() const {
    switch (settings()->borderSize()) {
        case KDecoration2::BorderSize::NoSides:
            return 0;
        default:
            return bottomBorderSize();
    }
}

bool Decoration::Decoration::leftBorderVisible() const {
    const auto *decoratedClient = client();
    return !decoratedClient->isMaximizedHorizontally()
        && !decoratedClient->adjacentScreenEdges().testFlag(Qt::LeftEdge);
}
bool Decoration::rightBorderVisible() const {
    const auto *decoratedClient = client();
    return !decoratedClient->isMaximizedHorizontally()
        && !decoratedClient->adjacentScreenEdges().testFlag(Qt::RightEdge);
}
bool Decoration::topBorderVisible() const {
    const auto *decoratedClient = client();
    return !decoratedClient->isMaximizedVertically()
        && !decoratedClient->adjacentScreenEdges().testFlag(Qt::TopEdge);
}
bool Decoration::bottomBorderVisible() const {
    const auto *decoratedClient = client();
    return !decoratedClient->isMaximizedVertically()
        && !decoratedClient->adjacentScreenEdges().testFlag(Qt::BottomEdge)
        && !decoratedClient->isShaded();
}

bool Decoration::titleBarIsHovered() const
{
    return sectionUnderMouse() == Qt::TitleBarArea;
}

int Decoration::getTextWidth(const QString text, bool showMnemonic) const
{
    const QFontMetrics fontMetrics(settings()->font());
    const QRect textRect(titleBarRect());
    int flags = showMnemonic ? Qt::TextShowMnemonic : Qt::TextHideMnemonic;
    const QRect boundingRect = fontMetrics.boundingRect(textRect, flags, text);
    return boundingRect.width();
}

//* scoped pointer convenience typedef
template <typename T> using ScopedPointer = QScopedPointer<T, QScopedPointerPodDeleter>;

QPoint Decoration::windowPos() const
{
    const auto *decoratedClient = client();
    WId windowId = decoratedClient->windowId();

    if (KWindowSystem::isPlatformX11()) {
#if HAVE_X11
        //--- From: BreezeSizeGrip.cpp
        /*
        get root position matching position
        need to use xcb because the embedding of the widget
        breaks QT's mapToGlobal and other methods
        */
        auto connection( QX11Info::connection() );
        xcb_get_geometry_cookie_t cookie( xcb_get_geometry( connection, windowId ) );
        ScopedPointer<xcb_get_geometry_reply_t> reply( xcb_get_geometry_reply( connection, cookie, nullptr ) );
        if (reply) {
            // translate coordinates
            xcb_translate_coordinates_cookie_t coordCookie( xcb_translate_coordinates(
                connection, windowId, reply.data()->root,
                -reply.data()->border_width,
                -reply.data()->border_width ) );

            ScopedPointer< xcb_translate_coordinates_reply_t> coordReply( xcb_translate_coordinates_reply( connection, coordCookie, nullptr ) );

            if (coordReply) {
                return QPoint(coordReply.data()->dst_x, coordReply.data()->dst_y);
            }
        }
#else
        Q_UNUSED(windowId)
#endif

    } else if (KWindowSystem::isPlatformWayland()) {
#if HAVE_Wayland
        // TODO
#endif
    }

    return QPoint(0, 0);
}

void Decoration::initDragMove(const QPoint pos)
{
    m_pressedPoint = pos;
}

void Decoration::resetDragMove()
{
    m_pressedPoint = QPoint();
}


bool Decoration::dragMoveTick(const QPoint pos)
{
    if (m_pressedPoint.isNull()) {
        return false;
    }

    QPoint diff = pos - m_pressedPoint;
    // qCDebug(category) << "    diff" << diff << "mL" << diff.manhattanLength() << "sDD" << QApplication::startDragDistance();
    if (diff.manhattanLength() >= QApplication::startDragDistance()) {
        sendMoveEvent(pos);
        resetDragMove();
        return true;
    }
    return false;
}

void Decoration::sendMoveEvent(const QPoint pos)
{
    const auto *decoratedClient = client();
    WId windowId = decoratedClient->windowId();

    QPoint globalPos = windowPos()
        - QPoint(0, titleBarHeight())
        + pos;

    if (KWindowSystem::isPlatformX11()) {
#if HAVE_X11
        //--- From: BreezeSizeGrip.cpp
        auto connection(QX11Info::connection());


        // move/resize atom
        if (!m_moveResizeAtom) {
            // create atom if not found
            const QString atomName( "_NET_WM_MOVERESIZE" );
            xcb_intern_atom_cookie_t cookie( xcb_intern_atom( connection, false, atomName.size(), qPrintable( atomName ) ) );
            ScopedPointer<xcb_intern_atom_reply_t> reply( xcb_intern_atom_reply( connection, cookie, nullptr ) );
            m_moveResizeAtom = reply ? reply->atom : 0;
        }
        if (!m_moveResizeAtom) {
            return;
        }

        // button release event
        xcb_button_release_event_t releaseEvent;
        memset(&releaseEvent, 0, sizeof(releaseEvent));

        releaseEvent.response_type = XCB_BUTTON_RELEASE;
        releaseEvent.event =  windowId;
        releaseEvent.child = XCB_WINDOW_NONE;
        releaseEvent.root = QX11Info::appRootWindow();
        releaseEvent.event_x = pos.x();
        releaseEvent.event_y = pos.y();
        releaseEvent.root_x = globalPos.x();
        releaseEvent.root_y = globalPos.y();
        releaseEvent.detail = XCB_BUTTON_INDEX_1;
        releaseEvent.state = XCB_BUTTON_MASK_1;
        releaseEvent.time = XCB_CURRENT_TIME;
        releaseEvent.same_screen = true;
        xcb_send_event(
            connection,
            false,
            windowId,
            XCB_EVENT_MASK_BUTTON_RELEASE,
            reinterpret_cast<const char*>(&releaseEvent)
        );

        xcb_ungrab_pointer(connection, XCB_TIME_CURRENT_TIME);

        // move resize event
        xcb_client_message_event_t clientMessageEvent;
        memset(&clientMessageEvent, 0, sizeof(clientMessageEvent));

        clientMessageEvent.response_type = XCB_CLIENT_MESSAGE;
        clientMessageEvent.type = m_moveResizeAtom;
        clientMessageEvent.format = 32;
        clientMessageEvent.window = windowId;
        clientMessageEvent.data.data32[0] = globalPos.x();
        clientMessageEvent.data.data32[1] = globalPos.y();
        clientMessageEvent.data.data32[2] = 8; // _NET_WM_MOVERESIZE_MOVE
        clientMessageEvent.data.data32[3] = Qt::LeftButton;
        clientMessageEvent.data.data32[4] = 0;

        xcb_send_event(
            connection,
            false,
            QX11Info::appRootWindow(),
            XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
            reinterpret_cast<const char*>(&clientMessageEvent)
        );

        xcb_flush(connection);
#else
        Q_UNUSED(windowId)
        Q_UNUSED(globalPos)
#endif

    } else if (KWindowSystem::isPlatformWayland()) {
#if HAVE_Wayland
        // TODO
#endif

    } else {
        // Not X11
    }
}

void Decoration::paintFrameBackground(QPainter *painter, const QRect &repaintRegion) const
{
    Q_UNUSED(repaintRegion)

    painter->save();

    painter->fillRect(rect(), Qt::transparent);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(borderColor());
    painter->setClipRect(0, borderTop(), size().width(), size().height() - borderTop(), Qt::IntersectClip);
    painter->drawRect(rect());

    painter->restore();
}

QColor Decoration::borderColor() const
{
    const auto *decoratedClient = client();
    const auto group = decoratedClient->isActive()
        ? KDecoration2::ColorGroup::Active
        : KDecoration2::ColorGroup::Inactive;
    const qreal opacity = decoratedClient->isActive()
        ? m_internalSettings->activeOpacity()
        : m_internalSettings->inactiveOpacity();
    QColor color = decoratedClient->color(group, KDecoration2::ColorRole::Frame);
    color.setAlphaF(opacity);
    return color;
}

QColor Decoration::titleBarBackgroundColor() const
{
    const auto *decoratedClient = client();
    const auto group = decoratedClient->isActive()
        ? KDecoration2::ColorGroup::Active
        : KDecoration2::ColorGroup::Inactive;
    const qreal opacity = decoratedClient->isActive()
        ? m_internalSettings->activeOpacity()
        : m_internalSettings->inactiveOpacity();
    QColor color = decoratedClient->color(group, KDecoration2::ColorRole::TitleBar);
    color.setAlphaF(opacity);
    return color;
}

QColor Decoration::titleBarForegroundColor() const
{
    const auto *decoratedClient = client();
    const auto group = decoratedClient->isActive()
        ? KDecoration2::ColorGroup::Active
        : KDecoration2::ColorGroup::Inactive;
    return decoratedClient->color(group, KDecoration2::ColorRole::Foreground);
}

void Decoration::paintTitleBarBackground(QPainter *painter, const QRect &repaintRegion) const
{
    Q_UNUSED(repaintRegion)

    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(titleBarBackgroundColor());
    painter->drawRect(QRect(0, 0, size().width(), titleBarHeight()));
    painter->restore();
}

void Decoration::paintCaption(QPainter *painter, const QRect &repaintRegion) const
{
    Q_UNUSED(repaintRegion)

    if (m_internalSettings->titleAlignment() == InternalSettings::TitleHidden) {
        return;
    }

    const auto *decoratedClient = client();

    const int textWidth = settings()->fontMetrics().boundingRect(decoratedClient->caption()).width();
    const QRect textRect((size().width() - textWidth) / 2, 0, textWidth, titleBarHeight());

    const bool appMenuVisible = !m_menuButtons->buttons().isEmpty();
    const int menuButtonsWidth = m_menuButtons->geometry().width()
        + (appMenuVisible ? appMenuCaptionSpacing() : 0);

    const QRect availableRect = centerRect().adjusted(
        (m_menuButtons->alwaysShow() ? menuButtonsWidth : 0),
        0,
        0,
        0
    );

    QRect captionRect;
    Qt::Alignment alignment;

    switch (m_internalSettings->titleAlignment()) {
        case InternalSettings::AlignLeft:
            captionRect = availableRect;
            alignment = Qt::AlignLeft | Qt::AlignVCenter;
            break;

        case InternalSettings::AlignRight:
            captionRect = availableRect;
            alignment = Qt::AlignRight | Qt::AlignVCenter;
            break;

        case InternalSettings::AlignCenter:
            captionRect = availableRect;
            alignment = Qt::AlignCenter;
            break;

        default:
        case InternalSettings::AlignCenterFullWidth:
            if (textRect.left() < availableRect.left()) {
                captionRect = availableRect;
                alignment = Qt::AlignLeft | Qt::AlignVCenter;
            } else if (availableRect.right() < textRect.right()) {
                captionRect = availableRect;
                alignment = Qt::AlignRight | Qt::AlignVCenter;
            } else {
                captionRect = titleBarRect();
                alignment = Qt::AlignCenter;
            }
            break;
    }

    const QString caption = painter->fontMetrics().elidedText(
        decoratedClient->caption(), Qt::ElideMiddle, captionRect.width());

    painter->save();
    painter->setFont(settings()->font());

    if (m_menuButtons->buttons().isEmpty()) {
        painter->setPen(titleBarForegroundColor());
    } else { // menuButtons is visible
        const int menuRight = m_menuButtons->geometry().right();
        const int textLeft = textRect.left();
        const int textRight = textRect.right();
        // qCDebug(category) << "textLeft" << textLeft << "menuRight" << menuRight;

        if (!m_menuButtons->alwaysShow()) { // caption fades away revealing menu
            painter->setOpacity(1.0 - m_menuButtons->opacity());
            painter->setPen(titleBarForegroundColor());
        } else if (m_menuButtons->overflowing()) { // hide caption leaving "whitespace" to easily grab.
            painter->setPen(Qt::transparent);
        } else if (textRight < menuRight) { // menuButtons completely coveres caption
            painter->setPen(Qt::transparent);
        } else if (textLeft < menuRight) { // menuButtons covers caption
            const int fadeWidth = 10; // TODO: scale by dpi
            const int x1 = menuRight;
            const int x2 = qMin(x1+fadeWidth, textRight);
            const float x1Ratio = (float)(x1-textLeft) / (float)textWidth;
            const float x2Ratio = (float)(x2-textLeft) / (float)textWidth;
            // qCDebug(category) << "    " << "x2" << x2 << "x1R" << x1Ratio << "x2R" << x2Ratio;
            QLinearGradient gradient(textRect.topLeft(), textRect.bottomRight());
            gradient.setColorAt(x1Ratio, Qt::transparent);
            gradient.setColorAt(x2Ratio, titleBarForegroundColor());
            QBrush brush(gradient);
            QPen pen(brush, 1);
            painter->setPen(pen);
        } else { // caption is not covered by menuButtons
            painter->setPen(titleBarForegroundColor());
        }
    }

    painter->drawText(captionRect, alignment, caption);
    painter->restore();
}

void Decoration::paintButtons(QPainter *painter, const QRect &repaintRegion) const
{
    m_leftButtons->paint(painter, repaintRegion);
    m_rightButtons->paint(painter, repaintRegion);
    m_menuButtons->paint(painter, repaintRegion);
}

void Decoration::paintOutline(QPainter *painter, const QRect &repaintRegion) const
{
    Q_UNUSED(repaintRegion)

    // Simple 1px border outline
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setBrush(Qt::NoBrush);
    QColor outlineColor(titleBarForegroundColor());
    outlineColor.setAlphaF(0.25);
    painter->setPen(outlineColor);
    painter->drawRect( rect().adjusted( 0, 0, -1, -1 ) );
    painter->restore();
}

} // namespace Material
