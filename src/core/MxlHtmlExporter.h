#ifndef QTV8READER_MXLHTMLEXPORTER_H
#define QTV8READER_MXLHTMLEXPORTER_H

#include "core/MxlDocument.h"

#include <QJsonObject>
#include <QString>

namespace v8reader {
namespace mxl {

// Параметры HTML-экспорта макета.
struct HtmlExportOptions
{
    QString title;           // заголовок страницы; пусто — «Табличный документ»
    bool namedAreas = true;  // рисовать контур именованных областей
    int zoomPercent = 100;   // масштаб отображения (CSS zoom), 25–400
};

// Экспорт табличного документа в самодостаточную HTML-страницу: одна
// таблица с объединениями, стилями, цветами и шрифтами. QtGui не нужен —
// экспортёр годится и для CLI, и для тестов без QApplication.
class MxlHtmlExporter
{
public:
    static QString toHtml(const Document &document,
                          const HtmlExportOptions &options = HtmlExportOptions());

    // Сводка макета для `mxl --json` (схема 1).
    static QJsonObject summarize(const Document &document);
};

} // namespace mxl
} // namespace v8reader

#endif // QTV8READER_MXLHTMLEXPORTER_H
