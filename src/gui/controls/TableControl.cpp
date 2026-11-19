#include "TableControl.h"

#include "FormStyle.h"

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
        const double rowHeight = FormStyle::tableRowHeight;
        const double verticalScrollWidth = 10.0;
        const double contentWidth = std::max(1.0, rect.width() - verticalScrollWidth);
        double columnsTotal = 0.0;
        for (double width : widths)
            columnsTotal += width;
        const bool horizontalScroll = columnsTotal > contentWidth + 0.5;
        const double scrollHeight = horizontalScroll ? 10.0 : 0.0;
        const QRectF header(rect.left(), rect.top(), rect.width(), headerHeight);
        painter.setPen(FormStyle::tableBorder);
        painter.setBrush(FormStyle::headerBackground);
        painter.drawRect(header);
        painter.setBrush(Qt::NoBrush);
        QFont headerFont = font;
        painter.setFont(headerFont);
        painter.setPen(FormStyle::text);
        double columnX = rect.left();
        for (int i = 0; i < columns.size()
             && columnX < rect.left() + contentWidth; ++i) {
            const double width = widths.at(i);
            const QRectF cell(columnX, rect.top(), width, headerHeight);
            painter.setPen(FormStyle::gridLine);
            if (columnX + width < rect.left() + contentWidth) {
                painter.drawLine(cell.topRight(), cell.bottomRight());
            }
            painter.setPen(FormStyle::tableBorder);
            painter.drawLine(cell.bottomLeft(), cell.bottomRight());
            painter.setPen(FormStyle::text);
            const QString text = metrics.elidedText(columns.at(i), Qt::ElideRight,
                                                    int(width) - 8);
            painter.drawText(cell.adjusted(8, 0, -6, 0),
                             Qt::AlignLeft | Qt::AlignVCenter, text);
            // Индикатор сортировки, как у списка 1С: по дате/периоду.
            if (i == 0 && (text == QStringLiteral("Дата")
                           || text == QStringLiteral("Период"))) {
                const double arrowX = cell.right() - 10.0;
                const double top = cell.top() + 6.0;
                const double bottom = cell.bottom() - 6.0;
                painter.setPen(QPen(FormStyle::secondaryText, 1.2));
                painter.drawLine(QPointF(arrowX, top), QPointF(arrowX, bottom));
                painter.drawLine(QPointF(arrowX, bottom),
                                 QPointF(arrowX - 3.0, bottom - 3.5));
                painter.drawLine(QPointF(arrowX, bottom),
                                 QPointF(arrowX + 3.0, bottom - 3.5));
                painter.setPen(FormStyle::text);
            }
            columnX += width;
        }
        painter.setFont(font);
        painter.setPen(FormStyle::tableBorder);
        painter.drawRect(header);

        const int rows = std::max(1,
            int((rect.height() - headerHeight - scrollHeight) / rowHeight));
        for (int row = 0; row < rows; ++row) {
            const QRectF rowRect(rect.left(),
                                 rect.top() + headerHeight + row * rowHeight,
                                 contentWidth, rowHeight);
            if (row % 2 == 1) {
                painter.fillRect(rowRect, FormStyle::alternateRow);
            }
            painter.setPen(QPen(FormStyle::gridLine, 1.0, Qt::DotLine));
            painter.drawLine(rowRect.bottomLeft(), rowRect.bottomRight());
            painter.setPen(QPen(QColor(216, 216, 216), 1.0));
            double x = rect.left();
            for (int i = 0; i < widths.size() && x < rowRect.right(); ++i) {
                painter.drawLine(QPointF(x, rowRect.top()), QPointF(x, rowRect.bottom()));
                x += widths.at(i);
            }
        }
        const double tableHeight = std::min(rect.height(),
            headerHeight + rows * rowHeight + scrollHeight);
        painter.setPen(FormStyle::tableBorder);
        painter.drawRect(QRectF(rect.left(), rect.top(), rect.width(), tableHeight));

        // Вертикальная полоса прокрутки списка.
        const QRectF verticalTrack(rect.right() - verticalScrollWidth + 1.0,
                                   rect.top() + headerHeight + 1.0,
                                   verticalScrollWidth - 2.0,
                                   std::max(1.0, tableHeight - headerHeight
                                       - scrollHeight - 2.0));
        painter.fillRect(verticalTrack, QColor(241, 241, 241));
        const double verticalThumb = std::max(24.0,
            verticalTrack.height() * 0.45);
        painter.fillRect(QRectF(verticalTrack.left(), verticalTrack.top(),
                                verticalTrack.width(),
                                std::min(verticalThumb, verticalTrack.height())),
                         QColor(193, 193, 193));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(120, 120, 120));
        const double arrowCenterX = verticalTrack.center().x();
        QPolygonF upArrow;
        upArrow << QPointF(arrowCenterX - 2.5, verticalTrack.top() + 4.0)
                << QPointF(arrowCenterX + 2.5, verticalTrack.top() + 4.0)
                << QPointF(arrowCenterX, verticalTrack.top() + 7.0);
        painter.drawPolygon(upArrow);
        QPolygonF downArrow;
        downArrow << QPointF(arrowCenterX - 2.5, verticalTrack.bottom() - 4.0)
                  << QPointF(arrowCenterX + 2.5, verticalTrack.bottom() - 4.0)
                  << QPointF(arrowCenterX, verticalTrack.bottom() - 7.0);
        painter.drawPolygon(downArrow);

        if (horizontalScroll) {
            // Уголок между полосами прокрутки.
            painter.fillRect(QRectF(rect.right() - verticalScrollWidth + 1.0,
                                    rect.bottom() - scrollHeight + 1.0,
                                    verticalScrollWidth - 2.0, scrollHeight - 2.0),
                             QColor(241, 241, 241));
            const QRectF track(rect.left() + 1.0,
                               rect.top() + tableHeight - scrollHeight + 1.0,
                               contentWidth - 2.0, scrollHeight - 2.0);
            painter.fillRect(track, QColor(241, 241, 241));
            const double thumb = std::max(20.0,
                track.width() * contentWidth / columnsTotal - 20.0);
            painter.fillRect(QRectF(track.left() + 10.0, track.top(),
                                    std::min(thumb, track.width() - 20.0),
                                    track.height()),
                             QColor(193, 193, 193));
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(120, 120, 120));
            const double arrowY = track.center().y();
            QPolygonF leftArrow;
            leftArrow << QPointF(track.left() + 7.0, arrowY - 3.0)
                      << QPointF(track.left() + 7.0, arrowY + 3.0)
                      << QPointF(track.left() + 3.0, arrowY);
            painter.drawPolygon(leftArrow);
            QPolygonF rightArrow;
            rightArrow << QPointF(track.right() - 7.0, arrowY - 3.0)
                       << QPointF(track.right() - 7.0, arrowY + 3.0)
                       << QPointF(track.right() - 3.0, arrowY);
            painter.drawPolygon(rightArrow);
        }
}

} // namespace controls
