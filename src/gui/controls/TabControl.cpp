#include "TabControl.h"

#include "FormStyle.h"

#include <QPainterPath>

namespace controls {

void paintTab(QPainter &painter, const TabVisual &visual,
              const QFontMetricsF &metrics)
{
    const QRectF &rect = visual.rect;
    QPainterPath tab;
    tab.moveTo(rect.left(), rect.bottom());
    tab.lineTo(rect.left(), rect.top() + 3.0);
    tab.quadTo(rect.left(), rect.top(), rect.left() + 3.0, rect.top());
    tab.lineTo(rect.right() - 3.0, rect.top());
    tab.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + 3.0);
    tab.lineTo(rect.right(), rect.bottom());
    tab.closeSubpath();
    painter.setPen(FormStyle::fieldBorder);
    painter.setBrush(visual.selected ? QColor(255, 255, 255)
                                     : QColor(236, 236, 236));
    painter.drawPath(tab);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(FormStyle::text);
    const QString text = metrics.elidedText(visual.title, Qt::ElideRight,
                                            int(rect.width()) - 8);
    painter.drawText(rect.adjusted(4, 0, -4, 0), Qt::AlignCenter, text);
}

} // namespace controls
