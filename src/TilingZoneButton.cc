// SPDX-License-Identifier: GPL-2.0-or-later
#include "TilingZoneButton.h"

#include <QPainter>
#include <cmath>

namespace Material
{
TilingZoneButton::TilingZoneButton(QWidget *parent)
    : QToolButton(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    m_animation.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_progress = value.toReal();
        update();
    });
}

void TilingZoneButton::setKeyboardSelected(bool selected)
{
    if (m_keyboardSelected == selected) return;
    m_keyboardSelected = selected;
    updateHighlight();
}

void TilingZoneButton::setAnimationSettings(bool enabled, int duration)
{
    duration = qMax(0, duration);
    if (m_animationsEnabled == enabled && m_duration == duration) return;
    m_animationsEnabled = enabled;
    m_duration = duration;
    updateHighlight();
}

void TilingZoneButton::resetHighlight()
{
    m_animation.stop();
    m_progress = 0;
    m_hovered = false;
    m_keyboardSelected = false;
    update();
}

void TilingZoneButton::updateHighlight()
{
    const qreal target = (m_hovered || m_keyboardSelected) ? 1.0 : 0.0;
    if (!m_animationsEnabled || !m_duration || !isVisible()) {
        m_animation.stop();
        m_progress = target;
        update();
        return;
    }
    if (m_animation.state() == QAbstractAnimation::Running && m_animation.endValue().toReal() == target) return;
    m_animation.stop();
    if (qFuzzyIsNull(m_progress - target)) return;
    // Start at the currently painted value, including when reversing mid-flight.
    m_animation.setStartValue(m_progress);
    m_animation.setEndValue(target);
    m_animation.setDuration(qMax(1, qRound(m_duration * std::abs(target - m_progress))));
    m_animation.start();
}

QColor TilingZoneButton::interpolatedColor(QPalette::ColorRole normal) const
{
    const QColor from = palette().color(normal);
    const QColor to = palette().color(QPalette::Highlight);
    const auto mix = [this](qreal a, qreal b) { return a + (b - a) * m_progress; };
    // Only the popup backdrop is translucent; zones always remain opaque.
    return QColor::fromRgbF(mix(from.redF(), to.redF()), mix(from.greenF(), to.greenF()), mix(from.blueF(), to.blueF()));
}

void TilingZoneButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(interpolatedColor(QPalette::Mid));
    painter.setBrush(interpolatedColor(QPalette::Midlight));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
}

void TilingZoneButton::enterEvent(QEnterEvent *event)
{
    QToolButton::enterEvent(event);
    m_hovered = true;
    updateHighlight();
}

void TilingZoneButton::leaveEvent(QEvent *event)
{
    QToolButton::leaveEvent(event);
    m_hovered = false;
    updateHighlight();
}

void TilingZoneButton::hideEvent(QHideEvent *event)
{
    QToolButton::hideEvent(event);
    resetHighlight();
}
}
