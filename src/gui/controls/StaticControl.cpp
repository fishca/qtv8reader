#include "StaticControl.h"

#include "FormStyle.h"

#include <QPen>

namespace controls {

void paintText(QPainter &painter, const TextVisual &visual,
               const QFontMetricsF &metrics)
{
    painter.setPen(FormStyle::text);
    painter.drawText(visual.rect, Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(visual.text, Qt::ElideRight,
                                        int(visual.rect.width())));
}

void paintFilterChip(QPainter &painter, const ChipVisual &visual)
{
    const QRectF &rect = visual.rect;
    painter.setPen(QColor(214, 196, 128));
    painter.setBrush(QColor(255, 247, 214));
    painter.drawRoundedRect(rect, 3.0, 3.0);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(90, 74, 0));
    painter.drawText(rect.adjusted(9, 0, -24, 0),
                     Qt::AlignLeft | Qt::AlignVCenter, visual.text);
    const QPointF close(rect.right() - 12.0, rect.center().y());
    painter.setPen(QPen(QColor(160, 138, 58), 1.2));
    painter.drawEllipse(close, 5.5, 5.5);
    painter.drawLine(close + QPointF(-2.5, -2.5), close + QPointF(2.5, 2.5));
    painter.drawLine(close + QPointF(-2.5, 2.5), close + QPointF(2.5, -2.5));
}

void paintSeparator(QPainter &painter, const QRectF &rect)
{
    painter.setPen(QPen(FormStyle::separator, 1.0));
    painter.drawLine(rect.topLeft(), rect.bottomLeft());
}

void paintHorizontalLine(QPainter &painter, const QRectF &rect)
{
    painter.setPen(QPen(FormStyle::fieldBorder, 1.0));
    painter.drawLine(QPointF(rect.left(), rect.center().y()),
                     QPointF(rect.right(), rect.center().y()));
}

} // namespace controls
