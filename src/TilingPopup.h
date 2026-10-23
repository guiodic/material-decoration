// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QMenu>
#include <QVector>

class QToolButton;

namespace Material
{

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
    void setTransparency(int percent);

signals:
    void zoneSelected(Qt::Edges edges);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QVector<QToolButton *> m_buttons;
    bool m_waitingForRelease = false;
    qreal m_backgroundOpacity = 0.85;
};

}
