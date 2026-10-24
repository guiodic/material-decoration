// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QToolButton>
#include <QVariantAnimation>

namespace Material
{
class TilingZoneButton : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(bool keyboardSelected READ keyboardSelected WRITE setKeyboardSelected)
public:
    explicit TilingZoneButton(Qt::Edges outerEdges, QWidget *parent = nullptr);
    bool keyboardSelected() const { return m_keyboardSelected; }
    void setKeyboardSelected(bool selected);
    void setAnimationSettings(bool enabled, int duration);
    void resetHighlight();
    void setCornerRadius(qreal radius);
    void setSquareCornersTiled(bool enabled);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void updateHighlight();
    QColor interpolatedColor(QPalette::ColorRole normal) const;

    QVariantAnimation m_animation;
    qreal m_progress = 0;
    qreal m_cornerRadius = 6;
    Qt::Edges m_outerEdges;
    bool m_squareCornersTiled = false;
    bool m_hovered = false;
    bool m_keyboardSelected = false;
    bool m_animationsEnabled = true;
    int m_duration = 250;
};
}
