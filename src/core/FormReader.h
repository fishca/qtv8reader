#ifndef QTV8READER_FORMREADER_H
#define QTV8READER_FORMREADER_H

#include "core/FormDocument.h"

#include <QByteArray>
#include <QString>

namespace v8reader {

class FormBindingResolver;

class FormReader
{
public:
    // Быстрая проверка, что данные похожи на управляемую форму 1С.
    static bool looksLikeForm(const QByteArray &data);

    // resolver (необязательный) расшифровывает пути к данным по метаданным
    // объекта: заголовки, типы и ширины полей.
    static bool parseBytes(const QByteArray &data, FormDocument *document,
                           QString *error = nullptr,
                           const FormBindingResolver *resolver = nullptr);
    static bool parseFile(const QString &path, FormDocument *document,
                          QString *error = nullptr,
                          const FormBindingResolver *resolver = nullptr);
};

} // namespace v8reader

#endif // QTV8READER_FORMREADER_H
