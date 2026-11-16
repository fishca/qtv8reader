#include "gui/MxlRenderer.h"

#include <QColor>
#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace v8reader {

namespace {

constexpr double kDpi = 96.0;
constexpr double kInchUnits = 288.0;
constexpr double kColumnUnitsPerChar = 8.0;
constexpr double kDefaultColumnUnits = 72.0;
constexpr int kMargin = 2;

double fontPixelSize(const QFont &font)
{
    if (font.pixelSize() > 0)
        return font.pixelSize();
    const double points = font.pointSizeF() > 0 ? font.pointSizeF() : 10.0;
    return points * kDpi / 72.0;
}

int pixelSizeForTenthPt(int sizeTenthPt)
{
    return std::max(1, qRound(sizeTenthPt / 10.0 * kDpi / 72.0));
}

} // namespace

MxlRenderer::MxlRenderer(const mxl::Document &document)
{
    setDocument(document);
}

void MxlRenderer::setDocument(const mxl::Document &document)
{
    m_document = document;
    if (m_document.valid)
        rebuildLayout();
    else {
        m_columnLeft.clear();
        m_rowTop.clear();
        m_rowHeight.clear();
        m_contentWidth = 0.0;
        m_contentHeight = 0.0;
        m_maxColumn = 0;
        m_maxRowIndex = 0;
    }
}

int MxlRenderer::rowCount() const
{
    return m_document.valid ? m_document.rowCount() : 0;
}

int MxlRenderer::columnCount() const
{
    return m_document.valid && m_maxColumn > 0 ? m_maxColumn + 1 : 0;
}

int MxlRenderer::maxRowIndex() const
{
    return m_maxRowIndex;
}

QFont MxlRenderer::baseFont() const
{
    QFont font(QStringLiteral("Arial"));
    font.setPixelSize(pixelSizeForTenthPt(100));
    if (!m_document.fonts.isEmpty()) {
        const mxl::FontSpec &spec = m_document.fonts.first();
        if (!spec.family.isEmpty())
            font.setFamily(spec.family);
        if (spec.sizeTenthPt > 0)
            font.setPixelSize(pixelSizeForTenthPt(spec.sizeTenthPt));
        font.setBold(spec.bold);
        font.setItalic(spec.italic);
        font.setUnderline(spec.underline);
        font.setStrikeOut(spec.strikeout);
    }
    return font;
}

QFont MxlRenderer::fontForFormat(const mxl::CellFormat *format) const
{
    QFont font = baseFont();
    if (!format || format->font < 0 || format->font >= m_document.fonts.size())
        return font;

    const mxl::FontSpec &spec = m_document.fonts.at(format->font);
    if (!spec.family.isEmpty())
        font.setFamily(spec.family);
    if (spec.sizeTenthPt > 0)
        font.setPixelSize(pixelSizeForTenthPt(spec.sizeTenthPt));
    font.setBold(spec.bold);
    font.setItalic(spec.italic);
    font.setUnderline(spec.underline);
    font.setStrikeOut(spec.strikeout);
    return font;
}

QColor MxlRenderer::colorForIndex(int index, const QColor &fallback) const
{
    quint32 rgb = 0;
    if (index < 0 || !m_document.colorAt(index, &rgb))
        return fallback;
    return QColor(static_cast<int>(rgb & 0xFF),
                  static_cast<int>((rgb >> 8) & 0xFF),
                  static_cast<int>((rgb >> 16) & 0xFF));
}

double MxlRenderer::columnWidth(int column) const
{
    if (column < 0 || column > m_maxColumn)
        return kDefaultColumnUnits / kColumnUnitsPerChar
            * QFontMetricsF(baseFont()).horizontalAdvance(QLatin1Char('0'));

    if (column + 1 < m_columnLeft.size())
        return m_columnLeft.at(column + 1) - m_columnLeft.at(column);
    return kDefaultColumnUnits / kColumnUnitsPerChar
        * QFontMetricsF(baseFont()).horizontalAdvance(QLatin1Char('0'));
}

double MxlRenderer::columnLeft(int column) const
{
    if (column < 0)
        return 0.0;
    if (column < m_columnLeft.size())
        return m_columnLeft.at(column);
    return m_columnLeft.isEmpty() ? 0.0 : m_columnLeft.last();
}

double MxlRenderer::rowTop(int rowIndex) const
{
    return m_rowTop.value(rowIndex, 0.0);
}

double MxlRenderer::rowHeight(int rowIndex) const
{
    return m_rowHeight.value(rowIndex, 20.0);
}

namespace {

double columnUnitsFor(const mxl::Document &document, int column)
{
    const mxl::ColumnGroup &group = document.defaultGroup;
    for (const auto &entry : group.columns) {
        if (entry.first != column)
            continue;
        const mxl::CellFormat *format = document.formatAt(entry.second);
        if (format && format->width > 0)
            return format->width;
        break;
    }
    return kDefaultColumnUnits;
}

double rowUnitsFor(const mxl::Document &document, int rowIndex)
{
    for (const mxl::Row &row : document.rows) {
        if (row.index != rowIndex)
            continue;
        const mxl::CellFormat *format = document.formatAt(row.format);
        if (format && format->height > 0)
            return format->height;
        break;
    }
    return 0.0;
}

} // namespace

void MxlRenderer::rebuildLayout()
{
    m_maxColumn = m_document.maxColumn();
    m_maxRowIndex = m_document.maxRowIndex();
    m_columnLeft.clear();
    m_rowTop.clear();
    m_rowHeight.clear();

    // Ширина колонки в 1С — в 1/8 ширины «стандартного символа»
    // (Arial 10), а не шрифта конкретного макета.
    QFont standardFont(QStringLiteral("Arial"));
    standardFont.setPixelSize(pixelSizeForTenthPt(100));
    QFontMetricsF metrics(standardFont);
    double characterWidth = metrics.horizontalAdvance(QLatin1Char('0'));
    if (characterWidth <= 0.0)
        characterWidth = metrics.averageCharWidth();
    if (characterWidth <= 0.0)
        characterWidth = 7.0;

    m_columnLeft.reserve(m_maxColumn + 2);
    double x = kMargin;
    for (int column = 0; column <= m_maxColumn; ++column) {
        m_columnLeft.append(x);
        const double units = columnUnitsFor(m_document, column);
        x += units / kColumnUnitsPerChar * characterWidth;
    }
    m_columnLeft.append(x);
    m_contentWidth = x + kMargin;

    double y = kMargin;
    for (const mxl::Row &row : m_document.rows) {
        m_rowTop.insert(row.index, y);
        const double units = rowUnitsFor(m_document, row.index);
        double height = units > 0.0 ? units / kInchUnits * kDpi : 0.0;
        if (height <= 0.0) {
            double maxHeight = QFontMetricsF(baseFont()).height();
            const mxl::CellFormat *rowFormat = m_document.formatAt(row.format);
            if (rowFormat) {
                maxHeight = std::max(
                    maxHeight, QFontMetricsF(fontForFormat(rowFormat)).height());
            }
            for (const auto &cell : row.cells) {
                const mxl::CellFormat *format = m_document.formatAt(cell.second.format);
                if (!format)
                    continue;
                const QFont font = fontForFormat(format);
                QFontMetricsF metrics(font);
                maxHeight = std::max(maxHeight, metrics.height());

                // Автовысота учитывает перенос текста (как в 1С).
                if (format->textControl != 3 || cell.second.text.isEmpty())
                    continue;
                const mxl::Merge *merge = m_document.mergeAt(row.index, cell.first);
                double width = 0.0;
                if (merge) {
                    width = columnLeft(merge->right + 1) - columnLeft(merge->left) - 4.0;
                } else {
                    width = columnLeft(cell.first + 1) - columnLeft(cell.first) - 4.0;
                }
                if (width <= 1.0)
                    continue;
                const QRectF bounds = metrics.boundingRect(
                    QRectF(0.0, 0.0, width, 100000.0),
                    Qt::TextWordWrap, cell.second.text);
                maxHeight = std::max(maxHeight, bounds.height() + 2.0);
            }
            height = std::max(14.0, std::ceil(maxHeight + 1.0));
        }
        m_rowHeight.insert(row.index, height);
        y += height;
    }
    m_contentHeight = y + kMargin;
}

QRectF MxlRenderer::namedAreaRect(const mxl::NamedArea &area) const
{
    if (!area.hasRows() && !area.hasColumns())
        return QRectF();

    double left = kMargin;
    double right = m_contentWidth - kMargin;
    double top = kMargin;
    double bottom = m_contentHeight - kMargin;

    if (area.hasColumns()) {
        left = columnLeft(std::max(0, area.left));
        right = columnLeft(area.right + 1);
    }
    if (area.hasRows()) {
        top = rowTop(area.top);
        bottom = rowTop(area.bottom) + rowHeight(area.bottom);
    }
    return QRectF(left, top, right - left, bottom - top);
}

void MxlRenderer::drawCell(QPainter &painter, int rowIndex, int column,
                           const mxl::Merge *merge, const mxl::Cell *cell,
                           const mxl::CellFormat *format) const
{
    int left = column;
    int top = rowIndex;
    int right = column;
    int bottom = rowIndex;
    if (merge) {
        left = merge->left;
        top = merge->top;
        right = merge->right;
        bottom = merge->bottom;
    }
    if (left < 0 || right >= m_columnLeft.size() - 1)
        return;
    if (!m_rowTop.contains(top) || !m_rowTop.contains(bottom))
        return;

    const double x0 = columnLeft(left);
    const double x1 = columnLeft(right + 1);
    const double y0 = rowTop(top);
    const double y1 = rowTop(bottom) + rowHeight(bottom);
    const QRectF rect(x0, y0, x1 - x0, y1 - y0);

    if (format) {
        QColor background = colorForIndex(format->bgColor, QColor());
        if (format->pattern >= 0 && format->pattern != 255) {
            const QColor patternColor = colorForIndex(format->patternColor, QColor());
            if (patternColor.isValid())
                background = patternColor;
        }
        if (background.isValid())
            painter.fillRect(rect, background);
    }

    const QString text = cell ? cell->text : QString();
    if (!text.isEmpty()) {
        const QColor textColor = format
            ? colorForIndex(format->textColor, Qt::black)
            : QColor(Qt::black);
        painter.setFont(fontForFormat(format));
        painter.setPen(textColor);

        int flags = Qt::AlignLeft;
        if (format) {
            if (format->hAlign == 2)
                flags = Qt::AlignRight;
            else if (format->hAlign == 6)
                flags = Qt::AlignHCenter;
            else if (format->hAlign == 4)
                flags = Qt::AlignJustify;

            if (format->vAlign == 8)
                flags |= Qt::AlignBottom;
            else if (format->vAlign == 24)
                flags |= Qt::AlignVCenter;
            else
                flags |= Qt::AlignTop;

            if (format->textControl == 3)
                flags |= Qt::TextWordWrap;
        } else {
            flags |= Qt::AlignTop;
        }

        QRectF textRect = rect.adjusted(1, 1, -1, -1);
        const int textControl = format ? format->textControl : -1;
        if (textControl <= 0) {
            // «Авто»: текст выходит за границу ячейки, пока справа пустые
            // ячейки (как в 1С). Прекращаем на первой непустой.
            double rightLimit = textRect.right();
            for (int c = right + 1; c <= m_maxColumn; ++c) {
                const mxl::Cell *neighbor = m_document.cellAt(rowIndex, c);
                if (neighbor && !neighbor->text.isEmpty())
                    break;
                rightLimit = columnLeft(c + 1);
            }
            if (rightLimit > textRect.right())
                textRect.setRight(rightLimit - 1.0);
        }

        const bool rotated = format && format->orientation > 0
            && (format->orientation % 450 == 0 || format->orientation == 900);
        if (rotated) {
            painter.save();
            painter.translate(rect.center());
            painter.rotate(format->orientation / 10.0);
            const QRectF rotatedRect(-rect.height() / 2.0, -rect.width() / 2.0,
                                     rect.height(), rect.width());
            painter.drawText(rotatedRect.adjusted(1, 1, -1, -1),
                             static_cast<int>(flags), text);
            painter.restore();
        } else {
            painter.drawText(textRect, static_cast<int>(flags), text);
        }
    }

    if (!format)
        return;
    const QColor borderColor = colorForIndex(format->bordersColor, QColor(Qt::black));
    const auto drawSide = [&](int lineIndex, const QLineF &line) {
        if (lineIndex < 0 || lineIndex >= m_document.lines.size())
            return;
        const mxl::LineStyle &style = m_document.lines.at(lineIndex);
        if (style.kind == 0 || style.thickness <= 0)
            return;

        QPen pen(borderColor);
        pen.setWidthF(std::max(1.0, style.thickness * 0.8));
        switch (style.kind) {
        case 2: pen.setStyle(Qt::DotLine); break;
        case 3: pen.setStyle(Qt::SolidLine); pen.setWidthF(2.2); break;
        case 4: pen.setStyle(Qt::DashLine); break;
        case 5: pen.setStyle(Qt::DashLine); pen.setWidthF(2.0); break;
        case 6: {
            pen.setStyle(Qt::CustomDashLine);
            pen.setWidthF(2.0);
            pen.setDashPattern({6.0, 3.0});
            break;
        }
        default: pen.setStyle(Qt::SolidLine); break;
        }
        painter.setPen(pen);
        painter.drawLine(line);
    };

    drawSide(format->borderLeft, QLineF(rect.left(), rect.top(), rect.left(), rect.bottom()));
    drawSide(format->borderTop, QLineF(rect.left(), rect.top(), rect.right(), rect.top()));
    drawSide(format->borderRight, QLineF(rect.right(), rect.top(), rect.right(), rect.bottom()));
    drawSide(format->borderBottom, QLineF(rect.left(), rect.bottom(), rect.right(), rect.bottom()));
}

void MxlRenderer::drawGrid(QPainter &painter, const QRectF &visible) const
{
    if (!m_document.valid)
        return;

    painter.save();
    QPen pen(QColor(200, 200, 200));
    pen.setWidthF(0.6);
    painter.setPen(pen);

    const double top = kMargin;
    const double bottom = m_contentHeight - kMargin;
    for (int column = 0; column <= m_maxColumn; ++column) {
        const double x = columnLeft(column);
        if (x < visible.left() - 1.0 || x > visible.right() + 1.0)
            continue;
        painter.drawLine(QPointF(x, top), QPointF(x, bottom));
    }

    const double left = kMargin;
    const double right = m_contentWidth - kMargin;
    for (const mxl::Row &row : m_document.rows) {
        const double y = rowTop(row.index);
        if (y < visible.top() - 1.0 || y > visible.bottom() + 1.0)
            continue;
        painter.drawLine(QPointF(left, y), QPointF(right, y));
    }
    if (!m_document.rows.isEmpty()) {
        const mxl::Row &last = m_document.rows.last();
        const double y = rowTop(last.index) + rowHeight(last.index);
        if (y >= visible.top() - 1.0 && y <= visible.bottom() + 1.0)
            painter.drawLine(QPointF(left, y), QPointF(right, y));
    }
    painter.restore();
}

void MxlRenderer::draw(QPainter &painter, const QRectF &visible, bool showNamedAreas,
                       const QString &activeArea) const
{
    if (!m_document.valid)
        return;

    for (const mxl::Row &row : m_document.rows) {
        const double top = rowTop(row.index);
        const double bottom = top + rowHeight(row.index);
        if (bottom < visible.top() || top > visible.bottom())
            continue;

        const mxl::CellFormat *rowFormat = m_document.formatAt(row.format);
        for (const auto &cell : row.cells) {
            const int column = cell.first;
            if (column < 0 || column > m_maxColumn)
                continue;
            const double left = columnLeft(column);
            const double right = columnLeft(column + 1);
            if (right < visible.left() || left > visible.right())
                continue;

            const mxl::Merge *merge = m_document.mergeAt(row.index, column);
            if (merge && (merge->left != column || merge->top != row.index))
                continue;

            const mxl::CellFormat *format = m_document.formatAt(cell.second.format);
            if (!format)
                format = rowFormat;
            drawCell(painter, row.index, column, merge, &cell.second, format);
        }
    }

    // Объединения, у верхней-левой ячейки которых нет явной записи.
    for (const mxl::Merge &merge : m_document.merges) {
        if (m_document.cellAt(merge.top, merge.left))
            continue;
        const mxl::CellFormat *format = nullptr;
        for (const mxl::Row &row : m_document.rows) {
            if (row.index == merge.top) {
                format = m_document.formatAt(row.format);
                break;
            }
        }
        drawCell(painter, merge.top, merge.left, &merge, nullptr, format);
    }

    if (showNamedAreas)
        drawNamedAreas(painter, visible, activeArea);
}

void MxlRenderer::drawNamedAreas(QPainter &painter, const QRectF &visible,
                                 const QString &activeArea) const
{
    if (m_document.namedAreas.isEmpty())
        return;

    painter.save();
    QFont labelFont = baseFont();
    labelFont.setPixelSize(11);
    labelFont.setBold(true);

    for (const mxl::NamedArea &area : m_document.namedAreas) {
        const QRectF rect = namedAreaRect(area);
        if (!rect.isValid() || rect.isEmpty())
            continue;
        if (!rect.intersects(visible))
            continue;

        const bool active = !activeArea.isEmpty()
            && area.name.compare(activeArea, Qt::CaseInsensitive) == 0;
        QColor fill = active ? QColor(255, 193, 7, 70) : QColor(33, 150, 243, 35);
        QColor border = active ? QColor(230, 150, 0) : QColor(33, 150, 243, 180);

        painter.fillRect(rect, fill);
        QPen pen(border);
        pen.setWidthF(active ? 1.8 : 1.2);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect);

        painter.setFont(labelFont);
        painter.setPen(active ? QColor(120, 70, 0) : QColor(20, 90, 160));
        const QFontMetricsF metrics(labelFont);
        const double textWidth = metrics.horizontalAdvance(area.name) + 8.0;
        const double textHeight = metrics.height() + 2.0;
        QRectF labelRect(rect.left() + 2.0, rect.top() + 1.0,
                         std::min(textWidth, std::max(rect.width() - 4.0, 10.0)),
                         textHeight);
        painter.fillRect(labelRect, QColor(255, 255, 255, 210));
        painter.drawText(labelRect.adjusted(3.0, 0.0, -2.0, 0.0),
                         Qt::AlignLeft | Qt::AlignVCenter, area.name);
    }
    painter.restore();
}

} // namespace v8reader
