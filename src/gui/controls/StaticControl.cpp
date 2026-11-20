#include "StaticControl.h"

#include "FormStyle.h"

#include <QPen>

namespace controls {

void paintText(QPainter &painter, const TextVisual &visual,
               const QFontMetricsF &metrics)
{
    if (visual.closeGlyph) {
        const QPointF center = visual.rect.center();
        painter.setPen(QPen(QColor(120, 120, 120), 1.2));
        painter.drawLine(center + QPointF(-3.5, -3.5), center + QPointF(3.5, 3.5));
        painter.drawLine(center + QPointF(-3.5, 3.5), center + QPointF(3.5, -3.5));
        return;
    }
    if (visual.banner) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(252, 245, 198));
        painter.drawRoundedRect(visual.rect, 2.0, 2.0);
        painter.setBrush(Qt::NoBrush);
    }
    painter.setPen(FormStyle::text);
    // Текст баннера 1С не обрезает многоточием — он обязан быть виден целиком.
    if (visual.banner) {
        painter.drawText(visual.rect.adjusted(4, 0, -4, 0),
                         Qt::AlignLeft | Qt::AlignVCenter, visual.text);
        return;
    }
    painter.drawText(visual.rect.adjusted(2, 0, -2, 0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(visual.text, Qt::ElideRight,
                                        int(visual.rect.width())));
}

void paintFilterChip(QPainter &painter, const ChipVisual &visual)
{
    const QRectF &rect = visual.rect;
    painter.setPen(QColor(207, 192, 124));
    painter.setBrush(QColor(253, 245, 199));
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
