#include "TabControl.h"

#include "FormStyle.h"

#include <QPainterPath>

namespace controls {

void paintTab(QPainter &painter, const TabVisual &visual,
              const QFontMetricsF &metrics)
{
    const QRectF &rect = visual.rect;
    // Вкладка страницы 8.3: плоская, со скруглением сверху; активная — белая и
    // без нижней кромки, чтобы сливаться с панелью страницы.
    QPainterPath tab;
    const double radius = 3.0;
    tab.moveTo(rect.left(), rect.bottom());
    tab.lineTo(rect.left(), rect.top() + radius);
    tab.quadTo(rect.left(), rect.top(), rect.left() + radius, rect.top());
    tab.lineTo(rect.right() - radius, rect.top());
    tab.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + radius);
    tab.lineTo(rect.right(), rect.bottom());
    tab.closeSubpath();

    painter.setPen(FormStyle::tabBorder);
    painter.setBrush(visual.selected ? QColor(255, 255, 255)
                                     : FormStyle::tabBackground);
    painter.drawPath(tab);
    if (visual.selected) {
        // Стираем нижнюю кромку активной вкладки — она переходит в панель.
        painter.setPen(QPen(QColor(255, 255, 255), 1.0));
        painter.drawLine(QPointF(rect.left() + 1.0, rect.bottom() - 0.5),
                         QPointF(rect.right() - 1.0, rect.bottom() - 0.5));
    } else {
        // Верхний светлый блик неактивной вкладки.
        painter.setPen(QPen(FormStyle::headerHighlight, 1.0));
        painter.drawLine(QPointF(rect.left() + radius, rect.top() + 0.5),
                         QPointF(rect.right() - radius, rect.top() + 0.5));
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(FormStyle::text);
    const QString text = metrics.elidedText(visual.title, Qt::ElideRight,
                                            int(rect.width()) - 14);
    painter.drawText(rect.adjusted(7, 0, -7, 0), Qt::AlignCenter, text);
}

} // namespace controls
