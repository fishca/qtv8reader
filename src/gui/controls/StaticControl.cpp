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
