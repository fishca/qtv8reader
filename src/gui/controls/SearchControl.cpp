#include "SearchControl.h"

#include "FormStyle.h"

#include <QLinearGradient>
#include <QPen>

namespace controls {

void paintSearch(QPainter &painter, const SearchVisual &visual)
{
    const QRectF &rect = visual.rect;
    painter.setPen(FormStyle::fieldBorder);
    painter.setBrush(Qt::white);
    painter.drawRect(rect);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(125, 125, 125));
    const double cy = rect.center().y();
    painter.drawText(rect.adjusted(8, 0, -26, 0),
                     Qt::AlignLeft | Qt::AlignVCenter, visual.placeholder);
    painter.setPen(QColor(110, 110, 110));
    // Очистка строки поиска.
    painter.drawLine(QPointF(rect.right() - 18.0, cy - 4.0),
                     QPointF(rect.right() - 10.0, cy + 4.0));
    painter.drawLine(QPointF(rect.right() - 18.0, cy + 4.0),
                     QPointF(rect.right() - 10.0, cy - 4.0));
}

void paintSearchOptions(QPainter &painter, const QRectF &rect)
{
    QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
    gradient.setColorAt(0.0, QColor(247, 247, 247));
    gradient.setColorAt(1.0, QColor(228, 228, 228));
    painter.setPen(FormStyle::fieldBorder);
    painter.setBrush(gradient);
    painter.drawRoundedRect(rect, 2.0, 2.0);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QColor(110, 110, 110));
    const double cy = rect.center().y();
    const double iconX = rect.left() + 13.0;
    painter.drawEllipse(QPointF(iconX, cy - 1.0), 4.0, 4.0);
    painter.drawLine(QPointF(iconX + 2.0, cy + 2.0),
                     QPointF(iconX + 5.0, cy + 5.0));
    painter.drawLine(QPointF(rect.right() - 9.0, cy - 2.0),
                     QPointF(rect.right() - 6.0, cy + 1.0));
    painter.drawLine(QPointF(rect.right() - 6.0, cy + 1.0),
                     QPointF(rect.right() - 3.0, cy - 2.0));
}

} // namespace controls
