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

#pragma once

// KDecoration
#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationButton>

// Qt
#include <QMarginsF>
#include <QMouseEvent>
#include <QRectF>
#include <QVariantAnimation>
#include <QTransform>

class QTimer;
class QPainter;

#include "PixelSnapper.h"

namespace Material
{

class Decoration;

/**
 * @brief Base class for window decoration buttons in the Material theme.
 *
 * Implements KDecoration3::DecorationButton with support for smooth hover animations,
 * long-press hold timers, pen stroke width scaling, and pixel-snapped icon painting.
 */
class Button : public KDecoration3::DecorationButton
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a decoration button of the given type.
     * @param type Decoration button type (Close, Maximize, Minimize, etc.).
     * @param decoration Pointer to parent Decoration.
     * @param parent Optional parent object.
     */
    Button(KDecoration3::DecorationButtonType type, Decoration *decoration, QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~Button() override;

    Q_PROPERTY(bool animationEnabled READ animationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(qreal transitionValue READ transitionValue WRITE setTransitionValue NOTIFY transitionValueChanged)

    /**
     * @brief Factory method for creating decoration buttons used by KDecoration3::DecorationButtonGroup.
     * @param type Button type.
     * @param decoration Parent decoration.
     * @param parent Optional parent object.
     * @return Created KDecorationButton instance.
     */
    static KDecoration3::DecorationButton *create(KDecoration3::DecorationButtonType type, KDecoration3::Decoration *decoration, QObject *parent = nullptr);

    /**
     * @brief Plugin constructor used when loading buttons dynamically (e.g., applet-window-buttons).
     * @param parent Parent object.
     * @param args Plugin initialization arguments.
     */
    explicit Button(QObject *parent, const QVariantList &args);

    /**
     * @brief Paints the button background, hover effects, and icon.
     * @param painter QPainter instance.
     * @param repaintRegion Repaint bounding box.
     */
    void paint(QPainter *painter, const QRectF &repaintRegion) override;

    /**
     * @brief Handles mouse release events, triggering action or hold timer cancellation.
     * @param event Mouse event.
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /**
     * @brief Forces the button to unpressed state and cancels any active hover/hold state.
     */
    void forceUnpress();

    /**
     * @brief Configures painter pen width adjusted for scale and optional pixel snapping.
     * @param painter Target QPainter.
     * @param scale Pen scale factor.
     * @param snapped True if pen width should be snapped to integer physical pixels.
     */
    void setPenWidth(QPainter *painter, const qreal scale, bool snapped = false);

    /**
     * @brief Checks if hover animations are enabled.
     * @return True if animations are enabled.
     */
    bool animationEnabled() const;

    /**
     * @brief Enables or disables hover animations.
     * @param value True to enable animations.
     */
    void setAnimationEnabled(bool value);

    /**
     * @brief Returns hover animation duration in milliseconds.
     * @return Animation duration in ms.
     */
    int animationDuration() const;

    /**
     * @brief Sets hover animation duration in milliseconds.
     * @param duration Duration in ms.
     */
    void setAnimationDuration(int duration);

    /**
     * @brief Returns button opacity.
     * @return Opacity value between 0.0 and 1.0.
     */
    qreal opacity() const;

    /**
     * @brief Sets button opacity.
     * @param value Opacity value between 0.0 and 1.0.
     */
    void setOpacity(qreal value);

    /**
     * @brief Sets horizontal padding for the button content.
     * @param value Horizontal padding in local units.
     */
    void setHorzPadding(qreal value);

    /**
     * @brief Sets whether this button is the leftmost button in its group.
     * @param isLeftmost True if leftmost button.
     */
    void setIsLeftmost(bool isLeftmost);

    /**
     * @brief Sets whether this button is the rightmost button in its group.
     * @param isRightmost True if rightmost button.
     */
    void setIsRightmost(bool isRightmost);

    /**
     * @brief Returns active pen scale factor considering button dimensions and DPR.
     * @return Pen scale factor.
     */
    qreal penScale() const;

private:
    void updateAnimationState(bool hovered);
    void handleHoldTimeout();

signals:
    void animationEnabledChanged();
    void animationDurationChanged();
    void opacityChanged();
    void transitionValueChanged(qreal);
    void paddingChanged();

protected:
    virtual void paintIcon(QPainter *painter, const QRectF &iconRect, const PixelSnapper &snapper);
    virtual void updateSize(qreal contentWidth, qreal contentHeight);
    virtual void setHeight(qreal buttonHeight);

    virtual QColor backgroundColor() const;
    virtual QColor foregroundColor() const;

    QRectF contentArea() const;

    qreal transitionValue() const;
    void setTransitionValue(qreal value);

    QMarginsF &padding();

    void onCloseHold();
    void onMinimizeHold();
    void onMaximizeHold();

    bool m_animationEnabled;
    QVariantAnimation *m_animation;
    qreal m_opacity;
    qreal m_transitionValue;
    QMarginsF m_padding;
    bool m_isGtkButton;
    bool m_isLeftmost = false;
    bool m_isRightmost = false;

    QTimer *m_holdTimer = nullptr;
    bool m_longPressTriggered = false;
    qreal m_penScale = 1.0;

    friend class Decoration;
};

} // namespace Material
