#include "gui/MxlPrinter.h"

#include "gui/MxlRenderer.h"

#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPrinter>
#include <QSizeF>

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace v8reader {

namespace {

constexpr double kDpi = 96.0;
constexpr double kMmPerInch = 25.4;

struct PrintPlan
{
    QVector<QPair<int, int>> columns; // диапазоны колонок (включительно)
    QVector<QPair<int, int>> rows;    // диапазоны строк (по порядку документа)
    double scale = 1.0;
};

mxl::PrintSettings effectiveSettings(const mxl::Document &document)
{
    mxl::PrintSettings settings = document.printSettings;
    if (!settings.valid) {
        settings = mxl::PrintSettings();
        settings.valid = true;
    }
    if (settings.pageSize.isEmpty())
        settings.pageSize = QStringLiteral("A4");
    if (settings.orientation != 2)
        settings.orientation = 1;
    if (settings.scale <= 0 || settings.scale > 400)
        settings.scale = 100;
    if (settings.topMarginMm < 0)
        settings.topMarginMm = 10.0;
    if (settings.bottomMarginMm < 0)
        settings.bottomMarginMm = 10.0;
    if (settings.leftMarginMm < 0)
        settings.leftMarginMm = 10.0;
    if (settings.rightMarginMm < 0)
        settings.rightMarginMm = 10.0;
    return settings;
}

QPageSize pageSizeFor(const mxl::PrintSettings &settings)
{
    const QString name = settings.pageSize.trimmed().toUpper();
    if (name == QStringLiteral("A3"))
        return QPageSize(QPageSize::A3);
    if (name == QStringLiteral("A4"))
        return QPageSize(QPageSize::A4);
    if (name == QStringLiteral("A5"))
        return QPageSize(QPageSize::A5);
    if (name == QStringLiteral("A6"))
        return QPageSize(QPageSize::A6);
    if (name == QStringLiteral("LETTER"))
        return QPageSize(QPageSize::Letter);
    if (name == QStringLiteral("LEGAL"))
        return QPageSize(QPageSize::Legal);

    if (settings.pageWidthMm > 0 && settings.pageHeightMm > 0) {
        return QPageSize(QSizeF(settings.pageWidthMm, settings.pageHeightMm),
                         QPageSize::Millimeter);
    }
    return QPageSize(QPageSize::A4);
}

QPageLayout makeLayoutFor(const mxl::PrintSettings &settings)
{
    const QPageLayout::Orientation orientation =
        settings.orientation == 2 ? QPageLayout::Landscape : QPageLayout::Portrait;
    // Поля вычитаются вручную при отрисовке, поэтому у страницы они нулевые.
    return QPageLayout(pageSizeFor(settings), orientation,
                       QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
}

bool buildPlan(const MxlRenderer &renderer, const mxl::PrintSettings &settings,
               const QPageLayout &layout, PrintPlan *plan, QString *error)
{
    if (!renderer.isValid()) {
        if (error)
            *error = QStringLiteral("Табличный документ не разобран.");
        return false;
    }

    const QRectF full = layout.fullRect(QPageLayout::Millimeter);
    double printableWidthMm = full.width() - settings.leftMarginMm - settings.rightMarginMm;
    double printableHeightMm = full.height() - settings.topMarginMm - settings.bottomMarginMm;
    if (printableWidthMm <= 5.0)
        printableWidthMm = std::max(10.0, full.width() * 0.8);
    if (printableHeightMm <= 5.0)
        printableHeightMm = std::max(10.0, full.height() * 0.8);

    const double scale = std::clamp(settings.scale / 100.0, 0.1, 4.0);
    plan->scale = scale;
    const double printableWidthDoc = printableWidthMm / kMmPerInch * kDpi / scale;
    const double printableHeightDoc = printableHeightMm / kMmPerInch * kDpi / scale;

    const int maxColumn = std::max(0, renderer.columnCount() - 1);
    int column = 0;
    while (column <= maxColumn) {
        const int start = column;
        double width = renderer.columnWidth(column);
        ++column;
        while (column <= maxColumn
               && width + renderer.columnWidth(column) <= printableWidthDoc + 0.01) {
            width += renderer.columnWidth(column);
            ++column;
        }
        plan->columns.append({start, column - 1});
    }

    if (renderer.rowCount() == 0) {
        if (error)
            *error = QStringLiteral("В документе нет строк.");
        return false;
    }

    const auto &rows = renderer.document().rows;
    int firstIndex = 0;
    double height = 0.0;
    for (int i = 0; i < rows.size(); ++i) {
        const int rowIndex = rows.at(i).index;
        const double rowHeight = renderer.rowHeight(rowIndex);
        if (i == firstIndex) {
            height = rowHeight;
            if (height > printableHeightDoc) {
                // одна строка выше страницы — отдельная страница
                plan->rows.append({rowIndex, rowIndex});
                firstIndex = i + 1;
                height = 0.0;
                continue;
            }
            continue;
        }
        if (height + rowHeight > printableHeightDoc + 0.01) {
            plan->rows.append({rows.at(firstIndex).index, rows.at(i - 1).index});
            firstIndex = i;
            height = rowHeight;
        } else {
            height += rowHeight;
        }
    }
    if (firstIndex < rows.size())
        plan->rows.append({rows.at(firstIndex).index, rows.last().index});

    if (plan->columns.isEmpty() || plan->rows.isEmpty()) {
        if (error)
            *error = QStringLiteral("Не удалось разложить документ по страницам.");
        return false;
    }
    return true;
}

template <typename Device>
bool printToDevice(const mxl::Document &document, Device &device, QString *error)
{
    const mxl::PrintSettings settings = effectiveSettings(document);
    const QPageLayout layout = makeLayoutFor(settings);
    if (!device.setPageLayout(layout)) {
        if (error)
            *error = QStringLiteral("Устройство печати не приняло параметры страницы.");
        return false;
    }
    if constexpr (std::is_same_v<Device, QPrinter>)
        device.setFullPage(true);

    MxlRenderer renderer(document);
    PrintPlan plan;
    if (!buildPlan(renderer, settings, layout, &plan, error))
        return false;

    QPainter painter;
    if (!painter.begin(&device)) {
        if (error)
            *error = QStringLiteral("Не удалось начать печать.");
        return false;
    }

    int dpi = device.resolution();
    if (dpi <= 0)
        dpi = 96;
    const double marginLeft = settings.leftMarginMm / kMmPerInch * dpi;
    const double marginTop = settings.topMarginMm / kMmPerInch * dpi;
    const double contentScale = plan.scale * dpi / kDpi;

    bool firstPage = true;
    for (const auto &columnRange : plan.columns) {
        for (const auto &rowRange : plan.rows) {
            if (!firstPage) {
                if (!device.newPage()) {
                    painter.end();
                    if (error)
                        *error = QStringLiteral("Не удалось создать новую страницу.");
                    return false;
                }
            }
            firstPage = false;

            const double x = renderer.columnLeft(columnRange.first);
            const double y = renderer.rowTop(rowRange.first);
            const double width = renderer.columnLeft(columnRange.second + 1) - x;
            const double height = renderer.rowTop(rowRange.second)
                + renderer.rowHeight(rowRange.second) - y;
            const QRectF visible(x, y, width, height);

            painter.save();
            painter.translate(marginLeft, marginTop);
            painter.scale(contentScale, contentScale);
            painter.translate(-x, -y);
            painter.setClipRect(visible);
            renderer.draw(painter, visible, false, QString());
            painter.restore();
        }
    }

    painter.end();
    return true;
}

} // namespace

bool MxlPrinter::print(const mxl::Document &document, QPrinter &printer,
                       QString *error)
{
    return printToDevice(document, printer, error);
}

bool MxlPrinter::exportPdf(const mxl::Document &document, const QString &path,
                           QString *error)
{
    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("Не указан путь для PDF.");
        return false;
    }
    QPdfWriter writer(path);
    writer.setResolution(300);
    writer.setTitle(QStringLiteral("MXL"));
    return printToDevice(document, writer, error);
}

int MxlPrinter::pageCount(const mxl::Document &document, const QPageLayout &layout)
{
    const mxl::PrintSettings settings = effectiveSettings(document);
    MxlRenderer renderer(document);
    PrintPlan plan;
    if (!buildPlan(renderer, settings, layout, &plan, nullptr))
        return 0;
    return plan.columns.size() * plan.rows.size();
}

QPageLayout MxlPrinter::makeLayout(const mxl::Document &document)
{
    return makeLayoutFor(effectiveSettings(document));
}

} // namespace v8reader
