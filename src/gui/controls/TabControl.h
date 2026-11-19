#pragma once

#include <QFontMetricsF>
#include <QPainter>
#include <QRectF>
#include <QString>

namespace controls {

// Закладка страницы формы 1С: скруглённая сверху, белая у выбранной.
struct TabVisual
{
    QRectF rect;
    QString title;
    bool selected = false;
};

void paintTab(QPainter &painter, const TabVisual &visual,
              const QFontMetricsF &metrics);

} // namespace controls
