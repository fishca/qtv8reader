#include "core/FormReader.h"

#include "core/FormBindings.h"
#include "core/V8Text.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>

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

// Узел макета группы {29, ...}: в нём лежат ориентация (1), выделение
// (3), показ заголовка (4) и поведение (10–12). Значения сверены
// с Form.xml на формах БП.
const V8TextNode *findLayoutNode(const V8TextNode &node)
{
    for (const V8TextNode &child : node.children) {
        if (!child.isList() || child.children.isEmpty())
            continue;
        const V8TextNode *first = child.child(0);
        if (!first)
            continue;
        const bool isLayout = (first->isInteger() && first->toInteger() == 29)
            || (first->isScalar() && first->scalar() == QLatin1String("29"));
        if (isLayout)
            return &child;
    }
    return nullptr;
}

int layoutInteger(const V8TextNode &layout, int index)
{
    const V8TextNode *child = layout.child(index);
    if (!child || !child->isInteger())
        return -1;
    return static_cast<int>(child->toInteger());
}

// Сегмент пути к данным: {индекс}, {код} или {0, guid}.
bool isPathSegment(const V8TextNode &node)
{
    if (!node.isList() || node.children.isEmpty() || node.children.size() > 2)
        return false;
    const V8TextNode *first = node.child(0);
    if (!first || !first->isInteger())
        return false;
    if (node.children.size() == 2) {
        const V8TextNode *second = node.child(1);
        if (!second || !second->isGuid())
            return false;
    }
    return true;
}

// Описание пути к данным (DataPath) поля: {длина, сегменты...}.
// В бинарной форме путь хранится ссылками на реквизиты метаданных,
// а не текстом: {2,{1},{-3}} — «Объект.Дата», {3,{1},{0,guid},{-2}} —
// «Объект.ТабличнаяЧасть.Колонка».
bool isPathDescriptor(const V8TextNode &node)
{
    if (!node.isList() || node.children.size() < 2)
        return false;
    const V8TextNode *count = node.child(0);
    if (!count || !count->isInteger())
        return false;
    const qint64 length = count->toInteger();
    if (length < 1 || length > 8 || node.children.size() != length + 1)
        return false;
    for (int i = 1; i < node.children.size(); ++i) {
        if (!isPathSegment(*node.child(i)))
            return false;
    }
    return true;
}

QVector<FormPathSegment> parsePathDescriptor(const V8TextNode &node)
{
    QVector<FormPathSegment> segments;
    for (int i = 1; i < node.children.size(); ++i) {
        const V8TextNode *child = node.child(i);
        if (!child || !child->isList() || child->children.isEmpty())
            continue;
        FormPathSegment segment;
        if (child->children.size() == 2 && child->child(1)->isGuid()) {
            segment.kind = FormPathSegment::Kind::Attribute;
            segment.guid = child->child(1)->normalizedGuid();
        } else if (child->child(0)->isInteger()) {
            const int code = static_cast<int>(child->child(0)->toInteger());
            segment.code = code;
            segment.kind = code < 0 ? FormPathSegment::Kind::StandardAttribute
                                    : FormPathSegment::Kind::Index;
        } else {
            continue;
        }
        segments.append(segment);
    }
    return segments;
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

// Настройки динамического списка лежат в записи реквизита формы плоским
// набором пар «ключ, значение»: "QueryText", ["S", "ВЫБРАТЬ …"], "MainTable",
// ["#", …], "FieldsMapItemId0", ["N", "5"], "FieldsMapItemName0",
// ["S", "Наименование"]. Ищем список, в котором есть ключ карты полей; часть
// сборок пишет его с опечаткой («FiledsMapItem…»).
bool isListFieldMapKey(const QString &key)
{
    return key.startsWith(QLatin1String("FieldsMapItemId"))
        || key.startsWith(QLatin1String("FiledsMapItemId"));
}

const V8TextNode *findListSettings(const V8TextNode &node, int depth = 0)
{
    if (!node.isList() || depth > 5)
        return nullptr;
    for (const V8TextNode &child : node.children) {
        if (!child.isList())
            continue;
        for (const V8TextNode &slot : child.children) {
            if (slot.isScalar() && isListFieldMapKey(slot.scalar()))
                return &child;
        }
        if (const V8TextNode *nested = findListSettings(child, depth + 1))
            return nested;
    }
    return nullptr;
}

// Значение настройки записано парой «вид, текст»: ["N", "5"], ["S", "…"],
// ["B", "1"]; иногда встречается просто скаляр.
QString settingValue(const V8TextNode &node)
{
    if (node.isScalar())
        return node.scalar();
    if (node.isList() && node.children.size() >= 2 && node.child(1)
        && node.child(1)->isScalar()) {
        return node.child(1)->scalar();
    }
    return QString();
}

// Номер в конце ключа карты полей: «FieldsMapItemId12» → 12.
int settingKeyIndex(const QString &key, const QString &prefix)
{
    if (!key.startsWith(prefix))
        return -1;
    const QString tail = key.mid(prefix.size());
    if (tail.isEmpty())
        return -1;
    for (const QChar ch : tail) {
        if (!ch.isDigit())
            return -1;
    }
    return tail.toInt();
}

// Разбирает настройки динамического списка: карту полей (номер → имя) и
// текст запроса. Номер поля — это второй сегмент пути к данным колонки.
void readListSettings(const V8TextNode &settings, FormAttribute *attribute)
{
    QHash<int, int> fieldIds;      // номер записи карты → номер поля
    QHash<int, QString> fieldNames; // номер записи карты → имя поля
    for (int i = 0; i + 1 < settings.children.size(); ++i) {
        const V8TextNode *key = settings.child(i);
        if (!key || !key->isScalar())
            continue;
        const QString text = key->scalar();
        const QString value = settingValue(*settings.child(i + 1));
        if (value.isEmpty())
            continue;
        int index = settingKeyIndex(text, QStringLiteral("FieldsMapItemId"));
        if (index < 0)
            index = settingKeyIndex(text, QStringLiteral("FiledsMapItemId"));
        if (index >= 0) {
            fieldIds.insert(index, value.toInt());
            continue;
        }
        index = settingKeyIndex(text, QStringLiteral("FieldsMapItemName"));
        if (index < 0)
            index = settingKeyIndex(text, QStringLiteral("FiledsMapItemName"));
        if (index >= 0) {
            fieldNames.insert(index, value);
            continue;
        }
        if (text == QLatin1String("QueryText")) {
            attribute->listQuery = value;
        } else if (text == QLatin1String("ManualQuery")) {
            attribute->listManualQuery = value != QLatin1String("0")
                && value.compare(QLatin1String("false"), Qt::CaseInsensitive) != 0;
        }
    }
    for (auto it = fieldNames.constBegin(); it != fieldNames.constEnd(); ++it) {
        const int fieldId = fieldIds.value(it.key(), -1);
        if (fieldId < 0)
            continue;
        attribute->listFields.insert(fieldId, it.value());
    }
    if (!attribute->listFields.isEmpty()) {
        attribute->dynamicList = true;
        // Вид реквизита в терминах рендерера: по нему он понимает, что
        // таблица — динамический список (команды, поиск, настройки).
        attribute->typeHint = QStringLiteral("ДинамическийСписок");
    }
}

void readGeometry(const V8TextNode &node, int nameIndex, FormItem *item)
{
    auto integerSlot = [](const V8TextNode &record, int index) {
        const V8TextNode *child = record.child(index);
        return child && child->isInteger() ? static_cast<int>(child->toInteger()) : -1;
    };

    // Поля (InputField/LabelField/CheckBoxField/PictureField): геометрия лежит
    // в записи макета, первым элементом которой стоит её код:
    //   36 — поле ввода:   [2] Width, [3] Height, [4] HorizontalStretch,
    //                      [5] VerticalStretch;
    //   11 — метка/флажок: [1] Width, [2] Height, [3] HorizontalStretch,
    //                      [4] VerticalStretch.
    // Раскладка сверена с Form.xml по 229 полям формы «Авансовый отчет»
    // и подтверждена корреляцией по корпусу из 15 форм БП.
    if (item->typeCode == 37) {
        for (const V8TextNode &child : node.children) {
            if (!child.isList() || child.children.size() < 6)
                continue;
            const V8TextNode *first = child.child(0);
            if (!first || !first->isScalar())
                continue;
            const QString code = first->scalar().trimmed();
            if (code == QLatin1String("36")) {
                item->labelOnly = false;
                item->widthUnits = integerSlot(child, 2);
                item->heightUnits = integerSlot(child, 3);
                item->horizontalStretch = integerSlot(child, 4);
                item->verticalStretch = integerSlot(child, 5);
                return;
            }
            if (code == QLatin1String("11")) {
                item->labelOnly = true;
                item->widthUnits = integerSlot(child, 1);
                item->heightUnits = integerSlot(child, 2);
                item->horizontalStretch = integerSlot(child, 3);
                item->verticalStretch = integerSlot(child, 4);
                return;
            }
        }
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

    // Поля: сразу после имени — TitleLocation (0 — None, 1 — Auto,
    // 2 — Left, 3 — Top, 4 — Right, 5 — Bottom), через 37 полей — Visible.
    // Сверено с Form.xml по id (значение Top проверено на форме справочника
    // «Варианты отчетов»).
    if (item.typeCode == 37 && nameIndex >= 0) {
        const V8TextNode *title = node.child(nameIndex + 1);
        if (title && title->isInteger()) {
            const int location = static_cast<int>(title->toInteger());
            if (location >= 0 && location <= 5)
                item.titleLocation = static_cast<FormTitleLocation>(location);
            item.showTitle = item.titleLocation != FormTitleLocation::None;
        }
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

    // Группа: в узле макета {29,...} лежат выделение (3), показ заголовка (4)
    // и поведение (10–12). Обычные группы БП дают 0/1, раскрываемые — 1/1/1,
    // всплывающие — 1/0/0; сверено с Form.xml на четырёх справочниках.
    if (item.type == FormItem::Type::Group) {
        if (const V8TextNode *layout = findLayoutNode(node)) {
            const int representation = layoutInteger(*layout, 3);
            if (representation >= 0 && representation <= 3)
                item.representation = static_cast<FormGroupRepresentation>(representation);
            const int showTitle = layoutInteger(*layout, 4);
            if (showTitle >= 0)
                item.showTitle = showTitle != 0;
            const int behaviorFlag = layoutInteger(*layout, 10);
            const int collapsible = layoutInteger(*layout, 11);
            const int collapsibleSecond = layoutInteger(*layout, 12);
            if (behaviorFlag == 1) {
                item.behavior = (collapsible == 1 && collapsibleSecond == 1)
                    ? FormGroupBehavior::Collapsible
                    : FormGroupBehavior::PopUp;
            }
        }
    }

    // Кнопка: через 10 полей после имени — LocationInCommandBar
    // (0 — только в подменю «Еще», 1 — на панели). Сверено с Form.xml
    // на 30 кнопках пяти объектов БП.
    if (item.typeCode == 31 && nameIndex >= 0) {
        const V8TextNode *onBar = node.child(nameIndex + 10);
        if (onBar && onBar->isInteger())
            item.onCommandBar = onBar->toInteger() != 0;
    }

    // Путь к данным поля: в бинарной форме он записан ссылками на реквизиты
    // метаданных и всегда лежит в нескольких полях после имени элемента.
    // Таблица тоже привязана к данным («Список», «Объект.Товары»), но её
    // описание пути стоит дальше — сразу за заголовком и его расположением.
    if ((item.typeCode == 37 || item.typeCode == 55) && nameIndex >= 0) {
        const int last = std::min<int>(node.children.size() - 1,
                                       nameIndex + (item.typeCode == 55 ? 12 : 8));
        for (int j = nameIndex + 3; j <= last; ++j) {
            const V8TextNode *candidate = node.child(j);
            if (!candidate || !isPathDescriptor(*candidate))
                continue;
            item.dataPath = parsePathDescriptor(*candidate);
            if (!item.dataPath.isEmpty())
                break;
        }
    }

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
                            QString *error, const FormBindingResolver *resolver)
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
            if (const V8TextNode *settings = findListSettings(*entry))
                readListSettings(*settings, &attribute);
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
    if (resolver && !resolver->isEmpty())
        resolver->applyToDocument(document);
    return true;
}

bool FormReader::parseFile(const QString &path, FormDocument *document,
                           QString *error, const FormBindingResolver *resolver)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Не удалось открыть форму %1: %2")
                         .arg(QFileInfo(path).fileName(), file.errorString());
        return false;
    }
    return parseBytes(file.readAll(), document, error, resolver);
}

namespace {

QJsonValue slotToJson(const V8TextNode &node, int depth)
{
    if (node.isScalar())
        return QJsonValue(node.scalar());
    if (!node.isList())
        return QJsonValue();
    if (depth <= 0)
        return QJsonValue(QStringLiteral("..."));
    QJsonArray array;
    for (const V8TextNode &child : node.children)
        array.append(slotToJson(child, depth - 1));
    return array;
}

void appendNodeDescription(const V8TextNode &node, QJsonArray *target, int depth)
{
    for (const V8TextNode &child : node.children) {
        if (!isItemNode(child))
            continue;
        QJsonObject item;
        item.insert(QStringLiteral("typeCode"),
                    static_cast<int>(child.child(0)->toInteger()));
        int nameIndex = -1;
        item.insert(QStringLiteral("name"), findName(child, 2, &nameIndex));
        if (nameIndex >= 0)
            item.insert(QStringLiteral("nameIndex"), nameIndex);
        if (child.child(1)->children.size() >= 1
            && child.child(1)->child(0)->isInteger()) {
            item.insert(QStringLiteral("id"),
                        static_cast<int>(child.child(1)->child(0)->toInteger()));
        }
        QJsonArray slotValues;
        for (const V8TextNode &slot : child.children)
            slotValues.append(slotToJson(slot, 3));
        item.insert(QStringLiteral("slots"), slotValues);
        QJsonArray children;
        if (depth > 1)
            appendNodeDescription(child, &children, depth - 1);
        item.insert(QStringLiteral("childItems"), children);
        target->append(item);
    }
}

} // namespace

QByteArray FormReader::describeNodes(const QByteArray &data, QString *error)
{
    const QString text = V8Text::decode(data, nullptr, nullptr);
    V8TextNode root;
    V8TextError parseError;
    if (text.isEmpty() || !V8Text::parse(text, root, &parseError)) {
        if (error)
            *error = QStringLiteral("Не удалось разобрать форму: %1")
                         .arg(parseError.toString());
        return QByteArray();
    }
    const V8TextNode *header = root.child(1);
    if (!header || !header->isList()) {
        if (error)
            *error = QStringLiteral("В форме отсутствует заголовок (элемент 2).");
        return QByteArray();
    }
    QJsonObject result;
    QJsonArray items;
    appendNodeDescription(*header, &items, 40);
    result.insert(QStringLiteral("items"), items);

    // Раздел реквизитов формы — вне дерева элементов, поэтому в отладочный
    // дамп попадает отдельным списком: по нему видно, как записаны типы
    // реквизитов и настройки динамических списков.
    const V8TextNode *attributes = root.child(3);
    if (attributes && attributes->isList()) {
        QJsonArray attributeList;
        for (int i = 2; i < attributes->children.size(); ++i) {
            const V8TextNode *entry = attributes->child(i);
            if (!entry || !entry->isList())
                continue;
            QJsonObject attribute;
            int nameIndex = -1;
            attribute.insert(QStringLiteral("name"), findName(*entry, 2, &nameIndex));
            if (nameIndex >= 0)
                attribute.insert(QStringLiteral("nameIndex"), nameIndex);
            QJsonArray slotValues;
            for (const V8TextNode &slot : entry->children)
                slotValues.append(slotToJson(slot, 4));
            attribute.insert(QStringLiteral("slots"), slotValues);
            attributeList.append(attribute);
        }
        result.insert(QStringLiteral("attributes"), attributeList);
    }
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

} // namespace v8reader
