#ifndef QTV8READER_FORMBINDINGS_H
#define QTV8READER_FORMBINDINGS_H

#include "core/Configuration.h"
#include "core/FormDocument.h"

#include <QHash>
#include <QString>

namespace v8reader {

// Расшифровка пути к данным элемента формы по метаданным объекта.
struct FormBinding
{
    QString text;      // «Объект.Билеты.Сумма»
    QString title;     // синоним реквизита («Сумма»)
    QString typeText;  // «Число(15,2)», «СправочникСсылка.Организации»
    FormValueKind kind = FormValueKind::Unknown;
    int chars = 0;     // ширина поля в символах, как в конфигураторе
};

// Индекс реквизитов объекта метаданных: GUID -> имя, синоним и тип.
// В бинарной форме путь элемента ссылается на реквизиты именно по GUID,
// поэтому один и тот же индекс обслуживает все формы объекта.
class FormBindingResolver
{
public:
    void clear();
    void addObject(const MetadataObject &object);
    bool isEmpty() const { return m_byGuid.isEmpty(); }

    // Расшифровывает путь: подставляет заголовки и типы. Последний сегмент —
    // имя самого элемента, если путь оборвался на стандартном реквизите.
    FormBinding resolve(const QVector<FormPathSegment> &path,
                        const FormDocument &document,
                        const QString &fallbackName = QString()) const;

    // Заполняет dataPathText/dataTitle/typeText/valueKind/valueChars
    // у всех элементов документа.
    void applyToDocument(FormDocument *document) const;

    static FormValueKind kindForTypeText(const QString &typeText);
    static int charsForType(const QString &typeText, FormValueKind kind);
    static QString typeTextForStandardCode(int code);

private:
    struct Entry
    {
        QString name;
        QString synonym;
        QString typeText;
        bool tabularSection = false;
    };

    QHash<QString, Entry> m_byGuid;
    // Имена реквизитов верхнего уровня: нужны полям динамических списков,
    // которые в форме адресуются номером, а в метаданных — именем.
    QHash<QString, Entry> m_byName;
};

} // namespace v8reader

#endif // QTV8READER_FORMBINDINGS_H
