// SPDX-License-Identifier: GPL-2.0-or-later
#include "TilingPopup.h"
#include "TilingZoneButton.h"

#include <KLocalizedString>
#include <KWindowShadow>
#include <QPlatformSurfaceEvent>
#include <QWindow>
#include <QApplication>
#include <QGridLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QWidgetAction>
#include <limits>

namespace Material
{

QVector<TilingLayout> tilingLayouts()
{
    const TilingZone left{{0, 0, .5, 1}, Qt::LeftEdge, i18nd("materialdecoration", "Left half")};
    const TilingZone right{{.5, 0, .5, 1}, Qt::RightEdge, i18nd("materialdecoration", "Right half")};
    const TilingZone top{{0, 0, 1, .5}, Qt::TopEdge, i18nd("materialdecoration", "Top half")};
    const TilingZone bottom{{0, .5, 1, .5}, Qt::BottomEdge, i18nd("materialdecoration", "Bottom half")};
    const TilingZone topLeft{{0, 0, .5, .5}, Qt::TopEdge | Qt::LeftEdge, i18nd("materialdecoration", "Top left quarter")};
    const TilingZone topRight{{.5, 0, .5, .5}, Qt::TopEdge | Qt::RightEdge, i18nd("materialdecoration", "Top right quarter")};
    const TilingZone bottomLeft{{0, .5, .5, .5}, Qt::BottomEdge | Qt::LeftEdge, i18nd("materialdecoration", "Bottom left quarter")};
    const TilingZone bottomRight{{.5, .5, .5, .5}, Qt::BottomEdge | Qt::RightEdge, i18nd("materialdecoration", "Bottom right quarter")};
    return {{left, right}, {top, bottom}, {left, topRight, bottomRight}, {topLeft, topRight, bottomLeft, bottomRight}};
}

TilingPopup::TilingPopup(QWidget *parent)
    : QMenu(parent)
    , m_shadow(new KWindowShadow(this))
{
    setProperty("_KDE_NET_WM_SKIP_SHADOW", true);
    m_shadowUpdateTimer.setSingleShot(true);
    connect(&m_shadowUpdateTimer, &QTimer::timeout, this, &TilingPopup::updateShadow);
    // Use an alpha surface and paint only the backdrop with reduced opacity;
    // window opacity would also fade the layout buttons and their highlight.
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName(QStringLiteral("materialTilingPopup"));
    installEventFilter(this);
    setAccessibleName(i18nd("materialdecoration", "Window tiling layouts"));
    auto *panel = new QWidget(this);
    auto *grid = new QGridLayout(panel);
    const int unit = fontMetrics().height();
    const int gap = qMax(4, unit / 3);
    grid->setContentsMargins(gap, gap, gap, gap);
    grid->setSpacing(gap * 2);
    const auto layouts = tilingLayouts();
    for (qsizetype i = 0; i < layouts.size(); ++i) {
        auto *preview = new QWidget(panel);
        preview->setFixedSize(unit * 8, unit * 5);
        grid->addWidget(preview, int(i / 2), int(i % 2));
        for (const auto &zone : layouts[i]) {
            Qt::Edges outerEdges;
            if (qFuzzyIsNull(zone.rect.left())) outerEdges |= Qt::LeftEdge;
            if (qFuzzyCompare(zone.rect.right(), 1.0)) outerEdges |= Qt::RightEdge;
            if (qFuzzyIsNull(zone.rect.top())) outerEdges |= Qt::TopEdge;
            if (qFuzzyCompare(zone.rect.bottom(), 1.0)) outerEdges |= Qt::BottomEdge;
            auto *button = new TilingZoneButton(outerEdges, preview);
            const QRectF bounds(zone.rect.x() * preview->width(), zone.rect.y() * preview->height(),
                                zone.rect.width() * preview->width(), zone.rect.height() * preview->height());
            button->setGeometry(bounds.toAlignedRect().adjusted(gap / 2, gap / 2, -gap / 2, -gap / 2));
            button->setAccessibleName(zone.label);
            button->setToolTip(zone.label);
            button->installEventFilter(this);
            m_buttons.append(button);
            connect(button, &QToolButton::clicked, this, [this, edges = zone.edges] {
                hide();
                Q_EMIT zoneSelected(edges);
            });
        }
    }
    auto *action = new QWidgetAction(this);
    action->setDefaultWidget(panel);
    addAction(action);
}

void TilingPopup::setAnimationSettings(bool enabled, int duration)
{
    for (auto *button : m_buttons) button->setAnimationSettings(enabled, duration);
}

void TilingPopup::setCornerRadius(qreal radius)
{
    m_cornerRadius = qMax(qreal(0), radius);
    for (auto *button : m_buttons) button->setCornerRadius(m_cornerRadius);
    update();
}

void TilingPopup::setSquareCornersTiled(bool enabled)
{
    for (auto *button : m_buttons) button->setSquareCornersTiled(enabled);
}

void TilingPopup::setBackgroundOpacity(int percent)
{
    m_backgroundOpacity = qBound(0, percent, 100) / 100.0;
    update();
}

void TilingPopup::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor background = palette().color(QPalette::Window);
    background.setAlphaF(m_backgroundOpacity);
    QColor border = palette().color(QPalette::Mid);
    border.setAlphaF(m_backgroundOpacity);
    painter.setPen(border);
    painter.setBrush(background);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), m_cornerRadius, m_cornerRadius);
}

void TilingPopup::setShadowTexture(const QImage &image, const QMargins &padding)
{
    if (m_shadowTexture == image && m_shadowPadding == padding) return;
    m_shadowTexture = image;
    m_shadowPadding = padding;
    if (isVisible()) m_shadowUpdateTimer.start(0);
}

void TilingPopup::updateShadow()
{
    if (!isVisible() || !windowHandle()) return;
    m_shadow->destroy();
    m_shadow->setWindow(windowHandle());
    windowHandle()->installEventFilter(this);
    const auto tile = [](const QImage &image) {
        auto result = KWindowShadowTile::Ptr::create();
        result->setImage(image);
        return result;
    };
    QImage image = m_shadowTexture;
    QMargins padding = m_shadowPadding;
    if (image.isNull()) {
        // Transparent tiles also replace any shadow installed by the style.
        image = QImage(3, 3, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        padding = QMargins();
    }
    const int x = image.width() / 2;
    const int y = image.height() / 2;
    const int right = image.width() - x - 1;
    const int bottom = image.height() - y - 1;
    m_shadow->setTopLeftTile(tile(image.copy(0, 0, x, y)));
    m_shadow->setTopTile(tile(image.copy(x, 0, 1, y)));
    m_shadow->setTopRightTile(tile(image.copy(x + 1, 0, right, y)));
    m_shadow->setLeftTile(tile(image.copy(0, y, x, 1)));
    m_shadow->setRightTile(tile(image.copy(x + 1, y, right, 1)));
    m_shadow->setBottomLeftTile(tile(image.copy(0, y + 1, x, bottom)));
    m_shadow->setBottomTile(tile(image.copy(x, y + 1, 1, bottom)));
    m_shadow->setBottomRightTile(tile(image.copy(x + 1, y + 1, right, bottom)));
    m_shadow->setPadding(padding);
    m_shadow->create();
}

void TilingPopup::showEvent(QShowEvent *event)
{
    m_waitingForRelease = QApplication::mouseButtons().testFlag(Qt::LeftButton);
    QMenu::showEvent(event);
    for (auto *button : m_buttons) button->resetHighlight();
    selectKeyboardButton(-1);
    updateShadow();
}

bool TilingPopup::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_shadow->window()) {
        if (event->type() == QEvent::PlatformSurface) {
            const auto *surface = static_cast<QPlatformSurfaceEvent *>(event);
            if (surface->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
                m_shadow->destroy();
            } else if (isVisible()) {
                m_shadowUpdateTimer.start(0);
            }
        }
        return false;
    }
    if (object == this && event->type() == QEvent::StyleChange && isVisible()) {
        m_shadowUpdateTimer.start(0);
    }
    if (event->type() == QEvent::MouseButtonPress) {
        m_waitingForRelease = false;
    } else if (event->type() == QEvent::MouseButtonRelease && m_waitingForRelease) {
        // The release ending the long press must not select a zone.
        m_waitingForRelease = false;
        return true;
    } else if (event->type() == QEvent::KeyPress) {
        if (handleKeyPress(static_cast<QKeyEvent *>(event))) return true;
    }
    return QMenu::eventFilter(object, event);
}

void TilingPopup::selectKeyboardButton(int index)
{
    m_keyboardIndex = index;
    for (qsizetype i = 0; i < m_buttons.size(); ++i) {
        auto *button = m_buttons[i];
        button->setKeyboardSelected(i == index);
    }
    if (index >= 0) m_buttons[index]->setFocus(Qt::OtherFocusReason);
    else setFocus(Qt::PopupFocusReason);
}

bool TilingPopup::handleKeyPress(QKeyEvent *event)
{
    const int code = event->key();
    if (code == Qt::Key_Escape) {
        hide();
        return true;
    }
    if (code == Qt::Key_Return || code == Qt::Key_Enter || code == Qt::Key_Space) {
        // Opening the popup alone must never arm a default action.
        if (m_keyboardIndex >= 0) m_buttons[m_keyboardIndex]->click();
        return true;
    }
    if (code == Qt::Key_Tab || code == Qt::Key_Backtab) {
        const bool backwards = code == Qt::Key_Backtab || event->modifiers().testFlag(Qt::ShiftModifier);
        const int count = int(m_buttons.size());
        selectKeyboardButton(m_keyboardIndex < 0 ? (backwards ? count - 1 : 0)
            : (m_keyboardIndex + (backwards ? count - 1 : 1)) % count);
        return true;
    }
    if (code != Qt::Key_Left && code != Qt::Key_Right && code != Qt::Key_Up && code != Qt::Key_Down) return false;
    if (m_keyboardIndex < 0) {
        selectKeyboardButton(0);
        return true;
    }
    const auto *current = m_buttons[m_keyboardIndex];
    const QPoint origin = current->mapTo(this, current->rect().center());
    int next = m_keyboardIndex;
    qreal best = std::numeric_limits<qreal>::max();
    for (qsizetype i = 0; i < m_buttons.size(); ++i) {
        const QPoint delta = m_buttons[i]->mapTo(this, m_buttons[i]->rect().center()) - origin;
        const int forward = code == Qt::Key_Left ? -delta.x() : code == Qt::Key_Right ? delta.x()
            : code == Qt::Key_Up ? -delta.y() : delta.y();
        const int sideways = (code == Qt::Key_Left || code == Qt::Key_Right) ? delta.y() : delta.x();
        const qreal score = forward * forward + 4.0 * sideways * sideways;
        if (forward > 0 && score < best) { best = score; next = int(i); }
    }
    selectKeyboardButton(next);
    return true;
}

}
