#include "core/FormDocument.h"

namespace v8reader {

QString FormItem::typeTitle() const
{
    switch (type) {
    case Type::Group:
        switch (groupKind) {
        case 9:
            return QStringLiteral("Командная панель");
        case 6:
            return QStringLiteral("Группа кнопок");
        case 8:
            return QStringLiteral("Контекстное меню");
        case 1:
            return QStringLiteral("Подменю");
        case 3:
            return QStringLiteral("Страницы");
        case 4:
            return QStringLiteral("Страница");
        default:
            return QStringLiteral("Группа");
        }
    case Type::Field:
        return QStringLiteral("Поле");
    case Type::Button:
        return QStringLiteral("Кнопка");
    case Type::Table:
        return QStringLiteral("Таблица");
    case Type::Decoration:
        return QStringLiteral("Декорация");
    case Type::Column:
        return QStringLiteral("Колонка");
    case Type::AutoCommandBar:
        return QStringLiteral("Командная панель формы");
    case Type::Other:
        break;
    }
    return QStringLiteral("Элемент");
}

namespace {

int countItems(const QVector<FormItem> &items, bool fieldsOnly)
{
    int result = 0;
    for (const FormItem &item : items) {
        if (!fieldsOnly || item.type == FormItem::Type::Field)
            ++result;
        result += countItems(item.children, fieldsOnly);
    }
    return result;
}

} // namespace

int FormDocument::itemCount() const
{
    return countItems(items, false);
}

int FormDocument::fieldCount() const
{
    return countItems(items, true);
}

} // namespace v8reader
