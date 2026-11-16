#include "gui/MxlView.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace v8reader {

namespace {

constexpr double kDpi = 96.0;
constexpr double kInchUnits = 288.0; // высота строки — 1/288 дюйма
constexpr double kColumnUnitsPerChar = 8.0; // ширина колонки — 1/8 символа
constexpr double kDefaultColumnUnits = 72.0; // 9 символов
constexpr int kMargin = 2;

double fontPixelSize(const QFont &font)
{
    const double points = font.pointSizeF() > 0 ? font.pointSizeF() : 10.0;
    return points * kDpi / 72.0;
}

} // namespace

MxlView::MxlView(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setBackgroundRole(QPalette::Base);
    viewport()->setBackgroundRole(QPalette::Base);
    viewport()->setAutoFillBackground(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setMessage(QStringLiteral("Табличный документ не выбран"));
}

void MxlView::setMessage(const QString &message)
{
    m_message = message;
    viewport()->update();
}

void MxlView::clear()
{
    m_document = mxl::Document();
    m_hasDocument = false;
    m_message = QStringLiteral("Табличный документ не выбран");
    m_columnLeft.clear();
    m_rowTop.clear();
    m_rowHeight.clear();
    m_contentWidth = 0.0;
    m_contentHeight = 0.0;
    m_maxColumn = 0;
    updateScrollBars();
    viewport()->update();
}

void MxlView::setDocument(const mxl::Document &document)
{
    m_document = document;
    m_hasDocument = document.valid;
    m_message.clear();
    rebuildLayout();
    updateScrollBars();
    viewport()->update();
}

int MxlView::rowCount() const
{
    return m_hasDocument ? m_document.rowCount() : 0;
}

int MxlView::columnCount() const
{
    return m_hasDocument && m_maxColumn > 0 ? m_maxColumn + 1 : 0;
}

void MxlView::setZoom(double zoom)
{
    const double clamped = std::clamp(zoom, 0.2, 5.0);
    if (std::abs(clamped - m_zoom) < 0.0001)
        return;
    m_zoom = clamped;
    updateScrollBars();
    viewport()->update();
}

void MxlView::zoomIn()
{
    setZoom(m_zoom * 1.25);
}

void MxlView::zoomOut()
{
    setZoom(m_zoom / 1.25);
}

void MxlView::resetZoom()
{
    setZoom(1.0);
}

QFont MxlView::baseFont() const
{
    QFont font(QStringLiteral("Arial"), 10);
    if (!m_document.fonts.isEmpty()) {
        const mxl::FontSpec &spec = m_document.fonts.first();
        if (!spec.family.isEmpty())
            font.setFamily(spec.family);
        if (spec.sizeTenthPt > 0)
            font.setPointSizeF(spec.sizeTenthPt / 10.0);
        font.setBold(spec.bold);
        font.setItalic(spec.italic);
        font.setUnderline(spec.underline);
        font.setStrikeOut(spec.strikeout);
    }
    return font;
}

QFont MxlView::fontForFormat(const mxl::CellFormat *format) const
{
    QFont font = baseFont();
    if (!format || format->font < 0 || format->font >= m_document.fonts.size())
        return font;

    const mxl::FontSpec &spec = m_document.fonts.at(format->font);
    if (!spec.family.isEmpty())
        font.setFamily(spec.family);
    if (spec.sizeTenthPt > 0)
        font.setPointSizeF(spec.sizeTenthPt / 10.0);
    font.setBold(spec.bold);
    font.setItalic(spec.italic);
    font.setUnderline(spec.underline);
    font.setStrikeOut(spec.strikeout);
    return font;
}

QColor MxlView::colorForIndex(int index, const QColor &fallback) const
{
    quint32 rgb = 0;
    if (index < 0 || !m_document.colorAt(index, &rgb))
        return fallback;
    return QColor(static_cast<int>(rgb & 0xFF),
                  static_cast<int>((rgb >> 8) & 0xFF),
                  static_cast<int>((rgb >> 16) & 0xFF));
}

double MxlView::columnUnits(int column) const
{
    const mxl::ColumnGroup &group = m_document.defaultGroup;
    for (const auto &entry : group.columns) {
        if (entry.first != column)
            continue;
        const mxl::CellFormat *format = m_document.formatAt(entry.second);
        if (format && format->width > 0)
            return format->width;
        break;
    }
    return kDefaultColumnUnits;
}

double MxlView::rowUnits(const mxl::Row &row) const
{
    const mxl::CellFormat *format = m_document.formatAt(row.format);
    if (format && format->height > 0)
        return format->height;
    return 0.0; // автоподбор
}

double MxlView::cellPixelWidth(int column) const
{
    QFontMetricsF metrics(baseFont());
    double characterWidth = metrics.horizontalAdvance(QLatin1Char('0'));
    if (characterWidth <= 0.0)
        characterWidth = metrics.averageCharWidth();
    if (characterWidth <= 0.0)
        characterWidth = 7.0;
    return columnUnits(column) / kColumnUnitsPerChar * characterWidth;
}

double MxlView::cellPixelHeight(int rowIndex) const
{
    for (const mxl::Row &row : m_document.rows) {
        if (row.index != rowIndex)
            continue;

        const double units = rowUnits(row);
        if (units > 0.0)
            return units / kInchUnits * kDpi;

        double fontHeight = fontPixelSize(baseFont()) * 1.35 + 4.0;
        const mxl::CellFormat *rowFormat = m_document.formatAt(row.format);
        if (rowFormat) {
            const QFont font = fontForFormat(rowFormat);
            fontHeight = std::max(fontHeight, fontPixelSize(font) * 1.35 + 4.0);
        }
        for (const auto &cell : row.cells) {
            const mxl::CellFormat *format = m_document.formatAt(cell.second.format);
            if (!format)
                continue;
            const QFont font = fontForFormat(format);
            fontHeight = std::max(fontHeight, fontPixelSize(font) * 1.35 + 4.0);
        }
        return fontHeight;
    }
    return fontPixelSize(baseFont()) * 1.35 + 4.0;
}

void MxlView::rebuildLayout()
{
    m_maxColumn = m_document.maxColumn();
    m_columnLeft.clear();
    m_rowTop.clear();
    m_rowHeight.clear();

    m_columnLeft.reserve(m_maxColumn + 2);
    double x = kMargin;
    for (int column = 0; column <= m_maxColumn; ++column) {
        m_columnLeft.append(x);
        x += cellPixelWidth(column);
    }
    m_columnLeft.append(x);
    m_contentWidth = x + kMargin;

    double y = kMargin;
    for (const mxl::Row &row : m_document.rows) {
        m_rowTop.insert(row.index, y);
        const double height = cellPixelHeight(row.index);
        m_rowHeight.insert(row.index, height);
        y += height;
    }
    m_contentHeight = y + kMargin;
}

void MxlView::updateScrollBars()
{
    if (m_hasDocument) {
        horizontalScrollBar()->setRange(
            0, std::max(0, static_cast<int>(std::ceil(m_contentWidth * m_zoom))
                               - viewport()->width()));
        verticalScrollBar()->setRange(
            0, std::max(0, static_cast<int>(std::ceil(m_contentHeight * m_zoom))
                               - viewport()->height()));
        horizontalScrollBar()->setPageStep(viewport()->width());
        verticalScrollBar()->setPageStep(viewport()->height());
        horizontalScrollBar()->setSingleStep(20);
        verticalScrollBar()->setSingleStep(20);
    } else {
        horizontalScrollBar()->setRange(0, 0);
        verticalScrollBar()->setRange(0, 0);
    }
}

void MxlView::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBars();
}

void MxlView::wheelEvent(QWheelEvent *event)
{
    if ((event->modifiers() & Qt::ControlModifier) != 0) {
        const int delta = event->angleDelta().y();
        if (delta > 0)
            zoomIn();
        else if (delta < 0)
            zoomOut();
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

void MxlView::drawCell(QPainter &painter, int rowIndex, int column,
                       const mxl::Merge *merge, const mxl::Cell *cell,
                       const mxl::CellFormat *format)
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

    const double x0 = m_columnLeft.at(left);
    const double x1 = m_columnLeft.at(right + 1);
    const double y0 = m_rowTop.value(top);
    const double y1 = m_rowTop.value(bottom) + m_rowHeight.value(bottom);
    QRectF rect(x0, y0, x1 - x0, y1 - y0);

    // Фон
    if (format) {
        QColor background = colorForIndex(format->bgColor, QColor());
        if (format->pattern >= 0 && format->pattern != 255) {
            const QColor patternColor = colorForIndex(format->patternColor, QColor());
            if (patternColor.isValid())
                background = patternColor;
            else if (format->pattern == 0 && background.isValid() == false)
                background = colorForIndex(format->patternColor, QColor(QStringLiteral("#D0D0D0")));
        }
        if (background.isValid())
            painter.fillRect(rect, background);
    }

    // Текст
    const QString text = cell ? cell->text : QString();
    if (!text.isEmpty()) {
        const QColor textColor = format
            ? colorForIndex(format->textColor, Qt::black)
            : QColor(Qt::black);
        QFont font = fontForFormat(format);
        painter.setFont(font);
        painter.setPen(textColor);

        const int hAlign = format ? format->hAlign : -1;
        int flags = Qt::AlignLeft;
        if (hAlign == 2)
            flags = Qt::AlignRight;
        else if (hAlign == 6)
            flags = Qt::AlignHCenter;
        else if (hAlign == 4)
            flags = Qt::AlignJustify;

        const int vAlign = format ? format->vAlign : -1;
        if (vAlign == 8)
            flags |= Qt::AlignBottom;
        else if (vAlign == 24)
            flags |= Qt::AlignVCenter;
        else
            flags |= Qt::AlignTop;

        const int textControl = format ? format->textControl : -1;
        if (textControl == 3)
            flags |= Qt::TextWordWrap;

        const bool rotated = format && format->orientation > 0
            && (format->orientation % 450 == 0 || format->orientation == 900);
        if (rotated) {
            painter.save();
            const QPointF center = rect.center();
            painter.translate(center);
            painter.rotate(format->orientation / 10.0);
            const QRectF rotatedRect(-rect.height() / 2, -rect.width() / 2,
                                     rect.height(), rect.width());
            painter.drawText(rotatedRect.adjusted(1, 1, -1, -1),
                             static_cast<int>(flags), text);
            painter.restore();
        } else {
            painter.drawText(rect.adjusted(1, 1, -1, -1),
                             static_cast<int>(flags), text);
        }
    }

    // Рамки
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
        pen.setWidthF(std::max(0.8, style.thickness * 0.6));
        switch (style.kind) {
        case 2: pen.setStyle(Qt::DotLine); break;
        case 3: pen.setStyle(Qt::SolidLine); pen.setWidthF(2.2); break;
        case 4: pen.setStyle(Qt::DashDotLine); break;
        case 5: pen.setStyle(Qt::DashLine); break;
        case 6: pen.setStyle(Qt::CustomDashLine); break;
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

void MxlView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), Qt::white);

    if (!m_hasDocument) {
        if (!m_message.isEmpty()) {
            painter.setPen(QColor(120, 120, 120));
            painter.drawText(viewport()->rect().adjusted(16, 16, -16, -16),
                             Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
                             m_message);
        }
        return;
    }

    painter.save();
    painter.scale(m_zoom, m_zoom);
    painter.translate(-horizontalScrollBar()->value() / m_zoom,
                      -verticalScrollBar()->value() / m_zoom);

    const double visibleTop = verticalScrollBar()->value() / m_zoom;
    const double visibleBottom = (verticalScrollBar()->value() + viewport()->height()) / m_zoom;
    const double visibleLeft = horizontalScrollBar()->value() / m_zoom;
    const double visibleRight = (horizontalScrollBar()->value() + viewport()->width()) / m_zoom;

    for (const mxl::Row &row : m_document.rows) {
        const double top = m_rowTop.value(row.index);
        const double bottom = top + m_rowHeight.value(row.index);
        if (bottom < visibleTop || top > visibleBottom)
            continue;

        const mxl::CellFormat *rowFormat = m_document.formatAt(row.format);
        for (const auto &cell : row.cells) {
            const int column = cell.first;
            if (column < 0 || column > m_maxColumn)
                continue;
            const double left = m_columnLeft.at(column);
            const double right = m_columnLeft.at(column + 1);
            if (right < visibleLeft || left > visibleRight)
                continue;

            const mxl::Merge *merge = m_document.mergeAt(row.index, column);
            if (merge && (merge->left != column || merge->top != row.index))
                continue;

            const mxl::CellFormat *format =
                m_document.formatAt(cell.second.format);
            if (!format)
                format = rowFormat;
            drawCell(painter, row.index, column, merge, &cell.second, format);
        }
    }

    // Объединения, у верхней-левой ячейки которых нет явной записи.
    for (const mxl::Merge &merge : m_document.merges) {
        const mxl::Cell *cell = m_document.cellAt(merge.top, merge.left);
        if (cell)
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

    painter.restore();
}

} // namespace v8reader
