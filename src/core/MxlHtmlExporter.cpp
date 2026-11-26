#include "core/MxlHtmlExporter.h"

#include <QHash>
#include <QJsonArray>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace v8reader {
namespace mxl {
namespace {

// Ширина колонки в MXL — в 1/8 ширины «стандартного символа» (Arial 10).
// В браузере точные метрики шрифта недоступны, поэтому берём ширину цифры
// Arial 10 pt при 96 dpi: 13.33 px * 0.556 ≈ 7.4 px.
constexpr double kColumnUnitsPerChar = 8.0;
constexpr double kDefaultColumnUnits = 72.0;
constexpr double kCharWidthPx = 7.4;
constexpr double kDefaultFontTenthPt = 100.0;
constexpr double kRowUnitsPerInch = 288.0;
constexpr double kDpi = 96.0;
constexpr double kMinColumnPx = 6.0;
constexpr double kMinRowPx = 14.0;
constexpr int kSheetPadding = 12;

QString escapeHtml(const QString &text)
{
    QString result;
    result.reserve(text.size() + 16);
    for (const QChar ch : text) {
        switch (ch.unicode()) {
        case u'&':
            result += QLatin1String("&amp;");
            break;
        case u'<':
            result += QLatin1String("&lt;");
            break;
        case u'>':
            result += QLatin1String("&gt;");
            break;
        case u'"':
            result += QLatin1String("&quot;");
            break;
        case u'\'':
            result += QLatin1String("&#39;");
            break;
        case u'\n':
            result += QLatin1String("<br>");
            break;
        case u'\r':
            break;
        default:
            result += ch;
            break;
        }
    }
    return result;
}

QString px(double value)
{
    return QStringLiteral("%1px").arg(qRound(value));
}

QString colorHex(const Document &document, int index)
{
    quint32 rgb = 0;
    if (index < 0 || !document.colorAt(index, &rgb))
        return QString();
    const int red = static_cast<int>(rgb & 0xFF);
    const int green = static_cast<int>((rgb >> 8) & 0xFF);
    const int blue = static_cast<int>((rgb >> 16) & 0xFF);
    return QString::asprintf("#%02x%02x%02x", red, green, blue);
}

double pxForTenthPt(int tenthPt)
{
    return std::max(1.0, tenthPt / 10.0 * kDpi / 72.0);
}

QString cssFontFamily(const QString &family)
{
    QString clean = family;
    // Значение попадает в атрибут style="…", поэтому кавычки в имени
    // шрифта недопустимы: в CSS используем одинарные.
    clean.remove(QLatin1Char('"'));
    clean.remove(QLatin1Char('\''));
    clean.remove(QLatin1Char('\\'));
    clean.remove(QLatin1Char('\n'));
    clean.remove(QLatin1Char('\r'));
    clean = clean.trimmed();
    if (clean.isEmpty())
        clean = QStringLiteral("Arial");
    return QStringLiteral("'%1', Arial, Helvetica, sans-serif").arg(clean);
}

QString borderStyleName(int kind)
{
    switch (kind) {
    case 2:
        return QStringLiteral("dotted");
    case 3:
        return QStringLiteral("double");
    case 4:
    case 5:
    case 6:
        return QStringLiteral("dashed");
    default:
        return QStringLiteral("solid");
    }
}

// CSS-рамка стороны ячейки; пустая строка — стороны нет.
QString cssBorder(const Document &document, int lineIndex, int colorIndex)
{
    if (lineIndex < 0 || lineIndex >= document.lines.size())
        return QString();
    const LineStyle &line = document.lines.at(lineIndex);
    if (line.kind == 0)
        return QString();

    double width = std::max(1.0, line.thickness * 0.8);
    if (line.kind == 3)
        width = std::max(3.0, width); // двойную рамку рисуют только от 3 px
    QString color = colorHex(document, colorIndex);
    if (color.isEmpty())
        color = QStringLiteral("#000000");
    return QStringLiteral("border %1 %2 %3")
        .arg(px(width), borderStyleName(line.kind), color);
}

// Ширина колонки в единицах формата (1/8 стандартного символа).
double columnUnitsFor(const Document &document, int column)
{
    const ColumnGroup &group = document.defaultGroup;
    for (const auto &entry : group.columns) {
        if (entry.first != column)
            continue;
        const CellFormat *format = document.formatAt(entry.second);
        if (format && format->width > 0)
            return format->width;
        break;
    }
    return kDefaultColumnUnits;
}

QString buildCellStyle(const Document &document, const CellFormat *format)
{
    QStringList declarations;
    declarations << QStringLiteral("padding:1px 2px");

    int fontIndex = format ? format->font : -1;
    if (fontIndex < 0 || fontIndex >= document.fonts.size())
        fontIndex = document.fonts.isEmpty() ? -1 : 0;

    if (fontIndex >= 0) {
        const FontSpec &font = document.fonts.at(fontIndex);
        declarations << QStringLiteral("font-family:%1").arg(cssFontFamily(font.family));
        declarations << QStringLiteral("font-size:%1")
                            .arg(px(pxForTenthPt(font.sizeTenthPt > 0 ? font.sizeTenthPt
                                                                     : static_cast<int>(
                                                                           kDefaultFontTenthPt))));
        if (font.bold)
            declarations << QStringLiteral("font-weight:bold");
        if (font.italic)
            declarations << QStringLiteral("font-style:italic");
        QStringList decorations;
        if (font.underline)
            decorations << QStringLiteral("underline");
        if (font.strikeout)
            decorations << QStringLiteral("line-through");
        if (!decorations.isEmpty())
            declarations << QStringLiteral("text-decoration:%1")
                                .arg(decorations.join(QLatin1Char(' ')));
    } else {
        declarations << QStringLiteral("font-family:Arial, Helvetica, sans-serif");
        declarations << QStringLiteral("font-size:%1").arg(px(pxForTenthPt(100)));
    }

    const int textColor = format ? format->textColor : -1;
    QString textHex = colorHex(document, textColor);
    declarations << QStringLiteral("color:%1")
                        .arg(textHex.isEmpty() ? QStringLiteral("#000000") : textHex);

    QString background = colorHex(document, format ? format->bgColor : -1);
    if (format && format->pattern >= 0 && format->pattern != 255) {
        const QString pattern = colorHex(document, format->patternColor);
        if (!pattern.isEmpty())
            background = pattern;
    }
    if (!background.isEmpty())
        declarations << QStringLiteral("background:%1").arg(background);

    if (format) {
        QString left = cssBorder(document, format->borderLeft, format->bordersColor);
        QString top = cssBorder(document, format->borderTop, format->bordersColor);
        QString right = cssBorder(document, format->borderRight, format->bordersColor);
        QString bottom = cssBorder(document, format->borderBottom, format->bordersColor);
        if (!left.isEmpty())
            declarations << QStringLiteral("border-left:%1").arg(left.mid(7));
        if (!top.isEmpty())
            declarations << QStringLiteral("border-top:%1").arg(top.mid(7));
        if (!right.isEmpty())
            declarations << QStringLiteral("border-right:%1").arg(right.mid(7));
        if (!bottom.isEmpty())
            declarations << QStringLiteral("border-bottom:%1").arg(bottom.mid(7));
    }

    const int hAlign = format ? format->hAlign : -1;
    QString align = QStringLiteral("left");
    if (hAlign == 2)
        align = QStringLiteral("right");
    else if (hAlign == 6)
        align = QStringLiteral("center");
    else if (hAlign == 4)
        align = QStringLiteral("justify");
    declarations << QStringLiteral("text-align:%1").arg(align);

    const int vAlign = format ? format->vAlign : -1;
    QString vertical = QStringLiteral("top");
    if (vAlign == 8)
        vertical = QStringLiteral("bottom");
    else if (vAlign == 24)
        vertical = QStringLiteral("middle");
    declarations << QStringLiteral("vertical-align:%1").arg(vertical);

    const int textControl = format ? format->textControl : -1;
    if (textControl == 3) {
        declarations << QStringLiteral("white-space:pre-wrap");
        declarations << QStringLiteral("overflow-wrap:break-word");
    } else if (textControl == 1) {
        declarations << QStringLiteral("white-space:pre");
        declarations << QStringLiteral("overflow:hidden");
    } else {
        // «Авто» и «забивать»: текст не переносится. Выход за границу
        // соседних пустых ячеек не повторяем — в HTML он неотличим
        // от испорченной рамки.
        declarations << QStringLiteral("white-space:pre");
    }

    return declarations.join(QLatin1Char(';'));
}

struct RowMetric
{
    double top = 0.0;
    double height = kMinRowPx;
    double fixedHeight = 0.0; // > 0 — высота задана форматом строки
};

struct Grid
{
    int columns = 0;
    int rows = 0;
    QVector<int> rowIndex;        // порядковый номер -> номер строки документа
    QVector<RowMetric> metrics;
    QVector<double> columnLeft;   // columns + 1 границ
    QVector<QVector<int>> mergeId; // [строка][колонка] -> индекс в merges
    QVector<const Merge *> merges;
    QVector<int> mergeTop;
    QVector<int> mergeLeft;
    QVector<int> colspan;
    QVector<int> rowspan;
    double width = 0.0;
    double height = 0.0;
};

int seqForIndex(const Grid &grid, int rowIndex)
{
    const auto it = std::lower_bound(grid.rowIndex.cbegin(), grid.rowIndex.cend(), rowIndex);
    if (it == grid.rowIndex.cend() || *it != rowIndex)
        return -1;
    return static_cast<int>(it - grid.rowIndex.cbegin());
}

double estimatedRowHeight(const Document &document, const Row &row)
{
    double height = kMinRowPx;
    const auto bump = [&](const CellFormat *format) {
        int fontIndex = format ? format->font : -1;
        if (fontIndex < 0 || fontIndex >= document.fonts.size())
            fontIndex = document.fonts.isEmpty() ? -1 : 0;
        if (fontIndex < 0)
            return;
        const FontSpec &font = document.fonts.at(fontIndex);
        const double sizePx = font.sizeTenthPt > 0
            ? pxForTenthPt(font.sizeTenthPt)
            : pxForTenthPt(static_cast<int>(kDefaultFontTenthPt));
        height = std::max(height, sizePx * 1.35 + 2.0);
    };
    bump(document.formatAt(row.format));
    for (const auto &cell : row.cells)
        bump(document.formatAt(cell.second.format));
    return std::ceil(height);
}

Grid buildGrid(const Document &document)
{
    Grid grid;
    grid.columns = std::max(0, document.maxColumn() + 1);

    for (const Row &row : document.rows)
        grid.rowIndex.append(row.index);
    std::sort(grid.rowIndex.begin(), grid.rowIndex.end());
    grid.rowIndex.erase(std::unique(grid.rowIndex.begin(), grid.rowIndex.end()),
                        grid.rowIndex.end());
    grid.rows = static_cast<int>(grid.rowIndex.size());

    double left = 0.0;
    grid.columnLeft.reserve(grid.columns + 1);
    for (int column = 0; column < grid.columns; ++column) {
        grid.columnLeft.append(left);
        const double units = columnUnitsFor(document, column);
        left += std::max(kMinColumnPx, units / kColumnUnitsPerChar * kCharWidthPx);
    }
    grid.columnLeft.append(left);
    grid.width = left;

    QHash<int, const Row *> rowsByIndex;
    for (const Row &row : document.rows)
        rowsByIndex.insert(row.index, &row);

    double top = 0.0;
    grid.metrics.reserve(grid.rows);
    for (const int index : grid.rowIndex) {
        RowMetric metric;
        const Row *row = rowsByIndex.value(index, nullptr);
        const CellFormat *rowFormat = row ? document.formatAt(row->format) : nullptr;
        if (rowFormat && rowFormat->height > 0) {
            metric.fixedHeight = rowFormat->height / kRowUnitsPerInch * kDpi;
            metric.height = std::max(kMinRowPx, metric.fixedHeight);
        } else if (row) {
            metric.height = std::max(kMinRowPx, estimatedRowHeight(document, *row));
        }
        metric.top = top;
        top += metric.height;
        grid.metrics.append(metric);
    }
    grid.height = top;

    grid.mergeId.resize(grid.rows);
    for (QVector<int> &line : grid.mergeId)
        line = QVector<int>(grid.columns, -1);

    for (const Merge &merge : document.merges) {
        if (grid.columns <= 0)
            break;
        if (merge.left < 0 || merge.right < merge.left || merge.left >= grid.columns)
            continue;
        const int topSeq = seqForIndex(grid, merge.top);
        if (topSeq < 0)
            continue;
        int bottomSeq = seqForIndex(grid, merge.bottom);
        if (bottomSeq < topSeq)
            bottomSeq = topSeq;
        const int right = std::min(merge.right, grid.columns - 1);
        const int id = static_cast<int>(grid.merges.size());
        grid.merges.append(&merge);
        grid.mergeTop.append(topSeq);
        grid.mergeLeft.append(merge.left);
        grid.colspan.append(right - merge.left + 1);
        grid.rowspan.append(bottomSeq - topSeq + 1);
        for (int seq = topSeq; seq <= bottomSeq; ++seq) {
            for (int column = merge.left; column <= right; ++column) {
                if (grid.mergeId.at(seq).at(column) < 0)
                    grid.mergeId[seq][column] = id;
            }
        }
    }
    return grid;
}

double topForRow(const Grid &grid, int rowIndex)
{
    const auto it = std::lower_bound(grid.rowIndex.cbegin(), grid.rowIndex.cend(), rowIndex);
    if (it == grid.rowIndex.cend())
        return grid.height;
    return grid.metrics.at(static_cast<int>(it - grid.rowIndex.cbegin())).top;
}

double bottomForRow(const Grid &grid, int rowIndex)
{
    const auto it = std::upper_bound(grid.rowIndex.cbegin(), grid.rowIndex.cend(), rowIndex);
    if (it == grid.rowIndex.cbegin())
        return 0.0;
    const int seq = static_cast<int>(it - grid.rowIndex.cbegin()) - 1;
    return grid.metrics.at(seq).top + grid.metrics.at(seq).height;
}

QString buildAreasHtml(const Document &document, const Grid &grid)
{
    if (document.namedAreas.isEmpty() || grid.rows == 0 || grid.columns == 0)
        return QString();

    QStringList blocks;
    for (const NamedArea &area : document.namedAreas) {
        double top = 0.0;
        double bottom = grid.height;
        if (area.hasRows()) {
            top = topForRow(grid, area.top);
            bottom = bottomForRow(grid, area.bottom);
        }
        double left = 0.0;
        double right = grid.width;
        if (area.hasColumns()) {
            left = grid.columnLeft.at(std::clamp(area.left, 0, grid.columns));
            right = grid.columnLeft.at(std::clamp(area.right + 1, 0, grid.columns));
        }
        if (right <= left || bottom <= top)
            continue;
        blocks << QStringLiteral("<div class=\"area\" style=\"left:%1;top:%2;width:%3;height:%4\">"
                                "<span>%5</span></div>")
                      .arg(px(left + kSheetPadding), px(top + kSheetPadding),
                           px(right - left), px(bottom - top), escapeHtml(area.name));
    }
    if (blocks.isEmpty())
        return QString();
    return QStringLiteral("<div class=\"areas\">\n%1\n</div>\n")
        .arg(blocks.join(QLatin1Char('\n')));
}

QString baseCss(int zoomPercent)
{
    QString css = QStringLiteral(
        "html,body{margin:0;padding:0}\n"
        "body{background:#eef0f3;color:#1a1a1a;padding:16px 0;text-align:center;"
        "font:13.33px/1.25 Arial,Helvetica,sans-serif}\n"
        ".sheet{position:relative;display:inline-block;text-align:left;background:#fff;"
        "padding:12px;box-shadow:0 1px 4px rgba(0,0,0,.18);max-width:100%;overflow:visible}\n"
        ".mxl{border-collapse:collapse;table-layout:fixed;background:#fff}\n"
        ".mxl td{padding:0}\n"
        ".areas{position:absolute;left:0;top:0;right:0;bottom:0;pointer-events:none}\n"
        ".area{position:absolute;border:1px dashed #2196f3;background:rgba(33,150,243,.07)}\n"
        ".area>span{position:absolute;left:2px;top:1px;display:inline-block;white-space:nowrap;"
        "font:11px/1.3 Arial,Helvetica,sans-serif;color:#145aa0;"
        "background:rgba(255,255,255,.85);border-radius:2px;padding:0 3px}\n"
        ".empty{margin:0;padding:24px;color:#666}\n");
    if (zoomPercent != 100)
        css += QStringLiteral(".sheet{zoom:%1%}\n").arg(zoomPercent);
    return css;
}

QString printCss(const PrintSettings &settings)
{
    QString css;
    if (!settings.valid)
        return css;

    QStringList page;
    QString size = settings.pageSize.trimmed();
    bool asciiSize = !size.isEmpty();
    for (const QChar ch : size) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('.') && ch != QLatin1Char('-')) {
            asciiSize = false;
            break;
        }
    }
    if (settings.pageWidthMm > 0 && settings.pageHeightMm > 0) {
        page << QStringLiteral("%1mm %2mm").arg(settings.pageWidthMm).arg(settings.pageHeightMm);
    } else if (asciiSize) {
        page << size;
        if (settings.orientation == 2)
            page << QStringLiteral("landscape");
    }

    // Поля: верх, право, низ, лево (как в CSS).
    const QString margins = QStringLiteral("%1mm %2mm %3mm %4mm")
        .arg(settings.topMarginMm)
        .arg(settings.rightMarginMm)
        .arg(settings.bottomMarginMm)
        .arg(settings.leftMarginMm);

    css += QStringLiteral("@page{");
    if (!page.isEmpty())
        css += QStringLiteral("size:%1;").arg(page.join(QLatin1Char(' ')));
    css += QStringLiteral("margin:%1}\n").arg(margins);
    css += QStringLiteral("@media print{body{background:#fff;padding:0}"
                          ".sheet{margin:0;box-shadow:none;");
    const int scale = settings.scale > 0 ? settings.scale : 100;
    if (scale != 100)
        css += QStringLiteral("zoom:%1%;").arg(scale);
    css += QStringLiteral("}.areas{display:none}}\n");
    return css;
}

QString areaTypeName(NamedArea::Type type)
{
    switch (type) {
    case NamedArea::Type::Rows:
        return QStringLiteral("rows");
    case NamedArea::Type::Columns:
        return QStringLiteral("columns");
    case NamedArea::Type::Rectangle:
        return QStringLiteral("rectangle");
    case NamedArea::Type::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

} // namespace

QString MxlHtmlExporter::toHtml(const Document &document, const HtmlExportOptions &options)
{
    const Grid grid = buildGrid(document);

    QString title = options.title.trimmed();
    if (title.isEmpty())
        title = QStringLiteral("Табличный документ");
    const int zoom = options.zoomPercent > 0
        ? std::clamp(options.zoomPercent, 10, 800)
        : 100;

    QString html;
    html.reserve(64 * 1024);
    html += QStringLiteral("<!DOCTYPE html>\n<html lang=\"ru\">\n<head>\n"
                           "<meta charset=\"utf-8\">\n"
                           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n");
    html += QStringLiteral("<title>%1</title>\n<style>\n").arg(escapeHtml(title));
    html += baseCss(zoom);
    html += printCss(document.printSettings);
    html += QStringLiteral("</style>\n</head>\n<body>\n<div class=\"sheet\">\n");

    if (grid.rows == 0 || grid.columns == 0) {
        html += QStringLiteral("<p class=\"empty\">Макет не содержит данных</p>\n");
    } else {
        html += QStringLiteral("<table class=\"mxl\" style=\"width:%1\">\n<colgroup>\n")
                    .arg(px(grid.width));
        for (int column = 0; column < grid.columns; ++column) {
            const double width = grid.columnLeft.at(column + 1) - grid.columnLeft.at(column);
            html += QStringLiteral("<col style=\"width:%1\">\n").arg(px(width));
        }
        html += QStringLiteral("</colgroup>\n<tbody>\n");

        QHash<int, const Row *> rowsByIndex;
        for (const Row &row : document.rows)
            rowsByIndex.insert(row.index, &row);

        for (int seq = 0; seq < grid.rows; ++seq) {
            const int rowIndex = grid.rowIndex.at(seq);
            const Row *row = rowsByIndex.value(rowIndex, nullptr);
            const RowMetric &metric = grid.metrics.at(seq);

            html += QStringLiteral("<tr");
            if (metric.fixedHeight > 0)
                html += QStringLiteral(" style=\"height:%1\"").arg(px(metric.fixedHeight));
            html += QStringLiteral(">");

            QHash<int, const Cell *> cells;
            if (row) {
                for (const auto &cell : row->cells)
                    cells.insert(cell.first, &cell.second);
            }
            const CellFormat *rowFormat = row ? document.formatAt(row->format) : nullptr;

            int column = 0;
            while (column < grid.columns) {
                const int mergeId = grid.mergeId.at(seq).at(column);
                int colspan = 1;
                int rowspan = 1;
                if (mergeId >= 0) {
                    if (grid.mergeTop.at(mergeId) != seq
                        || grid.mergeLeft.at(mergeId) != column) {
                        ++column;
                        continue;
                    }
                    colspan = grid.colspan.at(mergeId);
                    rowspan = grid.rowspan.at(mergeId);
                }

                const Cell *cell = cells.value(column, nullptr);
                const CellFormat *format = cell ? document.formatAt(cell->format) : nullptr;
                if (!format)
                    format = rowFormat;

                QString textHtml;
                QString text = cell ? cell->text : QString();
                if (!text.isEmpty()) {
                    textHtml = escapeHtml(text);
                    if (format && format->orientation > 0) {
                        const QString degrees = QString::number(format->orientation / 10.0, 'g', 4);
                        textHtml = QStringLiteral("<span style=\"display:inline-block;"
                                                  "transform:rotate(%1deg)\">%2</span>")
                                       .arg(degrees, textHtml);
                    }
                }

                html += QStringLiteral("<td");
                if (colspan > 1)
                    html += QStringLiteral(" colspan=\"%1\"").arg(colspan);
                if (rowspan > 1)
                    html += QStringLiteral(" rowspan=\"%1\"").arg(rowspan);
                const QString style = buildCellStyle(document, format);
                if (!style.isEmpty())
                    html += QStringLiteral(" style=\"%1\"").arg(style);
                html += QLatin1Char('>');
                html += textHtml;
                html += QLatin1String("</td>");

                column += colspan;
            }
            html += QStringLiteral("</tr>\n");
        }

        html += QStringLiteral("</tbody>\n</table>\n");
        if (options.namedAreas)
            html += buildAreasHtml(document, grid);
    }

    html += QStringLiteral("</div>\n</body>\n</html>\n");
    return html;
}

QJsonObject MxlHtmlExporter::summarize(const Document &document)
{
    QJsonObject root;
    root.insert(QStringLiteral("schema"), 1);
    root.insert(QStringLiteral("rows"), document.rowCount());
    root.insert(QStringLiteral("columns"), document.maxColumn() + 1);
    root.insert(QStringLiteral("formats"), document.formats.size());
    root.insert(QStringLiteral("fonts"), document.fonts.size());
    root.insert(QStringLiteral("lines"), document.lines.size());
    root.insert(QStringLiteral("colors"), document.colors.size());
    root.insert(QStringLiteral("merges"), document.merges.size());
    root.insert(QStringLiteral("drawings"), document.drawings.size());

    QJsonArray areas;
    for (const NamedArea &area : document.namedAreas) {
        QJsonObject item;
        item.insert(QStringLiteral("name"), area.name);
        item.insert(QStringLiteral("type"), areaTypeName(area.type));
        if (area.hasRows()) {
            item.insert(QStringLiteral("top"), area.top);
            item.insert(QStringLiteral("bottom"), area.bottom);
        }
        if (area.hasColumns()) {
            item.insert(QStringLiteral("left"), area.left);
            item.insert(QStringLiteral("right"), area.right);
        }
        areas.append(item);
    }
    root.insert(QStringLiteral("namedAreas"), areas);

    QJsonObject print;
    const PrintSettings &settings = document.printSettings;
    print.insert(QStringLiteral("valid"), settings.valid);
    if (settings.valid) {
        print.insert(QStringLiteral("pageSize"), settings.pageSize);
        print.insert(QStringLiteral("orientation"),
                     settings.orientation == 2 ? QStringLiteral("landscape")
                                               : QStringLiteral("portrait"));
        print.insert(QStringLiteral("scale"), settings.scale > 0 ? settings.scale : 100);
        QJsonObject margins;
        margins.insert(QStringLiteral("top"), settings.topMarginMm);
        margins.insert(QStringLiteral("right"), settings.rightMarginMm);
        margins.insert(QStringLiteral("bottom"), settings.bottomMarginMm);
        margins.insert(QStringLiteral("left"), settings.leftMarginMm);
        print.insert(QStringLiteral("marginsMm"), margins);
        if (settings.pageWidthMm > 0)
            print.insert(QStringLiteral("widthMm"), settings.pageWidthMm);
        if (settings.pageHeightMm > 0)
            print.insert(QStringLiteral("heightMm"), settings.pageHeightMm);
    }
    root.insert(QStringLiteral("print"), print);
    return root;
}

} // namespace mxl
} // namespace v8reader
