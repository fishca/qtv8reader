#ifndef QTV8READER_FORMDOCUMENT_H
#define QTV8READER_FORMDOCUMENT_H

#include <QHash>
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
        Search,
        AutoCommandBar,
        Other
    };

    Type type = Type::Other;
    int typeCode = 0;      // код элемента в сериализации (22 — группа, 37 — поле, ...)
    int groupKind = -1;    // вид группы (9 — командная панель, 6 — группа кнопок, ...)
    int orientation = -1;  // 1 — горизонтально, 0 — вертикально (для групп)
    int id = 0;
    int widthUnits = -1;       // ширина поля в единицах формы (×10 px); -1 — авто
    int heightUnits = -1;      // высота в единицах формы
    int horizontalStretch = -1; // 0 — нет, 1 — да, 2 — авто
    int verticalStretch = -1;
    bool visible = true;
    bool showTitle = true;      // TitleLocation != None

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
    // Синонимы реквизитов объекта/формы: имя -> заголовок.
    QHash<QString, QString> titles;
    // Форма списка документа: платформа добавляет команды
    // «Создать на основании», «Еще», «?».
    bool runtimeListCommands = false;
    // Форма документа: платформа добавляет «Провести и закрыть»,
    // «Записать» и «Провести».
    bool runtimeDocumentCommands = false;

    int itemCount() const;
    int fieldCount() const;
};

} // namespace v8reader

#endif // QTV8READER_FORMDOCUMENT_H
