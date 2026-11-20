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
    // Таблица конфигуратора показывает вертикальную прокрутку справа.
    const double verticalScrollWidth = rect.height() > headerHeight + 80.0
        ? 14.0 : 0.0;
    const double actionWidth = visual.actionColumn ? 37.0 : 0.0;
    const double contentWidth = std::max(1.0, rect.width() - verticalScrollWidth);
    double columnsTotal = actionWidth;
    for (double width : widths)
        columnsTotal += width;
    const bool horizontalScroll = columnsTotal > contentWidth + 0.5;
    const double scrollHeight = horizontalScroll ? 16.0 : 0.0;
    const double tableHeight = rect.height();

    const QRectF header(rect.left(), rect.top(), rect.width(), headerHeight);
    // Шапка таблицы 8.3: светло-серая с белой внутренней подсветкой сверху и
    // слева, снизу — разделитель #ccc.
    painter.fillRect(header, FormStyle::headerBackground);
    painter.setPen(FormStyle::headerHighlight);
    painter.drawLine(QPointF(header.left() + 1.0, header.top() + 0.5),
                     QPointF(header.right() - 1.0, header.top() + 0.5));
    painter.drawLine(QPointF(header.left() + 0.5, header.top() + 1.0),
                     QPointF(header.left() + 0.5, header.bottom() - 1.0));
    painter.setPen(QColor(204, 204, 204));
    painter.drawLine(QPointF(header.left(), header.bottom() - 0.5),
                     QPointF(header.right(), header.bottom() - 0.5));

    QFont headerFont = font;
    headerFont.setPointSizeF(9.0);
    headerFont.setBold(false);
    painter.setFont(headerFont);
    painter.setPen(FormStyle::text);

    double columnX = rect.left();
    if (visual.actionColumn) {
        const QRectF cell(columnX, rect.top(), actionWidth, headerHeight);
        painter.setPen(FormStyle::gridLine);
        painter.drawLine(cell.topRight(), cell.bottomRight());
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
        painter.setPen(FormStyle::text);
        const QString text = metrics.elidedText(columns.at(i), Qt::ElideRight,
                                                int(width) - 8);
        painter.drawText(cell.adjusted(6, 0, -6, 0),
                         Qt::AlignLeft | Qt::AlignVCenter, text);
        columnX += width;
    }
    painter.setFont(font);

    // Пустая таблица конфигуратора — белое поле без строк.
    painter.fillRect(QRectF(rect.left(), rect.top() + headerHeight,
                            rect.width(), tableHeight - headerHeight),
                     Qt::white);
    painter.setPen(FormStyle::tableBorder);
    painter.drawRect(QRectF(rect.left(), rect.top(), rect.width(), tableHeight));

    if (horizontalScroll) {
        const double cornerWidth = 96.0;
        const QRectF track(rect.left() + 1.0,
                           rect.top() + tableHeight - scrollHeight + 1.0,
                           std::max(20.0, rect.width() - verticalScrollWidth
                               - cornerWidth - 21.0),
                           scrollHeight - 2.0);
        paintHorizontalScrollBar(painter, track, (contentWidth / columnsTotal) * 0.78);

        // Уголок с кнопками прокрутки, как в конфигураторе.
        const QRectF corner(rect.right() - cornerWidth - 19.0,
                            rect.top() + tableHeight - 20.0,
                            cornerWidth, 20.0);
        painter.fillRect(corner, QColor(240, 240, 240));
        painter.setPen(QColor(128, 128, 128));
        painter.drawRect(corner);
        for (int i = 0; i < 4; ++i) {
            const QRectF cell(corner.left() + i * 24.0, corner.top(), 24.0, 20.0);
            QLinearGradient cellGradient(cell.topLeft(), cell.bottomLeft());
            cellGradient.setColorAt(0.0, QColor(250, 250, 250));
            cellGradient.setColorAt(1.0, QColor(232, 232, 232));
            painter.setPen(QColor(128, 128, 128));
            painter.setBrush(cellGradient);
            painter.drawRect(cell);
            const QPointF c = cell.center();
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(90, 90, 90));
            QPolygonF arrow;
            switch (i) {
            case 0:
                arrow << QPointF(c.x() - 3.0, c.y() + 2.0)
                      << QPointF(c.x() + 3.0, c.y() + 2.0)
                      << QPointF(c.x(), c.y() - 3.0);
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
                arrow << QPointF(c.x() - 3.0, c.y() - 2.0)
                      << QPointF(c.x() + 3.0, c.y() - 2.0)
                      << QPointF(c.x(), c.y() + 3.0);
                break;
            }
            painter.drawPolygon(arrow);
            if (i == 0 || i == 3) {
                painter.setPen(QPen(QColor(90, 90, 90), 1.2));
                const double barY = i == 0 ? c.y() - 4.5 : c.y() + 4.5;
                painter.drawLine(QPointF(c.x() - 4.5, barY),
                                 QPointF(c.x() + 4.5, barY));
            }
        }
        painter.setBrush(Qt::NoBrush);
    }

    if (verticalScrollWidth > 0.0) {
        const double bottomInset = horizontalScroll ? 16.0 : 0.0;
        const QRectF track(rect.right() - verticalScrollWidth + 1.0,
                           rect.top() + headerHeight + 1.0,
                           verticalScrollWidth - 2.0,
                           std::max(20.0, tableHeight - headerHeight
                                              - bottomInset - 2.0));
        paintVerticalScrollBar(painter, track);
    }
}

} // namespace controls
