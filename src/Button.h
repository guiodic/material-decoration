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
#include <QRectF>
#include <QVariantAnimation>

namespace Material
{

class Decoration;

class Button : public KDecoration3::DecorationButton
{
    Q_OBJECT

public:
    Button(KDecoration3::DecorationButtonType type, Decoration *decoration, QObject *parent = nullptr);
    ~Button() override;

    Q_PROPERTY(bool animationEnabled READ animationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(qreal transitionValue READ transitionValue WRITE setTransitionValue NOTIFY transitionValueChanged)
    //Q_PROPERTY(QMarginsF* padding READ padding NOTIFY paddingChanged)

    // Passed to DecorationButtonGroup in Decoration
    static KDecoration3::DecorationButton *create(KDecoration3::DecorationButtonType type, KDecoration3::Decoration *decoration, QObject *parent = nullptr);

    // This is called by:
    // registerPlugin<Material::Button>(QStringLiteral("button"))
    // It is needed to create buttons for applet-window-buttons.
    explicit Button(QObject *parent, const QVariantList &args);


    void paint(QPainter *painter, const QRectF &repaintRegion) override;
    virtual void paintIcon(QPainter *painter, const QRectF &iconRect, const qreal);


    virtual void updateSize(qreal contentWidth, qreal contentHeight);
    virtual void setHeight(qreal buttonHeight);


    void forceUnpress();

    virtual qreal iconLineWidth(const qreal size) const;
    void setPenWidth(QPainter *painter, const qreal scale);

    virtual QColor backgroundColor() const;
    virtual QColor foregroundColor() const;

    QRectF contentArea() const;

    bool animationEnabled() const;
    void setAnimationEnabled(bool value);

    int animationDuration() const;
    void setAnimationDuration(int duration);

    qreal opacity() const;
    void setOpacity(qreal value);

    qreal transitionValue() const;
    void setTransitionValue(qreal value);

    QMarginsF &padding();
    void setHorzPadding(qreal value);
   // void setVertPadding(int value);

    void setIsLeftmost(bool isLeftmost);
    void setIsRightmost(bool isRightmost);
    
    bool windowIsMaximized();

private Q_SLOTS:
    void updateAnimationState(bool hovered);

signals:
    void animationEnabledChanged();
    void animationDurationChanged();
    void opacityChanged();
    void transitionValueChanged(qreal);
    void paddingChanged();

protected:
    bool m_animationEnabled;
    QVariantAnimation *m_animation;
    qreal m_opacity;
    qreal m_transitionValue;
    QMarginsF m_padding;
    bool m_isGtkButton;
    bool m_isLeftmost = false;
    bool m_isRightmost = false;
};

} // namespace Material
