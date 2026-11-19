#pragma once

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QRectF>
#include <QStringList>
#include <QVector>

namespace controls {

// Таблица 1С: шапка, строки с точечной сеткой и полосы прокрутки.
struct TableVisual
{
    QRectF rect;
    QStringList columns;
    QVector<double> widths;
    bool actionColumn = false;
};

void paintTable(QPainter &painter, const TableVisual &visual,
                const QFont &font, const QFontMetricsF &metrics);

} // namespace controls
