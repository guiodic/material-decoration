// SPDX-License-Identifier: GPL-2.0-or-later
#include "TilingPopup.h"

#include <KLocalizedString>
#include <QApplication>
#include <QGridLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QToolButton>
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
{
    setObjectName(QStringLiteral("materialTilingPopup"));
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
            auto *button = new QToolButton(preview);
            const QRectF bounds(zone.rect.x() * preview->width(), zone.rect.y() * preview->height(),
                                zone.rect.width() * preview->width(), zone.rect.height() * preview->height());
            button->setGeometry(bounds.toAlignedRect().adjusted(gap / 2, gap / 2, -gap / 2, -gap / 2));
            button->setAccessibleName(zone.label);
            button->setToolTip(zone.label);
            button->setFocusPolicy(Qt::StrongFocus);
            button->setStyleSheet(QStringLiteral(
                "QToolButton { background: palette(midlight); border: 1px solid palette(mid); border-radius: 4px; }"
                "QToolButton:hover, QToolButton:focus { background: palette(highlight); border-color: palette(highlight); }"));
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

void TilingPopup::showEvent(QShowEvent *event)
{
    m_waitingForRelease = QApplication::mouseButtons().testFlag(Qt::LeftButton);
    QMenu::showEvent(event);
    m_buttons.first()->setFocus(Qt::PopupFocusReason);
}

bool TilingPopup::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        m_waitingForRelease = false;
    } else if (event->type() == QEvent::MouseButtonRelease && m_waitingForRelease) {
        // The release ending the long press must not select a zone.
        m_waitingForRelease = false;
        return true;
    } else if (event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            hide();
            return true;
        }
        const int code = key->key();
        if (code == Qt::Key_Return || code == Qt::Key_Enter) {
            if (auto *button = qobject_cast<QToolButton *>(object)) button->click();
            return true;
        }
        if (code == Qt::Key_Left || code == Qt::Key_Right || code == Qt::Key_Up || code == Qt::Key_Down) {
            const auto *current = qobject_cast<QToolButton *>(object);
            if (!current) return false;
            const QPoint origin = current->mapTo(this, current->rect().center());
            QToolButton *next = nullptr;
            qreal best = std::numeric_limits<qreal>::max();
            for (auto *candidate : m_buttons) {
                const QPoint delta = candidate->mapTo(this, candidate->rect().center()) - origin;
                const int forward = code == Qt::Key_Left ? -delta.x() : code == Qt::Key_Right ? delta.x()
                    : code == Qt::Key_Up ? -delta.y() : delta.y();
                const int sideways = (code == Qt::Key_Left || code == Qt::Key_Right) ? delta.y() : delta.x();
                const qreal score = forward * forward + 4.0 * sideways * sideways;
                if (forward > 0 && score < best) { best = score; next = candidate; }
            }
            if (next) next->setFocus(Qt::OtherFocusReason);
            return true;
        }
    }
    return QMenu::eventFilter(object, event);
}

void TilingPopup::mousePressEvent(QMouseEvent *event)
{
    m_waitingForRelease = false;
    QMenu::mousePressEvent(event);
}

void TilingPopup::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_waitingForRelease) {
        m_waitingForRelease = false;
        event->accept();
        return;
    }
    QMenu::mouseReleaseEvent(event);
}

}
