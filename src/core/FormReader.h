#ifndef QTV8READER_FORMREADER_H
#define QTV8READER_FORMREADER_H

#include "core/FormDocument.h"

#include <QByteArray>
#include <QString>

namespace v8reader {

class FormReader
{
public:
    // Быстрая проверка, что данные похожи на управляемую форму 1С.
    static bool looksLikeForm(const QByteArray &data);

    static bool parseBytes(const QByteArray &data, FormDocument *document,
                           QString *error = nullptr);
    static bool parseFile(const QString &path, FormDocument *document,
                          QString *error = nullptr);
};

} // namespace v8reader

#endif // QTV8READER_FORMREADER_H
