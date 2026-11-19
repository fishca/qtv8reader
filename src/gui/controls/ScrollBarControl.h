#pragma once

#include <QPainter>
#include <QRectF>

namespace controls {

// Полосы прокрутки 1С: светлый трек, серый бегунок и стрелки по краям.
void paintVerticalScrollBar(QPainter &painter, const QRectF &track);
void paintHorizontalScrollBar(QPainter &painter, const QRectF &track,
                              double visibleFraction);

} // namespace controls
