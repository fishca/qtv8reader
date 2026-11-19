#include "core/FormReader.h"

#include "core/V8Text.h"

#include <QFile>
#include <QFileInfo>

namespace v8reader {

namespace {

bool isItemNode(const V8TextNode &node)
{
    if (!node.isList() || node.children.size() < 2)
        return false;
    const V8TextNode *code = node.child(0);
    const V8TextNode *ref = node.child(1);
    return code && code->isInteger() && ref && ref->isList()
        && ref->children.size() >= 2 && ref->child(0)->isInteger()
        && ref->child(1)->isGuid();
}

bool isIdentifierScalar(const V8TextNode &node)
{
    if (!node.isScalar())
        return false;
    const QString value = node.scalar();
    if (value.isEmpty() || value.at(0).isDigit() || value.at(0) == QLatin1Char('-'))
        return false;
    if (V8Text::isGuid(value))
        return false;
    static const QStringList service = {
        QStringLiteral("Pattern"), QStringLiteral("S"), QStringLiteral("#"),
        QStringLiteral("B"), QStringLiteral("N"), QStringLiteral("D"),
        QStringLiteral("U"), QStringLiteral("G"), QStringLiteral("L"),
        QStringLiteral("T"), QStringLiteral("F"), QStringLiteral("P"),
        QStringLiteral("N")
    };
    return !service.contains(value);
}

QString findName(const V8TextNode &node, int startIndex, int *nameIndex = nullptr)
{
    for (int i = startIndex; i < node.children.size(); ++i) {
        const V8TextNode *child = node.child(i);
        if (child && isIdentifierScalar(*child)) {
            if (nameIndex)
                *nameIndex = i;
            return child->scalar();
        }
    }
    if (nameIndex)
        *nameIndex = -1;
    return QString();
}

// Ищет первый локализованный текст {"ru", "..."} в дочерних списках,
// не заходя в дочерние элементы формы.
QString localizedText(const V8TextNode &node, int depth = 0)
{
    if (depth > 3 || !node.isList())
        return QString();
    for (const V8TextNode &child : node.children) {
        if (!child.isList() || child.children.size() < 2)
            continue;
        if (isItemNode(child))
            continue;
        const V8TextNode *lang = child.child(0);
        const V8TextNode *value = child.child(1);
        if (lang && value && lang->isScalar() && value->isScalar()
            && lang->scalar() == QLatin1String("ru") && !value->scalar().isEmpty()) {
            return value->scalar();
        }
        const QString nested = localizedText(child, depth + 1);
        if (!nested.isEmpty())
            return nested;
    }
    return QString();
}

int integerChild(const V8TextNode &node, int index)
{
    const V8TextNode *child = node.child(index);
    if (!child || !child->isInteger())
        return -1;
    return static_cast<int>(child->toInteger());
}

QString typeHintFromAttributes(const V8TextNode &node)
{
    for (const V8TextNode &child : node.children) {
        if (!child.isList() || child.children.isEmpty())
            continue;
        const V8TextNode *first = child.child(0);
        if (!first || !first->isScalar())
            continue;
        if (first->scalar() != QLatin1String("Pattern"))
            continue;

        bool hasString = false;
        bool hasRef = false;
        bool hasBool = false;
        bool hasNumber = false;
        bool hasDate = false;
        for (const V8TextNode &inner : child.children) {
            if (inner.isScalar()) {
                const QString value = inner.scalar();
                if (value == QLatin1String("S")) hasString = true;
                if (value == QLatin1String("#")) hasRef = true;
                if (value == QLatin1String("B")) hasBool = true;
                if (value == QLatin1String("N")) hasNumber = true;
                if (value == QLatin1String("D")) hasDate = true;
            } else if (inner.isList() && inner.child(0)) {
                const QString value = inner.child(0)->scalar();
                if (value == QLatin1String("S")) hasString = true;
                if (value == QLatin1String("#")) hasRef = true;
                if (value == QLatin1String("B")) hasBool = true;
                if (value == QLatin1String("N")) hasNumber = true;
                if (value == QLatin1String("D")) hasDate = true;
            }
        }
        if (hasString) return QStringLiteral("Строка");
        if (hasRef) return QStringLiteral("Ссылка");
        if (hasBool) return QStringLiteral("Булево");
        if (hasNumber) return QStringLiteral("Число");
        if (hasDate) return QStringLiteral("Дата");
        return QStringLiteral("Составной тип");
    }
    return QString();
}

void readGeometry(const V8TextNode &node, int nameIndex, FormItem *item)
{
    // Поля (InputField/LabelField/PictureField/CheckBoxField): вложенная
    // layout-структура с кодом 10/11/32:
    //   10/11: [1] Width, [2] Height, [3] HorizontalStretch, [4] VerticalStretch
    //   32:    [2] Width, [3] Height, [4] HorizontalStretch, [5] VerticalStretch
    if (item->typeCode == 37) {
        for (const V8TextNode &child : node.children) {
            if (!child.isList() || child.children.size() < 8)
                continue;
            const V8TextNode *first = child.child(0);
            if (!first || !first->isInteger())
                continue;
            const int kind = static_cast<int>(first->toInteger());
            if (kind == 10 || kind == 11) {
                item->labelOnly = true;
                if (child.child(1) && child.child(1)->isInteger())
                    item->widthUnits = static_cast<int>(child.child(1)->toInteger());
                if (child.child(2) && child.child(2)->isInteger())
                    item->heightUnits = static_cast<int>(child.child(2)->toInteger());
                if (child.child(3) && child.child(3)->isInteger())
                    item->horizontalStretch = static_cast<int>(child.child(3)->toInteger());
                if (child.child(4) && child.child(4)->isInteger())
                    item->verticalStretch = static_cast<int>(child.child(4)->toInteger());
                continue;
            }
            if (kind == 32) {
                item->labelOnly = false;
                if (child.child(2) && child.child(2)->isInteger())
                    item->widthUnits = static_cast<int>(child.child(2)->toInteger());
                if (child.child(3) && child.child(3)->isInteger())
                    item->heightUnits = static_cast<int>(child.child(3)->toInteger());
                if (child.child(4) && child.child(4)->isInteger())
                    item->horizontalStretch = static_cast<int>(child.child(4)->toInteger());
                if (child.child(5) && child.child(5)->isInteger())
                    item->verticalStretch = static_cast<int>(child.child(5)->toInteger());
                return;
            }
        }
        return;
    }

    // Группа: ширина в поле 12, высота — в поле 13; растяжение —
    // со смещениями +8/+9 от имени (сверено по 11 формам БП).
    if (item->typeCode == 22) {
        if (node.child(12) && node.child(12)->isInteger())
            item->widthUnits = static_cast<int>(node.child(12)->toInteger());
        if (node.child(13) && node.child(13)->isInteger())
            item->heightUnits = static_cast<int>(node.child(13)->toInteger());
        if (nameIndex >= 0) {
            const V8TextNode *h = node.child(nameIndex + 8);
            if (h && h->isInteger())
                item->horizontalStretch = static_cast<int>(h->toInteger());
            const V8TextNode *v = node.child(nameIndex + 9);
            if (v && v->isInteger())
                item->verticalStretch = static_cast<int>(v->toInteger());
        }
        return;
    }

    // Декорация: ширина в поле 10, высота — в поле 11.
    if (item->typeCode == 12) {
        if (node.child(10) && node.child(10)->isInteger())
            item->widthUnits = static_cast<int>(node.child(10)->toInteger());
        if (node.child(11) && node.child(11)->isInteger())
            item->heightUnits = static_cast<int>(node.child(11)->toInteger());
    }
}

FormItem parseItem(const V8TextNode &node)
{
    FormItem item;
    item.typeCode = static_cast<int>(node.child(0)->toInteger());

    const V8TextNode *ref = node.child(1);
    if (ref && ref->isList() && ref->children.size() >= 2) {
        if (ref->child(0)->isInteger())
            item.id = static_cast<int>(ref->child(0)->toInteger());
        if (ref->child(1)->isGuid())
            item.guid = ref->child(1)->normalizedGuid();
    }

    int nameIndex = -1;
    item.name = findName(node, 2, &nameIndex);
    if (item.typeCode == 22 && nameIndex > 2)
        item.groupKind = integerChild(node, nameIndex - 1);
    item.synonym = localizedText(node);

    // Поля: сразу после имени — TitleLocation (1 — показывать, 0 — None),
    // через 37 полей — Visible (1/0). Сверено с Form.xml по id.
    if (item.typeCode == 37 && nameIndex >= 0) {
        const V8TextNode *title = node.child(nameIndex + 1);
        if (title && title->isInteger())
            item.showTitle = title->toInteger() != 0;
        const V8TextNode *visible = node.child(nameIndex + 37);
        if (visible && visible->isInteger())
            item.visible = visible->toInteger() != 0;
    }

    for (const V8TextNode &child : node.children) {
        if (!child.isList() || child.children.size() < 2)
            continue;
        if (child.child(0)->isScalar()
            && child.child(0)->scalar() == QLatin1String("29")
            && child.child(1)->isInteger()) {
            item.orientation = static_cast<int>(child.child(1)->toInteger());
            break;
        }
    }

    switch (item.typeCode) {
    case 22:
        item.type = FormItem::Type::Group;
        break;
    case 37:
        item.type = FormItem::Type::Field;
        break;
    case 31:
        item.type = FormItem::Type::Button;
        break;
    case 55:
        item.type = FormItem::Type::Table;
        break;
    case 12:
        item.type = FormItem::Type::Decoration;
        break;
    case 5:
        item.type = FormItem::Type::Search;
        break;
    case 2:
        item.type = FormItem::Type::AutoCommandBar;
        break;
    default:
        item.type = FormItem::Type::Other;
        break;
    }

    readGeometry(node, nameIndex, &item);

    for (const V8TextNode &child : node.children) {
        if (isItemNode(child))
            item.children.append(parseItem(child));
    }
    return item;
}

} // namespace

bool FormReader::looksLikeForm(const QByteArray &data)
{
    if (data.isEmpty())
        return false;

    QString error;
    QString text = V8Text::decode(data, nullptr, &error);
    if (text.isEmpty())
        return false;
    text = text.trimmed();
    if (!text.startsWith(QStringLiteral("{4,")))
        return false;

    V8TextNode root;
    V8TextError parseError;
    if (!V8Text::parse(text, root, &parseError) || !root.isList()
        || root.children.isEmpty() || !root.child(0)->isInteger())
        return false;
    return root.child(0)->toInteger() == 4;
}

bool FormReader::parseBytes(const QByteArray &data, FormDocument *document,
                            QString *error)
{
    if (!document) {
        if (error)
            *error = QStringLiteral("Не передан получатель формы.");
        return false;
    }
    *document = FormDocument();

    QString decodeError;
    const QString text = V8Text::decode(data, nullptr, &decodeError);
    if (text.isEmpty()) {
        if (error)
            *error = QStringLiteral("Не удалось декодировать форму: %1").arg(decodeError);
        return false;
    }

    V8TextNode root;
    V8TextError parseError;
    if (!V8Text::parse(text, root, &parseError)) {
        if (error)
            *error = QStringLiteral("Не удалось разобрать форму: %1")
                         .arg(parseError.toString());
        return false;
    }
    if (!root.isList() || root.children.size() < 2 || !root.child(0)->isInteger()
        || root.child(0)->toInteger() != 4) {
        if (error)
            *error = QStringLiteral("Неизвестный формат формы: ожидался корневой "
                                    "элемент управляемой формы (4).");
        return false;
    }

    const V8TextNode *header = root.child(1);
    if (!header || !header->isList()) {
        if (error)
            *error = QStringLiteral("В форме отсутствует заголовок (элемент 2).");
        return false;
    }

    const V8TextNode *module = root.child(2);
    if (module && module->isScalar())
        document->moduleText = module->scalar();

    // Элементы формы — прямые дочерние элементы заголовка формы.
    for (const V8TextNode &child : header->children) {
        if (isItemNode(child))
            document->items.append(parseItem(child));
    }
    // На случай нестандартной вложенности: ищем список, состоящий из элементов.
    if (document->items.isEmpty()) {
        for (const V8TextNode &child : header->children) {
            if (!child.isList() || child.children.isEmpty())
                continue;
            int itemCount = 0;
            for (const V8TextNode &nested : child.children) {
                if (isItemNode(nested))
                    ++itemCount;
            }
            if (itemCount > 0) {
                for (const V8TextNode &nested : child.children) {
                    if (isItemNode(nested))
                        document->items.append(parseItem(nested));
                }
                break;
            }
        }
    }

    const V8TextNode *attributes = root.child(3);
    if (attributes && attributes->isList()) {
        for (int i = 2; i < attributes->children.size(); ++i) {
            const V8TextNode *entry = attributes->child(i);
            if (!entry || !entry->isList())
                continue;
            FormAttribute attribute;
            attribute.name = findName(*entry, 2);
            if (attribute.name.isEmpty())
                continue;
            attribute.synonym = localizedText(*entry);
            attribute.typeHint = typeHintFromAttributes(*entry);
            if (!attribute.name.isEmpty() && !attribute.synonym.isEmpty())
                document->titles.insert(attribute.name, attribute.synonym);
            document->attributes.append(attribute);
        }
    }

    const V8TextNode *commands = root.child(5);
    if (commands && commands->isList()) {
        for (int i = 2; i < commands->children.size(); ++i) {
            const V8TextNode *entry = commands->child(i);
            if (!entry || !entry->isList() || entry->children.size() < 2)
                continue;
            if (!entry->child(1)->isList())
                continue;
            FormCommand command;
            command.name = findName(*entry, 2);
            if (command.name.isEmpty())
                continue;
            command.synonym = localizedText(*entry);
            document->commands.append(command);
        }
    }

    document->valid = !document->items.isEmpty() || !document->moduleText.isEmpty();
    if (!document->valid) {
        if (error)
            *error = QStringLiteral("Форма не содержит элементов.");
        return false;
    }
    return true;
}

bool FormReader::parseFile(const QString &path, FormDocument *document,
                           QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Не удалось открыть форму %1: %2")
                         .arg(QFileInfo(path).fileName(), file.errorString());
        return false;
    }
    return parseBytes(file.readAll(), document, error);
}

} // namespace v8reader
