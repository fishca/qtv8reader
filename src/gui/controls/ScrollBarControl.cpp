#include "ScrollBarControl.h"

#include <QPen>
#include <QPolygonF>

#include <algorithm>

namespace controls {

void paintVerticalScrollBar(QPainter &painter, const QRectF &track)
{
    painter.fillRect(track, QColor(241, 241, 241));
    const double thumb = std::max(24.0, track.height() * 0.45);
    painter.fillRect(QRectF(track.left(), track.top(), track.width(),
                            std::min(thumb, track.height())),
                     QColor(193, 193, 193));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(120, 120, 120));
    const double centerX = track.center().x();
    QPolygonF upArrow;
    upArrow << QPointF(centerX - 2.5, track.top() + 4.0)
            << QPointF(centerX + 2.5, track.top() + 4.0)
            << QPointF(centerX, track.top() + 7.0);
    painter.drawPolygon(upArrow);
    QPolygonF downArrow;
    downArrow << QPointF(centerX - 2.5, track.bottom() - 4.0)
              << QPointF(centerX + 2.5, track.bottom() - 4.0)
              << QPointF(centerX, track.bottom() - 7.0);
    painter.drawPolygon(downArrow);
}

void paintHorizontalScrollBar(QPainter &painter, const QRectF &track,
                              double visibleFraction)
{
    painter.fillRect(track, QColor(241, 241, 241));
    const double thumb = std::max(20.0,
        track.width() * visibleFraction - 20.0);
    painter.fillRect(QRectF(track.left() + 10.0, track.top(),
                            std::min(thumb, track.width() - 20.0),
                            track.height()),
                     QColor(193, 193, 193));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(120, 120, 120));
    const double arrowY = track.center().y();
    QPolygonF leftArrow;
    leftArrow << QPointF(track.left() + 7.0, arrowY - 3.0)
              << QPointF(track.left() + 7.0, arrowY + 3.0)
              << QPointF(track.left() + 3.0, arrowY);
    painter.drawPolygon(leftArrow);
    QPolygonF rightArrow;
    rightArrow << QPointF(track.right() - 7.0, arrowY - 3.0)
               << QPointF(track.right() - 7.0, arrowY + 3.0)
               << QPointF(track.right() - 3.0, arrowY);
    painter.drawPolygon(rightArrow);
}

} // namespace controls
