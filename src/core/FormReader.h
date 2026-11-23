#ifndef QTV8READER_FORMREADER_H
#define QTV8READER_FORMREADER_H

#include "core/FormDocument.h"

#include <QByteArray>
#include <QString>

namespace v8reader {

class FormBindingResolver;
class FormStyleTable;

class FormReader
{
public:
    // Быстрая проверка, что данные похожи на управляемую форму 1С.
    static bool looksLikeForm(const QByteArray &data);

    // resolver (необязательный) расшифровывает пути к данным по метаданным
    // объекта: заголовки, типы и ширины полей.
    // styles (необязательный) переводит ссылки на стили оформления из GUID-ов
    // и кодов платформы в имена, как их пишет Form.xml.
    static bool parseBytes(const QByteArray &data, FormDocument *document,
                           QString *error = nullptr,
                           const FormBindingResolver *resolver = nullptr,
                           const FormStyleTable *styles = nullptr);
    static bool parseFile(const QString &path, FormDocument *document,
                          QString *error = nullptr,
                          const FormBindingResolver *resolver = nullptr,
                          const FormStyleTable *styles = nullptr);

    // Отладочное представление узлов формы (JSON): для каждого элемента —
    // код типа, имя и «слоты» — дочерние узлы в том порядке, в котором они
    // записаны в контейнере. Нужно для сверки расположения свойств с Form.xml.
    static QByteArray describeNodes(const QByteArray &data, QString *error = nullptr);
};

} // namespace v8reader

#endif // QTV8READER_FORMREADER_H
