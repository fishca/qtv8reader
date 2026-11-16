#ifndef QTV8READER_FORMDOCUMENT_H
#define QTV8READER_FORMDOCUMENT_H

#include <QString>
#include <QVector>

namespace v8reader {

// Элемент управляемой формы 1С (поле, группа, кнопка, таблица и т. д.).
struct FormItem
{
    enum class Type {
        Group,
        Field,
        Button,
        Table,
        Decoration,
        Column,
        AutoCommandBar,
        Other
    };

    Type type = Type::Other;
    int typeCode = 0;      // код элемента в сериализации (22 — группа, 37 — поле, ...)
    int groupKind = -1;    // вид группы (9 — командная панель, 6 — группа кнопок, ...)
    int orientation = -1;  // 1 — горизонтально, 0 — вертикально (для групп)
    int id = 0;
    QString guid;
    QString name;
    QString synonym;
    QVector<FormItem> children;

    QString title() const { return synonym.isEmpty() ? name : synonym; }
    QString typeTitle() const;
};

struct FormAttribute
{
    QString name;
    QString synonym;
    QString typeHint;
};

struct FormCommand
{
    QString name;
    QString synonym;
};

struct FormDocument
{
    bool valid = false;
    QString name;     // имя формы из метаданных
    QString synonym;  // синоним формы из метаданных
    QString moduleText;
    QVector<FormItem> items;
    QVector<FormAttribute> attributes;
    QVector<FormCommand> commands;

    int itemCount() const;
    int fieldCount() const;
};

} // namespace v8reader

#endif // QTV8READER_FORMDOCUMENT_H
