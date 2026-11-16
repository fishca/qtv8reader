#ifndef QTV8READER_MXLPRINTER_H
#define QTV8READER_MXLPRINTER_H

#include "core/MxlDocument.h"

#include <QPageLayout>
#include <QString>

class QPrinter;
class QPdfWriter;

namespace v8reader {

// Печать и экспорт в PDF табличного документа MXL с учётом параметров
// печати макета: размер страницы, ориентация, масштаб, поля. Документ
// разбивается на страницы по колонкам и строкам.
class MxlPrinter
{
public:
    static bool print(const mxl::Document &document, QPrinter &printer,
                      QString *error = nullptr);
    static bool exportPdf(const mxl::Document &document, const QString &path,
                          QString *error = nullptr);

    // Количество страниц при заданном макете страницы (для прогресса/сводки).
    static int pageCount(const mxl::Document &document, const QPageLayout &layout);

private:
    static QPageLayout makeLayout(const mxl::Document &document);
};

} // namespace v8reader

#endif // QTV8READER_MXLPRINTER_H
