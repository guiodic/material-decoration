// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QMenu>
#include <QVector>

namespace Material
{

class TilingZoneButton;

// Logical edges, independent of the KWin API and screen coordinates.
struct TilingZone {
    QRectF rect;
    Qt::Edges edges;
    QString label;
};
using TilingLayout = QVector<TilingZone>;
QVector<TilingLayout> tilingLayouts();

class TilingPopup : public QMenu
{
    Q_OBJECT
public:
    explicit TilingPopup(QWidget *parent = nullptr);
    void setBackgroundOpacity(int percent);
    void setCornerRadius(qreal radius);
    void setSquareCornersTiled(bool enabled);
    void setAnimationSettings(bool enabled, int duration);

signals:
    void zoneSelected(Qt::Edges edges);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    bool handleKeyPress(QKeyEvent *event);
    void selectKeyboardButton(int index);

    QVector<TilingZoneButton *> m_buttons;
    int m_keyboardIndex = -1;
    bool m_waitingForRelease = false;
    qreal m_backgroundOpacity = 0.85;
    qreal m_cornerRadius = 6;
};

}
