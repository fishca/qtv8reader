#pragma once

#include <QPainter>
#include <QRectF>
#include <QString>

namespace controls {

// Строка поиска и кнопка с выпадающим списком в командной панели 1С.
struct SearchVisual
{
    QRectF rect;
    QString placeholder;
};

void paintSearch(QPainter &painter, const SearchVisual &visual);
void paintSearchOptions(QPainter &painter, const QRectF &rect);

} // namespace controls
