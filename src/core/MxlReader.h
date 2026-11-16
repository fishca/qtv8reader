#ifndef QTV8READER_MXLREADER_H
#define QTV8READER_MXLREADER_H

#include "core/MxlDocument.h"

#include <QByteArray>
#include <QString>

namespace v8reader {

// Читатель табличных документов 1С (формат MXL8: 1С 8.2/8.3, BAS).
// Поддерживается содержимое отдельного файла *.mxl и элемента контейнера
// .cf/.epf (там лежит такой же MXL-файл).
class MxlReader
{
public:
    static bool parseBytes(const QByteArray &data, mxl::Document *document,
                           QString *error = nullptr);
    static bool parseFile(const QString &path, mxl::Document *document,
                          QString *error = nullptr);

    // Быстрая проверка сигнатуры (без полного разбора).
    static bool looksLikeMxl(const QByteArray &data);
};

} // namespace v8reader

#endif // QTV8READER_MXLREADER_H
