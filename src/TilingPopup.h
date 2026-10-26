// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "CustomTilingLayout.h"
#include <QMenu>
#include <QVector>

class QLabel;

namespace Material
{

class TilingZoneButton;

// Logical edges, independent of the KWin API and screen coordinates.
struct TilingZone {
    QRectF rect;
    Qt::Edges edges;
    QString label;
    QString customTileId = {};
};
using TilingLayout = QVector<TilingZone>;
QVector<TilingLayout> tilingLayouts();

class TilingPopup : public QMenu
{
    Q_OBJECT
public:
    explicit TilingPopup(QWidget *parent = nullptr);
    void setCustomLayout(const CustomTilingLayout &layout);
    void setZoneCornerRadius(qreal radius);
    void setSquareCornersTiled(bool enabled);
    void setAnimationSettings(bool enabled, int duration);

signals:
    void zoneSelected(Qt::Edges edges);
    void customZoneSelected(const QString &tileId);
    void keyboardNavigationStarted();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    TilingZoneButton *createZone(QWidget *preview, const TilingZone &zone);
    bool handleKeyPress(QKeyEvent *event);
    void selectKeyboardButton(int index);

    QVector<TilingZoneButton *> m_buttons;
    QWidget *m_customPreview = nullptr;
    QLabel *m_customPlaceholder = nullptr;
    qreal m_zoneCornerRadius = 6;
    bool m_squareCornersTiled = false;
    bool m_animationsEnabled = true;
    int m_animationDuration = 250;
    int m_keyboardIndex = -1;
    bool m_waitingForRelease = false;
};

}
