// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QToolButton>
#include <QVariantAnimation>

namespace Material
{
// A zone's appearance is independent of its layout and tiling operation.
class TilingZoneButton : public QToolButton
{
    Q_OBJECT
    Q_PROPERTY(bool keyboardSelected READ keyboardSelected WRITE setKeyboardSelected)
public:
    explicit TilingZoneButton(QWidget *parent = nullptr);
    bool keyboardSelected() const { return m_keyboardSelected; }
    void setKeyboardSelected(bool selected);
    void setAnimationSettings(bool enabled, int duration);
    void resetHighlight();

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
    bool m_hovered = false;
    bool m_keyboardSelected = false;
    bool m_animationsEnabled = true;
    int m_duration = 250;
};
}
