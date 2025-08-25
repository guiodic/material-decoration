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

// own
#include "BuildConfig.h"
#include "AppMenuButtonGroup.h"
#include "InternalSettings.h"

// KDecoration
#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationButton>
#include <KDecoration3/DecorationButtonGroup>

// Qt
#include <QHoverEvent>
#include <QMouseEvent>
#include <QRectF>
#include <QSharedPointer>
#include <QWheelEvent>
#include <QVariant>

#if HAVE_X11
#include <xcb/xcb.h>
#endif


namespace Material
{

class Button;
class TextButton;
class MenuOverflowButton;

class Decoration : public KDecoration3::Decoration
{
    Q_OBJECT

public:
    Decoration(QObject *parent = nullptr, const QVariantList &args = QVariantList());
    ~Decoration() override;

    QRect titleBarRect() const;
    QRect centerRect() const;

    void paint(QPainter *painter, const QRectF &repaintRegion) override;

    WId safeWindowId() const;

public slots:
    bool init() override;
    void reconfigure();

protected:
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private slots:
    void onSectionUnderMouseChanged(const Qt::WindowFrameSection value);

private:
    void updateBlur();
    void updateBorders();
    void updateResizeBorders();
    void updateTitleBar();
    void updateTitleBarHoverState();
    void setButtonGroupHeight(KDecoration3::DecorationButtonGroup *buttonGroup, int buttonHeight);
    void setButtonGroupHorzPadding(KDecoration3::DecorationButtonGroup *buttonGroup, int value);
    void setButtonGroupVertPadding(KDecoration3::DecorationButtonGroup *buttonGroup, int value);
    void updateButtonHeight();
    void updateButtonsGeometry();
    void setButtonGroupAnimation(KDecoration3::DecorationButtonGroup *buttonGroup, bool enabled, int duration);
    void updateButtonAnimation();
    void updateShadow();

    bool menuAlwaysShow() const;
    bool searchEnabled() const;
    bool showDisabledActions() const;
    bool animationsEnabled() const;
    int animationsDuration() const;
    int buttonPadding() const;
    int titleBarHeight() const;
    int appMenuButtonHorzPadding() const;
    int appMenuCaptionSpacing() const;
    int captionMinWidth() const;

    int bottomBorderSize() const;
    int sideBorderSize() const;

    bool leftBorderVisible() const;
    bool rightBorderVisible() const;
    bool topBorderVisible() const;
    bool bottomBorderVisible() const;

    bool titleBarIsHovered() const;
    int getTextWidth(const QString text, bool showMnemonic = false) const;
    QPoint windowPos() const;

    void initDragMove(const QPoint pos);
    void resetDragMove();
    bool dragMoveTick(const QPoint pos);
    void sendMoveEvent(const QPoint pos);

    QColor borderColor() const;
    QColor titleBarBackgroundColor() const;
    QColor titleBarForegroundColor() const;

    void paintFrameBackground(QPainter *painter, const QRectF &repaintRegion) const;
    void paintTitleBarBackground(QPainter *painter, const QRectF &repaintRegion) const;
    void paintCaption(QPainter *painter, const QRectF &repaintRegion) const;
    void paintButtons(QPainter *painter, const QRectF &repaintRegion) const;
    void paintOutline(QPainter *painter, const QRectF &repaintRegion) const;

    KDecoration3::DecorationButtonGroup *m_leftButtons;
    KDecoration3::DecorationButtonGroup *m_rightButtons;
    AppMenuButtonGroup *m_menuButtons;

    QSharedPointer<InternalSettings> m_internalSettings;

    QPoint m_pressedPoint;

#if HAVE_X11
    xcb_atom_t m_moveResizeAtom = 0;
#endif

    friend class AppMenuButtonGroup;
    friend class Button;
    friend class AppIconButton;
    friend class AppMenuButton;
    friend class TextButton;
    // friend class MenuOverflowButton;
};

} // namespace Material
