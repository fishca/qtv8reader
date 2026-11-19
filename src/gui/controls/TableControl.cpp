#include "TableControl.h"

#include "FormStyle.h"
#include "ScrollBarControl.h"

#include <QPen>
#include <QPolygonF>

#include <algorithm>

namespace controls {

void paintTable(QPainter &painter, const TableVisual &visual,
                const QFont &font, const QFontMetricsF &metrics)
{
    const QRectF &rect = visual.rect;
    const QStringList columns = visual.columns;
    if (columns.isEmpty())
        return;
    QVector<double> widths = visual.widths;
    if (widths.size() != columns.size()) {
        widths.clear();
        for (int i = 0; i < columns.size(); ++i)
            widths.append(rect.width() / columns.size());
    }
    const double headerHeight = FormStyle::tableHeaderHeight;
    const double verticalScrollWidth = 10.0;
    const double actionWidth = visual.actionColumn ? 37.0 : 0.0;
    const double contentWidth = std::max(1.0, rect.width() - verticalScrollWidth);
    double columnsTotal = actionWidth;
    for (double width : widths)
        columnsTotal += width;
    const bool horizontalScroll = columnsTotal > contentWidth + 0.5;
    const double scrollHeight = horizontalScroll ? 16.0 : 0.0;
    const double tableHeight = rect.height();

    const QRectF header(rect.left(), rect.top(), rect.width(), headerHeight);
    painter.setPen(FormStyle::tableBorder);
    painter.setBrush(FormStyle::headerBackground);
    painter.drawRect(header);
    painter.setBrush(Qt::NoBrush);

    QFont headerFont = font;
    painter.setFont(headerFont);
    painter.setPen(FormStyle::text);

    double columnX = rect.left();
    if (visual.actionColumn) {
        const QRectF cell(columnX, rect.top(), actionWidth, headerHeight);
        painter.setPen(FormStyle::gridLine);
        painter.drawLine(cell.topRight(), cell.bottomRight());
        painter.setPen(FormStyle::tableBorder);
        painter.drawLine(cell.bottomLeft(), cell.bottomRight());
        // Карандаш — команда редактирования строки в таблице конфигуратора.
        painter.setPen(QPen(QColor(110, 110, 110), 1.3));
        painter.drawLine(QPointF(cell.left() + 13.0, cell.bottom() - 8.0),
                         QPointF(cell.right() - 12.0, cell.top() + 8.0));
        painter.drawLine(QPointF(cell.left() + 13.0, cell.bottom() - 8.0),
                         QPointF(cell.left() + 17.0, cell.bottom() - 12.0));
        painter.drawLine(QPointF(cell.right() - 12.0, cell.top() + 8.0),
                         QPointF(cell.right() - 16.0, cell.top() + 12.0));
        columnX += actionWidth;
    }

    for (int i = 0; i < columns.size()
         && columnX < rect.left() + contentWidth; ++i) {
        const double width = widths.at(i);
        const QRectF cell(columnX, rect.top(), width, headerHeight);
        painter.setPen(FormStyle::gridLine);
        if (columnX + width < rect.left() + contentWidth)
            painter.drawLine(cell.topRight(), cell.bottomRight());
        painter.setPen(FormStyle::tableBorder);
        painter.drawLine(cell.bottomLeft(), cell.bottomRight());
        painter.setPen(FormStyle::text);
        const QString text = metrics.elidedText(columns.at(i), Qt::ElideRight,
                                                int(width) - 8);
        painter.drawText(cell.adjusted(12, 0, -6, 0),
                         Qt::AlignLeft | Qt::AlignVCenter, text);
        columnX += width;
    }
    painter.setFont(font);
    painter.setPen(FormStyle::tableBorder);
    painter.drawRect(header);

    // Пустая таблица конфигуратора — белое поле без строк.
    painter.setPen(FormStyle::tableBorder);
    painter.drawRect(QRectF(rect.left(), rect.top(), rect.width(), tableHeight));

    // Вертикальная полоса прокрутки вдоль правого края таблицы.
    const QRectF verticalTrack(rect.right() - 9.0,
                               rect.top() + headerHeight + 1.0, 8.0,
                               std::max(1.0, tableHeight - headerHeight
                                   - scrollHeight - 2.0));
    painter.fillRect(verticalTrack, QColor(248, 248, 248));
    const double verticalThumb = std::max(24.0, verticalTrack.height() * 0.45);
    painter.fillRect(QRectF(verticalTrack.left(), verticalTrack.top(),
                            verticalTrack.width(),
                            std::min(verticalThumb, verticalTrack.height())),
                     QColor(205, 205, 205));
    painter.setPen(QPen(QColor(160, 160, 160), 1.0));
    painter.drawLine(verticalTrack.topLeft(), verticalTrack.bottomLeft());

    if (horizontalScroll) {
        const double cornerWidth = 80.0;
        const QRectF track(rect.left() + 1.0,
                           rect.top() + tableHeight - scrollHeight + 1.0,
                           std::max(20.0, rect.width() - verticalScrollWidth
                               - cornerWidth - 2.0),
                           scrollHeight - 2.0);
        paintHorizontalScrollBar(painter, track, contentWidth / columnsTotal);

        // Уголок с кнопками прокрутки, как в конфигураторе.
        const QRectF corner(rect.right() - cornerWidth,
                            rect.top() + tableHeight - 20.0,
                            cornerWidth, 20.0);
        painter.fillRect(corner, QColor(240, 240, 240));
        painter.setPen(FormStyle::tableBorder);
        painter.drawRect(corner);
        for (int i = 0; i < 4; ++i) {
            const QRectF cell(corner.left() + i * 20.0, corner.top(), 20.0, 20.0);
            painter.setPen(FormStyle::tableBorder);
            painter.setBrush(QColor(240, 240, 240));
            painter.drawRect(cell);
            const QPointF c = cell.center();
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(120, 120, 120));
            QPolygonF arrow;
            switch (i) {
            case 0:
                arrow << QPointF(c.x() - 3.0, c.y() - 3.0)
                      << QPointF(c.x() - 3.0, c.y() + 3.0)
                      << QPointF(c.x() + 2.0, c.y());
                break;
            case 1:
                arrow << QPointF(c.x() - 3.0, c.y() + 2.0)
                      << QPointF(c.x() + 3.0, c.y() + 2.0)
                      << QPointF(c.x(), c.y() - 3.0);
                break;
            case 2:
                arrow << QPointF(c.x() - 3.0, c.y() - 2.0)
                      << QPointF(c.x() + 3.0, c.y() - 2.0)
                      << QPointF(c.x(), c.y() + 3.0);
                break;
            default:
                arrow << QPointF(c.x() + 3.0, c.y() - 3.0)
                      << QPointF(c.x() + 3.0, c.y() + 3.0)
                      << QPointF(c.x() - 2.0, c.y());
                break;
            }
            painter.drawPolygon(arrow);
        }
        painter.setBrush(Qt::NoBrush);
    }
}

} // namespace controls
