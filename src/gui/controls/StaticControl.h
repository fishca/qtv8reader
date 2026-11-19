#pragma once

#include <QFontMetricsF>
#include <QPainter>
#include <QRectF>
#include <QString>

namespace controls {

// Простые статические элементы формы: надпись-декорация, разделитель
// командной панели и горизонтальная линия.
struct TextVisual
{
    QRectF rect;
    QString text;
};

void paintText(QPainter &painter, const TextVisual &visual,
               const QFontMetricsF &metrics);
void paintSeparator(QPainter &painter, const QRectF &rect);
void paintHorizontalLine(QPainter &painter, const QRectF &rect);

} // namespace controls
