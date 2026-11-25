#include "core/FormReader.h"

#include "core/FormBindings.h"
#include "core/FormCommands.h"
#include "core/FormPictures.h"
#include "core/FormStyles.h"
#include "core/V8Text.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>

#include <algorithm>

namespace v8reader {

namespace {

// Поле формы записано одним из двух кодов: 37 (в большинстве форм) и 35
// (в форме «Банки универсального обмена» и ещё нескольких — всего 52 поля
// корпуса). Раскладка записи у них одна и та же: код вида перед именем,
// заголовок и подсказка через три и четыре поля после имени, видимость через
// 37, путь к данным — в полях сразу за заголовком.
bool isFieldCode(int typeCode)
{
    return typeCode == 37 || typeCode == 35;
}

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

QString normalizedText(QString text)
{
    // В контейнере переводы строк записаны парой \r\n, а Form.xml — одним \n.
    // Платформа показывает одинаковый текст, поэтому выравниваем: иначе
    // подсказки и заголовки расходятся с эталоном при сверке моделей.
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    // Строки в контейнере иногда заканчиваются пробелом перед переводом
    // строки, а выгрузка Form.xml такие пробелы не сохраняет. На вид текст
    // одинаковый, поэтому хвостовые пробелы строк убираем.
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString &line : lines) {
        while (!line.isEmpty()
               && (line.endsWith(QLatin1Char(' ')) || line.endsWith(QLatin1Char('\t')))) {
            line.chop(1);
        }
    }
    return lines.join(QLatin1Char('\n'));
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
            return normalizedText(value->scalar());
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

// Представление значения из списка выбора: локализованный текст лежит глубже,
// чем в свойствах элемента ({1, 1, {"ru", "..."}} внутри типизированной
// ссылки), поэтому идём по вложенным спискам до самого значения.
void collectLocalizedDeep(const V8TextNode &node, QString *out, int depth = 0)
{
    if (depth > 8 || !out->isEmpty() || !node.isList())
        return;
    if (node.children.size() == 3) {
        const V8TextNode *first = node.child(0);
        const V8TextNode *second = node.child(1);
        const V8TextNode *pair = node.child(2);
        if (first && first->isInteger() && second && second->isInteger()
            && pair && pair->isList() && pair->children.size() == 2) {
            const V8TextNode *lang = pair->child(0);
            const V8TextNode *text = pair->child(1);
            if (lang && text && lang->isScalar() && text->isScalar()
                && lang->scalar() == QLatin1String("ru") && !text->scalar().isEmpty()) {
                *out = text->scalar();
                return;
            }
        }
    }
    for (const V8TextNode &child : node.children) {
        collectLocalizedDeep(child, out, depth + 1);
        if (!out->isEmpty())
            return;
    }
}

// Список выбора поля хранится в записи макета блоком
// {3, количество, "", значение, флаги, "" , значение, флаги, ...}.
// Представления заданы в самой форме, поэтому подписи переключателей можно
// взять отсюда: у остальных элементов представления приходят из метаданных
// и в форме не записаны (их в контейнере нет вовсе).
bool readChoiceBlock(const V8TextNode &node, QStringList *target, int depth = 0)
{
    if (!node.isList() || depth > 6)
        return false;
    if (node.children.size() >= 5) {
        const V8TextNode *code = node.child(0);
        const V8TextNode *count = node.child(1);
        if (code && code->isScalar() && code->scalar() == QLatin1String("3")
            && count && count->isInteger()) {
            const int items = static_cast<int>(count->toInteger());
            if (items > 0 && node.children.size() == 2 + 3 * items) {
                QStringList names;
                for (int i = 1; i <= items; ++i) {
                    QString text;
                    const V8TextNode *value = node.child(2 * i + 1);
                    if (value)
                        collectLocalizedDeep(*value, &text);
                    names.append(text);
                }
                *target = names;
                return true;
            }
        }
    }
    for (const V8TextNode &child : node.children) {
        if (readChoiceBlock(child, target, depth + 1))
            return true;
    }
    return false;
}

// Узел макета группы {28|29, ...}: в нём лежат ориентация (1), выделение
// (3), показ заголовка (4) и поведение (10–12). Код узла зависит от сборки,
// которой сделана форма: из 199 групп корпуса 165 записаны кодом 29, а 34 —
// кодом 28; по составу полей записи совпадают.
//
// Ссылка на элемент формы — тоже список из двух элементов, {номер, GUID},
// а номер у групп небольшой и часто равен 28 или 29. Поэтому запись с двумя
// полями и нечисловым вторым полем — ссылка, а не макет: без этой проверки
// группа с номером 28/29 теряла ориентацию, поведение, отступы и сквозное
// выравнивание (группа «ГруппаПодтверждениеРасхода» формы «УчетНДС» —
// пример).
const V8TextNode *findLayoutNode(const V8TextNode &node)
{
    for (int i = 2; i < node.children.size(); ++i) {
        const V8TextNode &child = node.children.at(i);
        if (!child.isList() || child.children.size() <= 2)
            continue;
        const V8TextNode *first = child.child(0);
        if (!first)
            continue;
        const bool isLayout = (first->isInteger()
                               && (first->toInteger() == 29 || first->toInteger() == 28))
            || (first->isScalar() && (first->scalar() == QLatin1String("29")
                                      || first->scalar() == QLatin1String("28")));
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
        // Квалификаторы первого значения найденного вида платформа пишет
        // числами сразу за кодом типа: ["S", 40] — длина строки,
        // ["N", 10, 2] — разрядность и дробная часть. Без них модель формы
        // теряет и ширину поля по умолчанию, и кнопку калькулятора у числа.
        QString stringHint;
        QString numberHint;
        for (const V8TextNode &inner : child.children) {
            const bool nested = !inner.isScalar();
            const V8TextNode *codeNode = nested ? inner.child(0) : &inner;
            if (!codeNode || !codeNode->isScalar())
                continue;
            const QString value = codeNode->scalar();
            if (value == QLatin1String("S")) {
                hasString = true;
                if (stringHint.isEmpty() && nested) {
                    const V8TextNode *length = inner.child(1);
                    if (length && length->isInteger() && length->toInteger() > 0)
                        stringHint = QStringLiteral("Строка(%1)").arg(length->toInteger());
                }
            }
            if (value == QLatin1String("#")) hasRef = true;
            if (value == QLatin1String("B")) hasBool = true;
            if (value == QLatin1String("N")) {
                hasNumber = true;
                if (numberHint.isEmpty() && nested) {
                    const V8TextNode *digits = inner.child(1);
                    const V8TextNode *scale = inner.child(2);
                    if (digits && digits->isInteger() && digits->toInteger() > 0) {
                        const qint64 fraction = scale && scale->isInteger()
                            ? scale->toInteger() : 0;
                        numberHint = fraction > 0
                            ? QStringLiteral("Число(%1,%2)").arg(digits->toInteger()).arg(fraction)
                            : QStringLiteral("Число(%1)").arg(digits->toInteger());
                    }
                }
            }
            if (value == QLatin1String("D")) hasDate = true;
        }
        if (hasString) return stringHint.isEmpty() ? QStringLiteral("Строка") : stringHint;
        if (hasRef) return QStringLiteral("Ссылка");
        if (hasBool) return QStringLiteral("Булево");
        if (hasNumber) return numberHint.isEmpty() ? QStringLiteral("Число") : numberHint;
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
// Заголовок и подсказка элемента записаны одинаковыми записями:
// пустая — ["1","0"], с текстом — ["1","1",["ru","…"]]. В дереве рядом
// бывают и другие записи, поэтому проверяем форму, а не только текст.
bool localizedRecord(const V8TextNode &node, QString *text)
{
    // Заголовок и подсказка элемента записаны одинаковыми записями:
    // пустая — ["1","0"], с текстом — ["1","1",["ru","…"]]. В дереве рядом
    // бывают и другие записи, поэтому проверяем форму, а не только текст.
    if (!node.isList() || node.children.size() < 2)
        return false;
    const V8TextNode *flag = node.child(0);
    const V8TextNode *kind = node.child(1);
    if (!flag || !flag->isScalar() || flag->scalar() != QLatin1String("1"))
        return false;
    if (!kind || !kind->isScalar())
        return false;
    const QString code = kind->scalar();
    if (code == QLatin1String("0") && node.children.size() == 2) {
        text->clear();
        return true;
    }
    if (code == QLatin1String("1") && node.children.size() >= 3) {
        *text = localizedText(node);
        return true;
    }
    return false;
}

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
        } else if (text == QLatin1String("MainTableCategory")) {
            // 1 — список построен поверх таблицы объекта метаданных (в
            // MainTable лежит её GUID), 0 — по произвольному запросу.
            attribute->customMainTable = value.trimmed() == QLatin1String("0");
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

// Описание типа колонки таблицы значений: в записи колонки лежит образец
// значения («N, 15, 3» — число, «S, 40» — строка, «B» — булево, «D» — дата).
QString columnTypeText(const V8TextNode &pattern)
{
    if (!pattern.isList() || pattern.children.size() < 2)
        return QString();
    const V8TextNode *tag = pattern.child(0);
    if (!tag || !tag->isScalar())
        return QString();
    const QString code = tag->scalar().trimmed();
    if (code == QLatin1String("N") && pattern.children.size() >= 3) {
        const V8TextNode *digits = pattern.child(1);
        const V8TextNode *fraction = pattern.child(2);
        if (digits && digits->isInteger() && fraction && fraction->isInteger())
            return QStringLiteral("Число(%1,%2)")
                .arg(digits->toInteger())
                .arg(fraction->toInteger());
    }
    if (code == QLatin1String("S") && pattern.children.size() >= 2) {
        const V8TextNode *length = pattern.child(1);
        if (length && length->isInteger())
            return QStringLiteral("Строка(%1)").arg(length->toInteger());
    }
    if (code == QLatin1String("B"))
        return QStringLiteral("Булево");
    if (code == QLatin1String("D"))
        return QStringLiteral("Дата");
    return QString();
}

// Колонки таблицы значений лежат в записи реквизита отдельными элементами
// {5, номер, 0, имя, синоним, образец значения, ...}. Номер совпадает с
// номером в пути колонки («ТаблицаТоваров.#1»), поэтому по нему и
// расшифровываем путь.
void readTableColumns(const V8TextNode &entry, FormAttribute *attribute)
{
    for (const V8TextNode &child : entry.children) {
        if (!child.isList() || child.children.size() < 6)
            continue;
        const V8TextNode *code = child.child(0);
        const V8TextNode *index = child.child(1);
        const V8TextNode *name = child.child(3);
        if (!code || !code->isScalar() || code->scalar() != QLatin1String("5"))
            continue;
        if (!index || !index->isInteger() || !name || !name->isScalar())
            continue;
        const int number = static_cast<int>(index->toInteger());
        const QString columnName = name->scalar().trimmed();
        if (number <= 0 || columnName.isEmpty())
            continue;
        attribute->columnNames.insert(number, columnName);
        const V8TextNode *title = child.child(4);
        if (title) {
            const QString text = localizedText(*title);
            if (!text.isEmpty())
                attribute->columnTitles.insert(number, text);
        }
        const V8TextNode *pattern = child.child(5);
        if (pattern && pattern->isList()) {
            const QString type = columnTypeText(*pattern);
            if (!type.isEmpty())
                attribute->columnTypes.insert(number, type);
        }
    }
    if (!attribute->columnNames.isEmpty())
        return;
    // Список значений: состав колонок задан платформой — значение (0),
    // представление (1), пометка (2) и картинка (3), поэтому в форме они не
    // записаны. Вид значения распознаём по идентификатору типа в образце
    // значения — он один и тот же во всех конфигурациях.
    static const QString kValueListType =
        QStringLiteral("4772b3b4-f4a3-49c0-a1a5-8cb5961511a3");
    for (const V8TextNode &child : entry.children) {
        if (!child.isList() || child.children.size() != 2)
            continue;
        const V8TextNode *code = child.child(0);
        const V8TextNode *pattern = child.child(1);
        if (!code || !code->isScalar() || code->scalar() != QLatin1String("Pattern"))
            continue;
        if (!pattern || !pattern->isList() || pattern->children.size() != 2)
            continue;
        const V8TextNode *tag = pattern->child(0);
        const V8TextNode *type = pattern->child(1);
        if (!tag || !type || !tag->isScalar() || !type->isScalar())
            continue;
        if (tag->scalar() != QLatin1String("#") || !type->scalar().endsWith(kValueListType))
            continue;
        attribute->columnNames.insert(0, QStringLiteral("Value"));
        attribute->columnNames.insert(1, QStringLiteral("Presentation"));
        attribute->columnNames.insert(2, QStringLiteral("Check"));
        // Картинка строки списка значений — путь к ней платформа пишет в
        // RowPictureDataPath третьим номером («…СписокДокументов.Picture»).
        attribute->columnNames.insert(3, QStringLiteral("Picture"));
        break;
    }
}

// Код записи макета поля зависит от вида контрола: 36 и 32 — поле ввода (у
// обоих второй элемент записи — формат значения, поэтому размеры лежат со
// сдвигом на один), 11 — надпись, 10 — поле-картинка, 6 — календарь,
// 13 — поле табличного документа, 3 — графсхема и HTML-документ, 5 — текстовый
// документ, 1 — диаграмма, 2 — ползунок, 4 — индикатор. У флажка своя запись
// 11 — без размеров (в ней на месте ширины и высоты стоят вложенные записи).
// Коды совпадают с видом поля у всех 610 полей корпуса; чужой код у вида поля
// не встречается.
bool isFieldLayoutCode(FormFieldKind kind, const QString &code)
{
    switch (kind) {
    case FormFieldKind::Input:
        return code == QLatin1String("36") || code == QLatin1String("32");
    case FormFieldKind::Label:
    case FormFieldKind::CheckBox:
        return code == QLatin1String("11");
    case FormFieldKind::Picture:
        return code == QLatin1String("10");
    case FormFieldKind::Calendar:
        return code == QLatin1String("6");
    case FormFieldKind::SpreadSheetDocument:
        return code == QLatin1String("13");
    case FormFieldKind::TextDocument:
        return code == QLatin1String("5");
    case FormFieldKind::GraphicalSchema:
    case FormFieldKind::HTMLDocument:
        return code == QLatin1String("3");
    case FormFieldKind::Chart:
        return code == QLatin1String("1");
    case FormFieldKind::TrackBar:
        return code == QLatin1String("2");
    case FormFieldKind::ProgressBar:
        return code == QLatin1String("4");
    default:
        // Вид не прочитался — принимаем записи тех видов, у которых размеры
        // стоят на первых местах.
        return code == QLatin1String("36") || code == QLatin1String("32")
            || code == QLatin1String("11") || code == QLatin1String("10")
            || code == QLatin1String("6");
    }
}

// Растяжение (HorizontalStretch/VerticalStretch) лежит в записи макета только
// у полей ввода (36 и 32), надписей и флажков (11), поля-картинки (10) и
// календаря (6): у остальных видов на этих местах стоят другие числа (у поля
// табличного документа — признаки полос прокрутки). Сверено с Form.xml по
// 610 полям корпуса.
bool fieldLayoutHasStretch(const QString &code)
{
    return code == QLatin1String("36") || code == QLatin1String("32")
        || code == QLatin1String("11") || code == QLatin1String("10")
        || code == QLatin1String("6");
}

// Блок пределов размера занимает в записи пять полей подряд:
// AutoMaxWidth, MaxWidth, (поле не используется), AutoMaxHeight, MaxHeight.
// Положение блока зависит от вида поля и кода записи макета: у поля ввода
// (36/32) — с сорок девятого поля, у надписи и флажка (11) — с пятнадцатого,
// у HTML-документа (3) — с шестого, у форматированного документа (1) — с
// одиннадцатого, у поля табличного документа (13) — с двадцатого. У остальных
// видов (поле-картинка, календарь, графсхема, диаграмма, PDF-документ) блок
// на этих местах не лежит, поэтому пределы для них не читаются.
// Сверено с Form.xml: HTML-документы — по формам «Реклама/ФормаСписка»,
// «Номенклатура/ФормаСписка» и «Новости/ФормаДокумента», поле табличного
// документа — по «МашиночитаемыеДоверенностиКонтрагентов/ФормаЭлемента»,
// форматированный документ — по «ДоговорыКонтрагентов/ФормаПечатиЭлектронного
// Формата», надпись и флажок — по 171 записи корпуса, поле ввода — по 326.
int fieldLimitsBlockStart(FormFieldKind kind, const QString &code)
{
    switch (kind) {
    case FormFieldKind::Input:
        if (code == QLatin1String("36") || code == QLatin1String("32"))
            return 49;
        return -1;
    case FormFieldKind::Label:
    case FormFieldKind::CheckBox:
        return code == QLatin1String("11") ? 15 : -1;
    case FormFieldKind::HTMLDocument:
        return code == QLatin1String("3") ? 6 : -1;
    case FormFieldKind::FormattedDocument:
        return code == QLatin1String("1") ? 11 : -1;
    case FormFieldKind::SpreadSheetDocument:
        return code == QLatin1String("13") ? 20 : -1;
    default:
        break;
    }
    return -1;
}

// Читает блок пределов размера из записи макета: признаки авторазмера
// платформа пишет значением false (по умолчанию true), пределы — только
// заданными. Незаполненные поля записи не трогают значения элемента.
void readFieldSizeLimits(const V8TextNode &layout, int start, FormItem *item)
{
    if (start < 0)
        return;
    const auto codeAt = [&layout](int index) {
        const V8TextNode *child = layout.child(index);
        return child && child->isInteger() ? static_cast<int>(child->toInteger()) : -1;
    };
    const int autoMaxWidth = codeAt(start);
    if (autoMaxWidth >= 0)
        item->autoMaxWidth = autoMaxWidth != 0;
    const int maxWidth = codeAt(start + 1);
    if (maxWidth > 0)
        item->maxWidth = maxWidth;
    const int autoMaxHeight = codeAt(start + 3);
    if (autoMaxHeight >= 0)
        item->autoMaxHeight = autoMaxHeight != 0;
    const int maxHeight = codeAt(start + 4);
    if (maxHeight > 0)
        item->maxHeight = maxHeight;
}

// Выравнивание элемента внутри группы (GroupHorizontalAlign и
// GroupVerticalAlign) лежит в «хвосте» записи элемента, поэтому отсчёт ведётся
// от конца записи: необязательные вложенные записи (контекстное меню,
// расширенная подсказка, оформление) его не сдвигают. Смещения с конца:
// у группы (22) — 3 и 2, у декорации (12) — 4 и 3, у поля (37) — 6 и 5.
// У поля, записанного кодом 35, раскладка короче: горизонтальное выравнивание
// лежит третьим с конца, а вертикального в корпусе не встретилось.
// Коды: 0 — Left/Top, 1 — Center, 2 — Right/Bottom; 3 — Auto (значение по
// умолчанию, платформа в Form.xml его не пишет). Сверено с Form.xml по 112
// декорациям, 361 группе и 610 полям корпуса: расхождений нет.
void readGroupAlignment(const V8TextNode &node, FormItem *item)
{
    int horizontalFromEnd = -1;
    int verticalFromEnd = -1;
    if (item->typeCode == 22) {
        horizontalFromEnd = 3;
        verticalFromEnd = 2;
    } else if (item->typeCode == 12) {
        horizontalFromEnd = 4;
        verticalFromEnd = 3;
    } else if (item->typeCode == 35) {
        horizontalFromEnd = 3;
    } else if (item->typeCode == 37) {
        horizontalFromEnd = 6;
        verticalFromEnd = 5;
    } else {
        return;
    }
    const int count = static_cast<int>(node.children.size());
    // У некоторых полей (PDF-документ) запись заканчивается вложенной записью и
    // её признаком — они не входят в «хвост», поэтому смещение сдвигается на
    // два слота.
    int shift = 0;
    if (count >= 2) {
        const V8TextNode *beforeLast = node.child(count - 2);
        if (beforeLast && beforeLast->isList())
            shift = 2;
    }
    const auto tailCode = [&node, count, shift](int fromEnd) {
        if (fromEnd <= 0)
            return -1;
        const int index = count - shift - fromEnd;
        if (index < 0)
            return -1;
        const V8TextNode *slot = node.child(index);
        return slot && slot->isInteger() ? static_cast<int>(slot->toInteger()) : -1;
    };
    const int horizontal = tailCode(horizontalFromEnd);
    const int vertical = tailCode(verticalFromEnd);
    if (horizontal >= 0 && horizontal <= 2)
        item->groupHorizontalAlign = horizontal;
    if (vertical >= 0 && vertical <= 2)
        item->groupVerticalAlign = vertical;
}

void readGeometry(const V8TextNode &node, int nameIndex, FormItem *item)
{
    readGroupAlignment(node, item);
    auto integerSlot = [](const V8TextNode &record, int index) {
        const V8TextNode *child = record.child(index);
        return child && child->isInteger() ? static_cast<int>(child->toInteger()) : -1;
    };

    // Поля: размеры лежат в записи макета, первым элементом которой стоит её
    // код. У записи два варианта раскладки: если второй элемент — список
    // (формат значения, например {3,0}), размеры идут со сдвигом на один:
    //   36/32: [1] формат, [2] Width, [3] Height, [4] HStretch, [5] VStretch;
    //   11/10/6: [1] Width, [2] Height, [3] HStretch, [4] VStretch;
    //   13/3: [1] Width, [2] Height, дальше — признаки вида поля.
    // Сверено с Form.xml по корпусу: ширина и высота совпали у всех полей,
    // растяжение — у полей ввода и надписей.
    if (isFieldCode(item->typeCode)) {
        for (const V8TextNode &child : node.children) {
            if (!child.isList() || child.children.size() < 5)
                continue;
            const V8TextNode *first = child.child(0);
            if (!first || !first->isScalar())
                continue;
            const QString code = first->scalar().trimmed();
            if (!isFieldLayoutCode(item->fieldKind, code))
                continue;
            const V8TextNode *second = child.child(1);
            const int base = (second && second->isList()) ? 2 : 1;
            int baseWidth = integerSlot(child, base);
            int baseHeight = integerSlot(child, base + 1);
            // Запись с тем же кодом, но без чисел на месте размеров — это
            // другая запись (например, привязка действия к полю); ищем дальше.
            if (baseWidth < 0 && baseHeight < 0)
                continue;
            // У редких видов полей (графсхема, поле табличного документа,
            // диаграмма, ползунок, индикатор, текстовый документ) запись макета
            // хранит и действующие размеры, а не только заданные, поэтому
            // размеры по умолчанию (50×10 у графсхемы, табличного и текстового
            // документа, диаграммы; высота 1 у ползунка и индикатора) в модель
            // не идут — платформа их в Form.xml не пишет.
            if (code == QLatin1String("13") || code == QLatin1String("3")) {
                if (baseWidth == 50)
                    baseWidth = -1;
                if (baseHeight == 10)
                    baseHeight = -1;
            } else if (code == QLatin1String("1") || code == QLatin1String("5")) {
                if (baseHeight == 10)
                    baseHeight = -1;
            } else if (code == QLatin1String("4")) {
                // Индикатор: высота 1 — размер по умолчанию.
                if (baseHeight == 1)
                    baseHeight = -1;
            }
            if (baseWidth > 0)
                item->widthUnits = baseWidth;
            if (baseHeight > 0)
                item->heightUnits = baseHeight;
            // У флажка запись макета (11) короче и без размеров — там на месте
            // растяжения стоят другие записи, поэтому растяжение читаем только
            // когда на месте ширины и высоты действительно числа.
            if (fieldLayoutHasStretch(code) && baseWidth >= 0 && baseHeight >= 0) {
                item->horizontalStretch = integerSlot(child, base + 2);
                item->verticalStretch = integerSlot(child, base + 3);
            }
            if (item->fieldKind == FormFieldKind::Unknown)
                item->labelOnly = code == QLatin1String("11")
                    || code == QLatin1String("10");
            if (item->widthUnits > 0 || item->heightUnits > 0)
                return;
        }
    }

    // Группа: ширина записана через шесть полей после имени, высота — через
    // семь, растяжение — через восемь и девять (сверено с Form.xml по всем
    // 361 группе, странице, страницам, панели и всплывающей группе корпуса).
    if (item->typeCode == 22 && nameIndex >= 0) {
        if (const V8TextNode *width = node.child(nameIndex + 6)) {
            if (width->isInteger() && width->toInteger() > 0)
                item->widthUnits = static_cast<int>(width->toInteger());
        }
        if (const V8TextNode *height = node.child(nameIndex + 7)) {
            if (height->isInteger() && height->toInteger() > 0)
                item->heightUnits = static_cast<int>(height->toInteger());
        }
        if (const V8TextNode *horizontal = node.child(nameIndex + 8)) {
            if (horizontal->isInteger())
                item->horizontalStretch = static_cast<int>(horizontal->toInteger());
        }
        if (const V8TextNode *vertical = node.child(nameIndex + 9)) {
            if (vertical->isInteger())
                item->verticalStretch = static_cast<int>(vertical->toInteger());
        }
        return;
    }

    // Таблица: ширина и высота записаны через 14 и 15 полей после имени
    // (сверено с Form.xml: у таблиц «Шаблон заполнения» 45×7 и «Сортировка» 6),
    // растяжение — через 16 и 17. Растяжение есть только у таблицы с заданными
    // шириной и высотой: платформа пишет его только тогда, когда у таблицы
    // задан размер (у «Сортировки» задана одна высота, и растяжения нет).
    if (item->typeCode == 55 && nameIndex >= 0) {
        if (const V8TextNode *width = node.child(nameIndex + 14)) {
            if (width->isInteger() && width->toInteger() > 0)
                item->widthUnits = static_cast<int>(width->toInteger());
        }
        if (const V8TextNode *height = node.child(nameIndex + 15)) {
            if (height->isInteger() && height->toInteger() > 0)
                item->heightUnits = static_cast<int>(height->toInteger());
        }
        if (item->widthUnits > 0 && item->heightUnits > 0) {
            if (const V8TextNode *horizontal = node.child(nameIndex + 16)) {
                if (horizontal->isInteger())
                    item->horizontalStretch = static_cast<int>(horizontal->toInteger());
            }
            if (const V8TextNode *vertical = node.child(nameIndex + 17)) {
                if (vertical->isInteger())
                    item->verticalStretch = static_cast<int>(vertical->toInteger());
            }
        }
        return;
    }

    // Декорация: ширина и высота лежат через четыре и пять полей после имени,
    // растяжение — через шесть и семь (сверено с Form.xml: ширина — 32
    // декорации, высота — 18, растяжение — 73 надписи и 39 картинок).
    if (item->typeCode == 12 && nameIndex >= 0) {
        if (const V8TextNode *width = node.child(nameIndex + 4)) {
            if (width->isInteger() && width->toInteger() > 0)
                item->widthUnits = static_cast<int>(width->toInteger());
        }
        if (const V8TextNode *height = node.child(nameIndex + 5)) {
            if (height->isInteger() && height->toInteger() > 0)
                item->heightUnits = static_cast<int>(height->toInteger());
        }
        if (const V8TextNode *horizontal = node.child(nameIndex + 6)) {
            if (horizontal->isInteger())
                item->horizontalStretch = static_cast<int>(horizontal->toInteger());
        }
        if (const V8TextNode *vertical = node.child(nameIndex + 7)) {
            if (vertical->isInteger())
                item->verticalStretch = static_cast<int>(vertical->toInteger());
        }
    }
}

// «Видимость для пользователя» (UserVisible) записана прямо перед именем
// элемента записью {"0", {"0", {"B", 0|1}, "0"}}: у полей, групп, кнопок,
// декораций и дополнений — вторым элементом перед именем (первым идёт код
// вида поля/группы), у таблицы — первым. Ноль скрывает элемент уже при первом
// показе формы, единица — значение по умолчанию, и Form.xml свойство тогда не
// пишет. Сверено с Form.xml по 26 формам корпуса: набор элементов со скрытой
// пользовательской видимостью совпал полностью (25 элементов), и ни один
// элемент без признака не попал в набор.
void readUserVisible(const V8TextNode &node, int nameIndex, FormItem *item)
{
    if (nameIndex < 3)
        return;
    const int offset = item->typeCode == 55 ? 1 : 2;
    const V8TextNode *block = node.child(nameIndex - offset);
    if (!block || !block->isList() || block->children.size() < 2)
        return;
    const V8TextNode *marker = block->child(0);
    const V8TextNode *inner = block->child(1);
    if (!marker || !marker->isScalar()
        || marker->scalar().trimmed() != QLatin1String("0")) {
        return;
    }
    if (!inner || !inner->isList() || inner->children.size() < 3)
        return;
    const V8TextNode *innerMarker = inner->child(0);
    const V8TextNode *value = inner->child(1);
    const V8TextNode *innerTail = inner->child(2);
    if (!innerMarker || !innerMarker->isScalar()
        || innerMarker->scalar().trimmed() != QLatin1String("0")) {
        return;
    }
    if (!innerTail || !innerTail->isScalar()
        || innerTail->scalar().trimmed() != QLatin1String("0")) {
        return;
    }
    if (!value || !value->isList() || value->children.size() < 2)
        return;
    const V8TextNode *tag = value->child(0);
    const V8TextNode *flag = value->child(1);
    if (!tag || !tag->isScalar() || tag->scalar().trimmed() != QLatin1String("B"))
        return;
    if (!flag)
        return;
    if (flag->isInteger())
        item->userVisible = static_cast<int>(flag->toInteger());
    else if (flag->isScalar())
        item->userVisible = flag->scalar().trimmed() == QLatin1String("0") ? 0 : 1;
}

// Признаки декорации, которых нет в её имени: вид (надпись или картинка),
// гиперссылка и картинка. Все они лежат в записи оформления — через 12 полей
// после имени. Запись начинается тегом: «5» у надписи (тогда второй элемент —
// признак гиперссылки) и «4» у картинки (тогда второй элемент — описание
// рамки, третий — признак гиперссылки, а ссылка на картинку лежит внутри
// рамки третьим элементом: отрицательное число — стандартная картинка
// платформы, {0, GUID} — общая картинка конфигурации).
// Сверено с Form.xml по 112 декорациям 26 форм: вид, гиперссылка и картинка
// совпали во всех.
// Ссылка на картинку из записи картинки: {4, вид, {полезная нагрузка}, ...}.
// Полезная нагрузка — {0, GUID} у общей картинки конфигурации (GUID может
// быть и картинкой библиотеки платформы) или {код} у стандартной картинки
// платформы. Картинка может быть встроена прямо в форму: тогда в записи
// лежит base64 BMP, а Form.xml называет её «Abs:<свойство>.bmp».
struct ParsedPicture
{
    QString reference;    // «CommonPicture.Имя», «StdPicture.Имя» или пусто
    QString inlineData;   // base64 BMP для встроенной картинки
};

QString findBase64Picture(const V8TextNode &node)
{
    if (node.isScalar()) {
        const QString value = node.scalar().trimmed();
        const int marker = value.indexOf(QStringLiteral("#base64:"));
        if (marker >= 0) {
            QString payload = value.mid(marker + 8);
            payload.remove(QLatin1Char('\r'));
            payload.remove(QLatin1Char('\n'));
            payload.remove(QLatin1Char(' '));
            return payload;
        }
    }
    for (const V8TextNode &child : node.children) {
        const QString data = findBase64Picture(child);
        if (!data.isEmpty())
            return data;
    }
    return QString();
}

ParsedPicture pictureFromRecord(const V8TextNode &record,
                               const FormPictureTable *pictures,
                               int *standardCode = nullptr)
{
    ParsedPicture parsed;
    if (standardCode)
        *standardCode = 0;
    if (!record.isList() || record.children.size() < 3)
        return parsed;
    const V8TextNode *tag = record.child(0);
    if (!tag || !tag->isScalar() || tag->scalar().trimmed() != QLatin1String("4"))
        return parsed;
    const V8TextNode *payload = record.child(2);
    if (payload && payload->isList() && !payload->children.isEmpty()) {
        if (payload->children.size() >= 2) {
            const V8TextNode *guid = payload->child(1);
            if (guid && guid->isGuid()) {
                parsed.reference = pictures
                    ? pictures->reference(guid->normalizedGuid()) : QString();
                return parsed;
            }
        } else {
            const V8TextNode *code = payload->child(0);
            if (code && code->isInteger()) {
                const int value = static_cast<int>(code->toInteger());
                // Отрицательный код — стандартная картинка платформы;
                // ноль и положительные числа картинкой не задают
                // («нет картинки»), ниже ищем встроенную.
                if (value < 0) {
                    if (standardCode)
                        *standardCode = value;
                    parsed.reference = FormPictureTable::standardPictureName(value);
                    return parsed;
                }
            }
        }
    }
    // Встроенная картинка: её байты записаны в самой форме.
    parsed.inlineData = findBase64Picture(record);
    return parsed;
}

void readDecorationExtras(const V8TextNode &node, int nameIndex,
                          const FormPictureTable *pictures, FormItem *item)
{
    if (nameIndex >= 1) {
        const V8TextNode *kind = node.child(nameIndex - 1);
        if (kind && kind->isInteger())
            item->pictureDecoration = kind->toInteger() != 0;
    }
    // Размер картинки (PictureSize) лежит в записи оформления {4,…} третьим
    // полем: 0 — не задан (платформа свойства не пишет), 1 — Stretch,
    // 2 — Proportionally. Сверено с Form.xml по 39 декорациям-картинкам
    // корпуса: 2 с Stretch и 5 с Proportionally совпали.
    if (item->pictureDecoration) {
        if (const V8TextNode *size = node.child(nameIndex + 12)) {
            if (size->isList() && size->children.size() > 3) {
                const V8TextNode *value = size->child(3);
                if (value && value->isInteger() && value->toInteger() >= 1
                    && value->toInteger() <= 2) {
                    item->pictureSize = static_cast<int>(value->toInteger());
                }
            }
        }
    }
    // Пределы авторазмера декорации: двадцать первое поле после имени —
    // AutoMaxWidth, двадцать четвёртое — AutoMaxHeight (0 — false, 1 —
    // значение по умолчанию true). Платформа пишет обе свойства только
    // значением false. Сверено с Form.xml по 112 декорациям корпуса: 18 надписей
    // и одна картинка с AutoMaxWidth, три надписи с AutoMaxHeight.
    if (const V8TextNode *autoMaxWidth = node.child(nameIndex + 21);
        autoMaxWidth && autoMaxWidth->isInteger()) {
        item->autoMaxWidth = autoMaxWidth->toInteger() != 0;
    }
    if (const V8TextNode *autoMaxHeight = node.child(nameIndex + 24);
        autoMaxHeight && autoMaxHeight->isInteger()) {
        item->autoMaxHeight = autoMaxHeight->toInteger() != 0;
    }
    // Пределы размера декорации стоят в том же блоке: двадцать второе поле
    // после имени — MaxWidth, двадцать пятое — MaxHeight. Сверено с Form.xml по
    // надписи «НавигацияКартинка» (90 и 3) и картинке «ДекорацияОжидание»
    // (4 и 1) и по 112 декорациям корпуса.
    if (const V8TextNode *maxWidth = node.child(nameIndex + 22);
        maxWidth && maxWidth->isInteger() && maxWidth->toInteger() > 0) {
        item->maxWidth = static_cast<int>(maxWidth->toInteger());
    }
    if (const V8TextNode *maxHeight = node.child(nameIndex + 25);
        maxHeight && maxHeight->isInteger() && maxHeight->toInteger() > 0) {
        item->maxHeight = static_cast<int>(maxHeight->toInteger());
    }

    const V8TextNode *appearance = node.child(nameIndex + 12);
    if (!appearance || !appearance->isList() || appearance->children.size() < 3)
        return;
    // Форматированный заголовок: через 17 полей после имени лежит запись
    // {1, <заголовок>, признак}. В ней же хранится размеченный вариант
    // заголовка (шрифт, цвет, ссылки), поэтому при признаке true заголовком
    // становится именно он. Сверено с Form.xml по 100 декорациям корпуса:
    // признак совпал во всех, размеченный текст есть у 20 из них.
    if (const V8TextNode *formatted = node.child(nameIndex + 17)) {
        if (formatted->isList() && formatted->children.size() == 3) {
            const V8TextNode *flag = formatted->child(2);
            if (flag && flag->isScalar()
                && (flag->scalar() == QLatin1String("0")
                    || flag->scalar() == QLatin1String("1"))) {
                const QString text = localizedText(*formatted);
                // Признак «не форматированный» при пустом заголовке платформа
                // не пишет вовсе: оформлять нечего (12 декораций корпуса).
                if (!text.isEmpty() || flag->scalar() == QLatin1String("1")) {
                    item->hasTitleFormatted = true;
                    item->titleFormatted = flag->scalar() == QLatin1String("1");
                    if (item->titleFormatted && !text.isEmpty())
                        item->synonym = text;
                }
            }
        }
    }
    const V8TextNode *tag = appearance->child(0);
    if (!tag || !tag->isScalar())
        return;
    const bool pictured = tag->scalar().trimmed() == QLatin1String("4");
    // Выравнивание содержимого декорации: в записи оформления {5,…} второе
    // поле — горизонтальное (0 — Left, значение по умолчанию; 1 — Center,
    // 2 — Right), третье — вертикальное (0 — Top, 1 — Center, 2 — Bottom,
    // 3 — значение по умолчанию). Сверено с Form.xml по 73 надписям корпуса.
    if (!pictured) {
        if (const V8TextNode *horizontal = appearance->child(2);
            horizontal && horizontal->isInteger()) {
            const int value = static_cast<int>(horizontal->toInteger());
            if (value == 1 || value == 2)
                item->contentHorizontalAlign = value;
        }
        if (const V8TextNode *vertical = appearance->child(3);
            vertical && vertical->isInteger()) {
            const int value = static_cast<int>(vertical->toInteger());
            if (value >= 0 && value <= 2)
                item->contentVerticalAlign = value;
        }
    }
    const V8TextNode *flag = appearance->child(pictured ? 2 : 1);
    if (flag && flag->isInteger())
        item->hyperlink = flag->toInteger() != 0;
    // Режим перетаскивания файлов: 0 — AsFile, 1 — UseAsFile (по умолчанию);
    // в записи оформления через 11 полей. Сверено с Form.xml по 39 декорациям
    // корпуса.
    if (const V8TextNode *drag = appearance->child(11); drag && drag->isInteger())
        item->fileDragMode = static_cast<int>(drag->toInteger());
    if (!pictured)
        return;

    const V8TextNode *border = appearance->child(1);
    if (!border || !border->isList() || border->children.size() < 3)
        return;
    const ParsedPicture picture =
        pictureFromRecord(*border, pictures, &item->pictureStandardCode);
    if (!picture.reference.isEmpty()) {
        item->pictureRef = picture.reference;
        return;
    }
    if (!picture.inlineData.isEmpty()) {
        item->inlinePictures.insert(QStringLiteral("Picture.bmp"), picture.inlineData);
        item->pictureRef = QStringLiteral("Abs:Picture.bmp");
    }
}

// Цвет из записи формата. Запись начинается кодом 3, затем идёт вид значения:
//   0 — абсолютный цвет числом (0x00BBGGRR, как COLORREF Windows);
//   2 — web-цвет номером известного цвета (48 — Gainsboro);
//   3 — ссылка на стиль: {0, GUID} у общего стиля конфигурации, {код} у стиля
//       платформы (отрицательное число);
//   4 — значение не задано (по умолчанию).
// Сверено с Form.xml по корпусу: абсолютные цвета — #0000FF у надписи
// «ПозицииМННГиперСсылка» (число 16711680 = 0xFF0000), #999999 — у поля
// картинки; стили конфигурации — 40 элементов, стили платформы — BorderColor,
// FormBackColor, ToolTipBackColor, TableHeaderBackColor и AccentColor.
QString colorFromRecord(const V8TextNode &record, const FormStyleTable *styles)
{
    if (!record.isList() || record.children.size() < 2)
        return QString();
    const V8TextNode *tag = record.child(0);
    if (!tag || !tag->isScalar() || tag->scalar().trimmed() != QLatin1String("3"))
        return QString();
    const V8TextNode *mode = record.child(1);
    if (!mode || !mode->isScalar())
        return QString();
    const QString modeText = mode->scalar().trimmed();

    if (modeText == QLatin1String("0") || modeText == QLatin1String("2")) {
        const V8TextNode *payload = record.child(2);
        const V8TextNode *value = payload && payload->isList() ? payload->child(0) : nullptr;
        if (!value || !value->isInteger())
            return QString();
        if (modeText == QLatin1String("2")) {
            const QString name = FormStyleTable::webColorName(
                static_cast<int>(value->toInteger()));
            return name.isEmpty() ? QString() : QStringLiteral("web:") + name;
        }
        const qint64 color = value->toInteger();
        if (color <= 0)
            return QString();
        const int red = static_cast<int>(color & 0xFF);
        const int green = static_cast<int>((color >> 8) & 0xFF);
        const int blue = static_cast<int>((color >> 16) & 0xFF);
        return QStringLiteral("#%1%2%3")
            .arg(red, 2, 16, QLatin1Char('0'))
            .arg(green, 2, 16, QLatin1Char('0'))
            .arg(blue, 2, 16, QLatin1Char('0'))
            .toUpper();
    }
    if (modeText != QLatin1String("3"))
        return QString();

    const V8TextNode *payload = record.child(2);
    if (!payload || !payload->isList())
        return QString();
    if (payload->children.size() >= 2) {
        const V8TextNode *guid = payload->child(1);
        if (guid && guid->isGuid())
            return styles ? styles->reference(guid->normalizedGuid()) : QString();
        return QString();
    }
    const V8TextNode *code = payload->child(0);
    if (code && code->isInteger()) {
        const QString name = FormStyleTable::platformStyleName(
            static_cast<int>(code->toInteger()));
        if (!name.isEmpty())
            return QStringLiteral("style:") + name;
    }
    return QString();
}

// Шрифт из записи формата. Запись начинается кодом 7, вторым элементом идёт
// версия (всегда 2), третьим — маска заданных атрибутов, четвёртым — ссылка
// на стиль: {0, GUID} — общий стиль шрифта конфигурации, {код} — стиль
// платформы. Дальше идут только отмеченные маской значения: высота в десятых
// долях пункта (2), вес шрифта 400/700 (4), курсив (8), подчёркивание (16),
// зачёркивание (32), служебная единица, масштаб в процентах (512).
// Сверено с Form.xml: запись {7,2,60,…} — «жирный» 700 при выключенных
// остальных; {7,2,572,…} — явные bold/italic/underline/strikeout и масштаб
// 100; {7,2,2,…,160,…} — высота 16; {7,2,512,…,90} — масштаб 90.
FormFont fontFromRecord(const V8TextNode &record, const FormStyleTable *styles)
{
    FormFont font;
    if (!record.isList() || record.children.size() < 4)
        return font;
    const V8TextNode *tag = record.child(0);
    if (!tag || !tag->isScalar() || tag->scalar().trimmed() != QLatin1String("7"))
        return font;
    const V8TextNode *flagsNode = record.child(2);
    const int flags = flagsNode && flagsNode->isInteger()
        ? static_cast<int>(flagsNode->toInteger()) : 0;
    const V8TextNode *payload = record.child(3);
    if (!payload || !payload->isList())
        return font;
    if (payload->children.size() >= 2) {
        const V8TextNode *guid = payload->child(1);
        if (!guid || !guid->isGuid() || !styles)
            return font;
        font.reference = styles->reference(guid->normalizedGuid());
    } else {
        const V8TextNode *code = payload->child(0);
        if (!code || !code->isInteger())
            return font;
        const QString name = FormStyleTable::platformStyleName(
            static_cast<int>(code->toInteger()));
        if (name.isEmpty())
            return font;
        font.reference = QStringLiteral("style:") + name;
    }

    int pos = 4;
    auto value = [&record, &pos](int *out) {
        const V8TextNode *node = record.child(pos);
        ++pos;
        if (node && node->isInteger())
            *out = static_cast<int>(node->toInteger());
    };
    if (flags & 2) {
        int tenths = 0;
        value(&tenths);
        font.height = tenths / 10;
    }
    if (flags & 60) {
        // Вес шрифта: 700 — жирный, 400 — обычный.
        int weight = 0;
        value(&weight);
        font.bold = weight >= 700 ? 1 : 0;
        value(&font.italic);
        value(&font.underline);
        value(&font.strikeout);
    }
    // Служебная единица перед масштабом.
    ++pos;
    if (flags & 512) {
        int scale = 0;
        value(&scale);
        font.scale = scale;
    }
    return font;
}

QString colorAt(const V8TextNode &node, int index, const FormStyleTable *styles)
{
    const V8TextNode *child = node.child(index);
    return child ? colorFromRecord(*child, styles) : QString();
}

FormFont fontAt(const V8TextNode &node, int index, const FormStyleTable *styles)
{
    const V8TextNode *child = node.child(index);
    return child ? fontFromRecord(*child, styles) : FormFont();
}

// Запись макета поля: список, первый элемент которого — код раскладки
// (36 — поле ввода, 11 — надпись и флажок, 10 — поле-картинка, 6 — календарь).
// Ссылка на сам элемент тоже начинается с числа ({id, GUID}), поэтому запись
// макета отличается ещё и длиной: у неё не меньше восьми полей.
const V8TextNode *findFieldLayout(const V8TextNode &node, QString *code)
{
    for (const V8TextNode &child : node.children) {
        if (!child.isList() || child.children.size() < 8)
            continue;
        const V8TextNode *first = child.child(0);
        if (!first || !first->isScalar())
            continue;
        const QString value = first->scalar().trimmed();
        // Запись макета поля: {36,…} — поле ввода, {32,…} — то же для полей
        // некоторых видов (период, число с кнопками), {11,…} — надпись и
        // флажок, {10,…} — поле-картинка, {6,…} — прочие. Поля записи 32
        // совпадают по смыслу с 36 (сверено с Form.xml по 4 полям с
        // SpinButton и MinValue).
        if (value == QLatin1String("36") || value == QLatin1String("32")
            || value == QLatin1String("11") || value == QLatin1String("10")
            || value == QLatin1String("6")) {
            if (code)
                *code = value;
            return &child;
        }
    }
    return nullptr;
}

// Оформление поля. В записи макета три цвета (текст, фон, рамка) и шрифт:
//   36: 37 — TextColor, 38 — BackColor, 39 — BorderColor, 40 — Font;
//   11 и 6: 8 — TextColor, 9 — BackColor, 10 — Font;
//   10 (поле-картинка): 10 — TextColor, 11 — BackColor, 12 — Font.
// Цвет заголовка и шрифт заголовка записаны не в макете, а в записи элемента
// через 25 и 26 полей после имени.
// Сверено с Form.xml: поле ввода с фоном «ФонУправляющегоПоля» (справочник
// «Виды контактной информации»), поле-картинка с цветом #999999 и шрифтом,
// надпись с фоном и цветом заголовка («Организации»), поля «Авансового
// отчёта» с цветом и шрифтом.
void readFieldStyles(const V8TextNode &node, int nameIndex,
                     const FormStyleTable *styles, FormItem *item)
{
    if (!styles || styles->isEmpty())
        return;
    QString code;
    if (const V8TextNode *layout = findFieldLayout(node, &code)) {
        if (code == QLatin1String("36")) {
            item->textColor = colorAt(*layout, 37, styles);
            item->backColor = colorAt(*layout, 38, styles);
            item->borderColor = colorAt(*layout, 39, styles);
            item->font = fontAt(*layout, 40, styles);
        } else if (code == QLatin1String("11") || code == QLatin1String("6")) {
            item->textColor = colorAt(*layout, 8, styles);
            item->backColor = colorAt(*layout, 9, styles);
            item->font = fontAt(*layout, 10, styles);
        } else if (code == QLatin1String("10")) {
            item->textColor = colorAt(*layout, 10, styles);
            item->backColor = colorAt(*layout, 11, styles);
            item->font = fontAt(*layout, 12, styles);
        }
    }
    if (nameIndex >= 0) {
        item->titleTextColor = colorAt(node, nameIndex + 25, styles);
        item->titleFont = fontAt(node, nameIndex + 26, styles);
    }
}

// Оформление группы: фон лежит в узле макета девятым полем, цвет и шрифт
// заголовка — через 10 и 11 полей после имени. Сверено с Form.xml: группы
// «Авансового отчёта» с фоном баннера, «Новостей» с цветом и шрифтом
// заголовка.
void readGroupStyles(const V8TextNode &node, int nameIndex,
                     const FormStyleTable *styles, FormItem *item)
{
    if (!styles || styles->isEmpty())
        return;
    if (const V8TextNode *layout = findLayoutNode(node))
        item->backColor = colorAt(*layout, 9, styles);
    if (nameIndex >= 0) {
        item->titleTextColor = colorAt(node, nameIndex + 10, styles);
        item->titleFont = fontAt(node, nameIndex + 11, styles);
    }
}

// Оформление декорации: цвет текста и шрифт — через 8 и 9 полей после имени.
// Сверено с Form.xml по декорациям «Новостей» (цвет «ЦветИнформацияОшибочна»,
// шрифт «ШрифтМикротекста») и «Перехода РВБ».
void readDecorationStyles(const V8TextNode &node, int nameIndex,
                          const FormStyleTable *styles, FormItem *item)
{
    if (!styles || styles->isEmpty() || nameIndex < 0)
        return;
    item->textColor = colorAt(node, nameIndex + 8, styles);
    item->font = fontAt(node, nameIndex + 9, styles);
}

// Оформление кнопки: шрифт лежит в записи оформления {7, …} среди полей
// кнопки. Сверено с Form.xml по кнопке «Оставить отзыв о классификации
// выручки по ОКВЭД»: стиль SmallTextFont (в форме записан кодом −30).
void readButtonStyles(const V8TextNode &node, const FormStyleTable *styles,
                      FormItem *item)
{
    if (!styles || styles->isEmpty())
        return;
    for (const V8TextNode &child : node.children) {
        if (!child.isList() || child.children.size() < 4)
            continue;
        const V8TextNode *tag = child.child(0);
        if (!tag || !tag->isScalar() || tag->scalar().trimmed() != QLatin1String("7"))
            continue;
        const FormFont font = fontFromRecord(child, styles);
        if (!font.isEmpty()) {
            item->font = font;
            return;
        }
    }
}

// Оформление таблицы: запись формата {4, …} открывает блок из трёх цветов
// (текст, фон, рамка), шрифта, цвета и шрифта заголовка — всего шесть полей
// после неё. Сверено с Form.xml: таблица «Задачи исполнителя» (рамка
// BorderColor), таблицы «Перехода РВБ» (цвет и шрифт заголовка).
void readTableStyles(const V8TextNode &node, const FormStyleTable *styles,
                     FormItem *item)
{
    if (!styles || styles->isEmpty())
        return;
    for (int i = 0; i + 1 < node.children.size(); ++i) {
        const V8TextNode *base = node.child(i);
        if (!base || !base->isList() || base->children.size() < 8)
            continue;
        const V8TextNode *tag = base->child(0);
        if (!tag || !tag->isScalar() || tag->scalar().trimmed() != QLatin1String("4"))
            continue;
        const V8TextNode *next = node.child(i + 1);
        if (!next || !next->isList() || next->children.isEmpty())
            continue;
        const V8TextNode *nextTag = next->child(0);
        if (!nextTag || !nextTag->isScalar()
            || nextTag->scalar().trimmed() != QLatin1String("3")) {
            continue;
        }
        item->textColor = colorAt(node, i + 1, styles);
        item->backColor = colorAt(node, i + 2, styles);
        item->borderColor = colorAt(node, i + 3, styles);
        item->font = fontAt(node, i + 4, styles);
        item->titleTextColor = colorAt(node, i + 5, styles);
        item->titleFont = fontAt(node, i + 6, styles);
        return;
    }
}

// Картинки поля: значок в заголовке колонки лежит через 23 поля после имени,
// картинка значений — в записи макета поля-картинки (код 10) пятым полем.
// Сверено с Form.xml по корпусу: 47 значков колонок и 15 картинок значений.
void readFieldPictures(const V8TextNode &node, int nameIndex,
                       const FormPictureTable *pictures, FormItem *item)
{
    if (!pictures)
        return;
    if (nameIndex >= 0) {
        if (const V8TextNode *header = node.child(nameIndex + 23)) {
            const ParsedPicture picture = pictureFromRecord(*header, pictures);
            if (!picture.reference.isEmpty()) {
                item->headerPictureRef = picture.reference;
            } else if (!picture.inlineData.isEmpty()) {
                item->inlinePictures.insert(QStringLiteral("HeaderPicture.bmp"),
                                            picture.inlineData);
                item->headerPictureRef = QStringLiteral("Abs:HeaderPicture.bmp");
            }
        }
    }
    QString code;
    if (const V8TextNode *layout = findFieldLayout(node, &code)) {
        if (code == QLatin1String("10")) {
            if (const V8TextNode *record = layout->child(5)) {
                const ParsedPicture picture = pictureFromRecord(*record, pictures);
                if (!picture.reference.isEmpty()) {
                    item->valuesPictureRef = picture.reference;
                } else if (!picture.inlineData.isEmpty()) {
                    item->inlinePictures.insert(QStringLiteral("ValuesPicture.bmp"),
                                                picture.inlineData);
                    item->valuesPictureRef = QStringLiteral("Abs:ValuesPicture.bmp");
                }
            }
        }
    }
}

// Свойства поля и колонки таблицы, которые платформа пишет в Form.xml только
// при отличии от значения по умолчанию, поэтому сам признак читается как
// «слот не равен коду значения по умолчанию». Смещения — от имени элемента;
// коды значений — как в бинарной форме. Сверено с Form.xml по 372 колонкам и
// 233 полям корпуса: ни одного расхождения.
void readFieldFlags(const V8TextNode &node, int nameIndex, FormItem *item)
{
    if (nameIndex < 0)
        return;
    auto codeAt = [&node, nameIndex](int offset) {
        const V8TextNode *child = node.child(nameIndex + offset);
        return child && child->isInteger() ? static_cast<int>(child->toInteger()) : -1;
    };

    item->readOnly = codeAt(8) == 1;
    // SkipOnInput: 2 — значение по умолчанию (в XML свойства нет), 0 и 1 —
    // явно заданные false и true.
    const int skip = codeAt(9);
    if (skip == 0 || skip == 1)
        item->skipOnInput = skip;
    item->defaultItem = codeAt(10) == 1;
    // ShowInHeader/ShowInFooter по умолчанию true: в XML пишется только false.
    const int headerFlag = codeAt(14);
    if (headerFlag >= 0)
        item->showInHeader = headerFlag != 0;
    const int footerFlag = codeAt(15);
    if (footerFlag >= 0)
        item->showInFooter = footerFlag != 0;
    item->cellHyperlink = codeAt(16) == 1;
    item->horizontalAlign = codeAt(17);
    item->headerHorizontalAlign = codeAt(18);
    item->footerHorizontalAlign = codeAt(19);
    item->editMode = codeAt(20);
    item->verticalAlign = codeAt(21);
    item->autoCellHeight = codeAt(22) == 1;
    item->fixingInTable = codeAt(43);
    item->toolTipRepresentation = codeAt(44);
}

// Подсказка ввода (InputHint) лежит в записи макета поля ввода (код 36)
// сорок пятым полем, локализованной записью. Сверено с Form.xml по 35 полям
// корпуса; у 34 подсказка совпала, одно поле в XML содержит заглушку
// «<отсутствует>».
void readFieldInputHint(const V8TextNode &node, FormItem *item)
{
    QString code;
    const V8TextNode *layout = findFieldLayout(node, &code);
    if (!layout || code != QLatin1String("36"))
        return;
    if (const V8TextNode *hint = layout->child(44))
        item->inputHint = localizedText(*hint);
}

// Свойства поля, которые лежат в записи макета: {36,…} — поле ввода,
// {10,…} — поле-картинка. Номера полей записи сверены с Form.xml по 326 полям
// ввода и 56 полям-картинкам корпуса; значение по умолчанию платформа в XML не
// пишет, поэтому «не равно умолчанию» и есть признак явно заданного свойства.
// Число из типизированной записи «N» («N», «-99») или пустая строка, если
// запись означает отсутствие значения («U» — пустая ссылка).
QString numberNodeText(const V8TextNode &node)
{
    if (!node.isList() || !node.child(0) || !node.child(0)->isScalar())
        return QString();
    if (node.child(0)->scalar() != QLatin1String("N"))
        return QString();
    const V8TextNode *value = node.child(1);
    return value && value->isScalar() ? value->scalar() : QString();
}

// Вид переключателя (RadioButtonType): у переключателя в записи есть блок
// {8, {3, вид, "", …}}, которого нет ни у одного другого вида поля; вид —
// второе поле внутренней записи (1 — Auto, 2 — Tumbler, 0 — RadioButton,
// значение по умолчанию, в корпусе не встречается). Сверено с Form.xml по
// 9 переключателям корпуса.
void readRadioButtonType(const V8TextNode &node, FormItem *item)
{
    if (item->fieldKind != FormFieldKind::RadioButton)
        return;
    for (const V8TextNode &child : node.children) {
        if (!child.isList() || child.children.size() < 2)
            continue;
        const V8TextNode *tag = child.child(0);
        const V8TextNode *payload = child.child(1);
        if (!tag || !tag->isScalar() || tag->scalar().trimmed() != QLatin1String("8"))
            continue;
        if (!payload || !payload->isList() || payload->children.size() < 4)
            continue;
        const V8TextNode *kind = payload->child(0);
        const V8TextNode *type = payload->child(1);
        if (!kind || !kind->isScalar() || kind->scalar().trimmed() != QLatin1String("3"))
            continue;
        if (type && type->isInteger())
            item->radioButtonType = static_cast<int>(type->toInteger());
        break;
    }
}

void readFieldLayoutFlags(const V8TextNode &node, FormItem *item)
{
    QString code;
    const V8TextNode *layout = findFieldLayout(node, &code);
    if (!layout) {
        // У поля табличного документа (13), графсхемы и HTML-документа (3),
        // диаграммы и форматированного документа (1) запись макета общим
        // поиском не находится: её код не входит в набор findFieldLayout.
        // Второе и третье поля такой записи — ширина и высота поля, поэтому
        // она опознаётся по ним. Сверено с Form.xml по формам «Реклама/
        // ФормаСписка», «Номенклатура/ФормаСписка», «Новости/ФормаДокумента»,
        // «МашиночитаемыеДоверенностиКонтрагентов/ФормаЭлемента» и
        // «ДоговорыКонтрагентов/ФормаПечатиЭлектронногоФормата».
        for (const V8TextNode &child : node.children) {
            if (!child.isList() || child.children.size() < 8)
                continue;
            const V8TextNode *first = child.child(0);
            if (!first || !first->isScalar())
                continue;
            const int start = fieldLimitsBlockStart(item->fieldKind,
                                                    first->scalar().trimmed());
            if (start < 0)
                continue;
            const V8TextNode *width = child.child(1);
            const V8TextNode *height = child.child(2);
            if (!width || !width->isInteger() || !height || !height->isInteger())
                continue;
            readFieldSizeLimits(child, start, item);
            break;
        }
        return;
    }
    auto intAt = [layout](int index) {
        const V8TextNode *child = layout->child(index);
        return child && child->isInteger() ? static_cast<int>(child->toInteger()) : -1;
    };
    if (code == QLatin1String("36") || code == QLatin1String("32")) {
        item->wrap = intAt(6) != 0;
        // Многострочный режим: восьмое поле записи, 0 — false, 1 — true,
        // 2 — значение не задано (платформа свойства не пишет).
        if (intAt(8) == 0 || intAt(8) == 1)
            item->multiLine = intAt(8);
        item->choiceButton = intAt(12);
        item->clearButton = intAt(13);
        item->spinButton = intAt(14);
        item->openButton = intAt(15);
        item->dropListButton = intAt(47);
        item->choiceListButton = intAt(11);
        item->chooseType = intAt(32) != 0;
        item->textEdit = intAt(41) != 0;
        item->listChoiceMode = intAt(19) == 1;
        item->autoMarkIncomplete = intAt(31);
        item->autoChoiceIncomplete = intAt(28);
        item->choiceHistoryOnInput = intAt(48);
        item->choiceButtonRepresentation = intAt(46);
        item->choiceFoldersAndItems = intAt(24);
        // Формат и предел значения: двадцать девятое и тридцатое поля —
        // локализованные строки Format и EditFormat, шестнадцатое и
        // семнадцатое — пределы MinValue и MaxValue (число или пустая ссылка
        // <U>), шестьдесят шестое — ExtendedEditMultipleValues. Сверено с
        // Form.xml по 7 форматам, 6 форматам редактирования, 5 пределам и
        // 34 полям с расширенным редактированием корпуса.
        if (const V8TextNode *format = layout->child(29))
            item->format = localizedText(*format);
        if (const V8TextNode *editFormat = layout->child(30))
            item->editFormat = localizedText(*editFormat);
        if (const V8TextNode *minValue = layout->child(16))
            item->minValue = numberNodeText(*minValue);
        if (const V8TextNode *maxValue = layout->child(17))
            item->maxValue = numberNodeText(*maxValue);
        item->extendedEditMultipleValues = intAt(65) == 1;
    } else if (code == QLatin1String("10")) {
        // Поле-картинка: режим перетаскивания файлов и признак гиперссылки.
        item->fileDragMode = intAt(22);
        item->hyperlink = intAt(8) == 1;
    } else if (code == QLatin1String("11")) {
        // Надпись и флажок: ширина, высота и растяжение стоят в начале записи,
        // а пределы размера — пятнадцатым полем записи.
        // Сверено с Form.xml: AutoMaxWidth — 15 надписей, AutoMaxHeight — 3,
        // MaxHeight — 2.
        // Вид флажка: двенадцатое поле равно 3 у переключателя, 0 — у обычного
        // флажка (платформа пишет в Form.xml CheckBoxType=Auto).
        const int checkBox = intAt(12);
        if (checkBox == 3)
            item->checkBoxType = 3;
        // Гиперссылка в надписи: седьмое поле записи равно 1. Сверено с
        // Form.xml по 4 надписям с Hiperlink и 600 без него.
        if (intAt(7) == 1)
            item->hyperlink = true;
    }
    readFieldSizeLimits(*layout, fieldLimitsBlockStart(item->fieldKind, code), item);
}

// Настройки динамического списка записаны в записи таблицы блоком
// «количество, код, значение, …»: количество — целое число, коды возрастают,
// значения — типизированные записи ["B","1"], ["N","60"], ["S",""], ["U"] или
// ["#", GUID, …]. Сверено с Form.xml по 87 таблицам динамических списков
// корпуса: блок находится сразу за общими полями таблицы.
const V8TextNode *findTableSettingsBlock(const V8TextNode &node, int *start)
{
    for (int i = 0; i < node.children.size(); ++i) {
        const V8TextNode *count = node.child(i);
        if (!count || !count->isInteger())
            continue;
        const int n = static_cast<int>(count->toInteger());
        // Блок стоит после общих полей записи таблицы; встречаются и блоки
        // из одной пары (только RowFilter), поэтому пара из одного кода
        // допускается, но только с кодом из известного набора.
        if (n < 1 || i < 30 || i + 2 * n >= node.children.size())
            continue;
        if (n == 1) {
            const V8TextNode *key = node.child(i + 1);
            if (!key || !key->isInteger())
                continue;
            const int keyCode = static_cast<int>(key->toInteger());
            if (keyCode != 13 && keyCode != 19)
                continue;
        }
        int previous = -1;
        bool ok = true;
        for (int j = 0; j < n && ok; ++j) {
            const V8TextNode *key = node.child(i + 1 + 2 * j);
            const V8TextNode *value = node.child(i + 2 + 2 * j);
            if (!key || !key->isInteger() || !value || !value->isList()
                || !value->child(0) || !value->child(0)->isScalar()) {
                ok = false;
                break;
            }
            const int keyCode = static_cast<int>(key->toInteger());
            if (keyCode <= previous)
                ok = false;
            previous = keyCode;
        }
        if (ok) {
            if (start)
                *start = i;
            return &node;
        }
    }
    return nullptr;
}

// Дата периода в записи динамического списка — 14 цифр «ГГГГММДДЧЧММСС»;
// платформа пишет её в XML как «ГГГГ-ММ-ДДTЧЧ:ММ:СС».
QString periodDateText(const QString &digits)
{
    if (digits.size() != 14)
        return digits;
    return QStringLiteral("%1-%2-%3T%4:%5:%6")
        .arg(digits.mid(0, 4), digits.mid(4, 2), digits.mid(6, 2),
             digits.mid(8, 2), digits.mid(10, 2), digits.mid(12, 2));
}

void readDynamicListSettings(const V8TextNode &node, FormItem *item)
{
    int start = -1;
    if (!findTableSettingsBlock(node, &start))
        return;
    const int count = static_cast<int>(node.child(start)->toInteger());
    for (int j = 0; j < count; ++j) {
        const int key = static_cast<int>(node.child(start + 1 + 2 * j)->toInteger());
        const V8TextNode *value = node.child(start + 2 + 2 * j);
        const QString kind = value->child(0)->scalar().trimmed();
        auto ordinal = [value]() {
            const V8TextNode *third = value->child(2);
            return third && third->isInteger() ? static_cast<int>(third->toInteger()) : -1;
        };
        auto boolean = [value]() {
            const V8TextNode *second = value->child(1);
            return second && second->isScalar() && second->scalar().trimmed() != QLatin1String("0");
        };
        switch (key) {
        case 5:
            item->autoRefresh = boolean() ? 1 : 0;
            break;
        case 6: {
            const V8TextNode *second = value->child(1);
            if (second && second->isInteger())
                item->autoRefreshPeriod = static_cast<int>(second->toInteger());
            break;
        }
        case 7: {
            // Period: ["#", GUID, ["0", начало, конец]]; вид 0 — Custom.
            const V8TextNode *payload = value->child(2);
            if (payload && payload->isList() && payload->children.size() >= 3) {
                const V8TextNode *kindNode = payload->child(0);
                const V8TextNode *begin = payload->child(1);
                const V8TextNode *end = payload->child(2);
                if (kindNode && kindNode->isScalar() && kindNode->scalar() == QLatin1String("0")
                    && begin && begin->isScalar() && end && end->isScalar()) {
                    item->period = QStringLiteral("Custom, %1, %2")
                        .arg(periodDateText(begin->scalar()),
                             periodDateText(end->scalar()));
                }
            }
            break;
        }
        case 8:
            item->tableChoiceFolders = ordinal();
            break;
        case 9:
            item->restoreCurrentRow = boolean() ? 1 : 0;
            break;
        case 10:
            item->hasTopLevelParent = true;
            item->topLevelParent = kind == QLatin1String("S") && value->child(1)
                ? value->child(1)->scalar() : QString();
            break;
        case 11:
            item->showRoot = boolean() ? 1 : 0;
            break;
        case 12:
            item->allowRootChoice = boolean() ? 1 : 0;
            break;
        case 13:
            item->hasRowFilter = true;
            break;
        case 14:
            item->updateOnDataChange = ordinal();
            break;
        case 20:
            // AllowGettingCurrentRowURL: платформа пишет в XML только true
            // (99 форм корпуса — 17 таблиц динамических списков, и у всех
            // кладётся код 20 со значением ["B","1"]).
            item->allowGettingCurrentRowUrl = boolean() ? 1 : 0;
            break;
        default:
            break;
        }
    }
}

// Свойства таблицы: положение командной панели, чередование цветов строк,
// добавление строки, перетаскивание. В XML платформа пишет их только при
// отличии от значения по умолчанию. Сверено с Form.xml по 48 таблицам корпуса.
void readTableFlags(const V8TextNode &node, int nameIndex, FormItem *item,
                    const FormPictureTable *pictures)
{
    if (nameIndex < 0)
        return;
    auto codeAt = [&node, nameIndex](int offset) {
        const V8TextNode *child = node.child(nameIndex + offset);
        return child && child->isInteger() ? static_cast<int>(child->toInteger()) : -1;
    };

    // Отображение таблицы: третье поле записи, сразу после ссылки на имя
    // (0 — List, 1 — Table по умолчанию, 2 — Tree). Положение не зависит от
    // того, где в записи стоит имя: у 13 таблиц корпуса имя сдвинуто на поле
    // вперёд, а код отображения остаётся третьим. Сверено с Form.xml по
    // 54 таблицам: 25 List, 15 умолчаний и 4 Tree совпали.
    if (const V8TextNode *representation = node.child(3)) {
        if (representation->isInteger())
            item->tableRepresentation = static_cast<int>(representation->toInteger());
    }
    // TitleLocation таблицы: 0 — None (по умолчанию), 1 — Auto, 3 — Top.
    const int title = codeAt(1);
    if (title > 0 && title <= 5)
        item->tableTitleLocation = title;
    // CommandBarLocation: 1 — Auto (по умолчанию), 0 — None.
    item->commandBarLocation = codeAt(3);
    // Header: 0 — false, по умолчанию true.
    item->tableHeader = codeAt(21) != 0;
    // Оформление таблицы: линии сетки (0 — false, 1 — умолчание) стоят
    // двадцать седьмым и двадцать восьмым полем, начальное раскрытие дерева
    // (InitialTreeView: 0 — Default, 1 — ExpandTopLevel, 2 — ExpandAllLevels)
    // — тридцать четвёртым. Сверено с Form.xml по 54 таблицам корпуса, а
    // раздельность линий проверена на формах, где выключена только одна сетка:
    // «ВидыКонтактнойИнформации/ФормаСписка» (+27 = 0 при +28 = 1) и
    // «ПрограммыЭлектроннойПодписиИШифрования/ОшибкаОбращенияКПрограмме»
    // (+27 = 1 при +28 = 0).
    item->horizontalLines = codeAt(27);
    item->verticalLines = codeAt(28);
    item->initialTreeView = codeAt(34);
    // Путь картинки строки (RowPictureDataPath): тридцать девятое поле записи
    // таблицы — [0] (не задано) или ["1", [номер поля]]. У «Банков» и
    // «ЗадачиИсполнителя» там 10000000 (стандартное поле DefaultPicture
    // динамического списка), у «Файлов» — 2, у «СозданныхДокументов» — 4, то
    // есть номер колонки таблицы значений. Сверено с Form.xml по 54 таблицам
    // корпуса: у 12 таблиц с путём поле задано, у 42 — [0].
    if (const V8TextNode *record = node.child(nameIndex + 38)) {
        const V8TextNode *flag = record->child(0);
        if (record->isList() && flag && flag->isScalar()
            && flag->scalar().trimmed() == QLatin1String("1")) {
            const V8TextNode *ids = record->child(1);
            if (ids && ids->isList() && ids->child(0) && ids->child(0)->isInteger())
                item->rowPictureFieldId = static_cast<int>(ids->child(0)->toInteger());
        }
    }
    // «Картинки строк» (RowsPicture) — сороковое поле записи таблицы: та же
    // запись картинки {4, вид, {0, GUID}, …}, что у декорации-картинки. Сверено
    // с Form.xml по трём таблицам корпуса с этим свойством и 51 без него.
    if (pictures) {
        if (const V8TextNode *record = node.child(nameIndex + 39)) {
            const ParsedPicture picture = pictureFromRecord(*record, pictures);
            if (!picture.reference.isEmpty())
                item->rowsPicture = picture.reference;
        }
    }
    item->useAlternationRowColor = codeAt(31) == 1;
    item->autoInsertNewRow = codeAt(32) == 1;
    item->enableStartDrag = codeAt(47) == 1;
    item->enableDrag = codeAt(48) == 1;
    // Пределы размера таблицы — блок из пяти полей в конце записи:
    // AutoMaxWidth, MaxWidth, (поле не используется), AutoMaxHeight, MaxHeight.
    // Блок стоит на фиксированном расстоянии от конца записи, потому что число
    // вложенных записей (контекстное меню, командная панель, подсказка,
    // дополнения панели) у таблиц разное. Сверено с Form.xml по 54 таблицам
    // корпуса: AutoMaxWidth=false у 4, AutoMaxHeight=false у 2 — расхождений нет.
    const int tableEnd = static_cast<int>(node.children.size());
    const auto tailCode = [&node, tableEnd](int fromEnd) {
        const int index = tableEnd - fromEnd;
        if (index < 0)
            return -1;
        const V8TextNode *slot = node.child(index);
        return slot && slot->isInteger() ? static_cast<int>(slot->toInteger()) : -1;
    };
    const int autoMaxWidth = tailCode(15);
    if (autoMaxWidth >= 0)
        item->autoMaxWidth = autoMaxWidth != 0;
    const int tableMaxWidth = tailCode(14);
    if (tableMaxWidth > 0)
        item->maxWidth = tableMaxWidth;
    const int autoMaxHeight = tailCode(12);
    if (autoMaxHeight >= 0)
        item->autoMaxHeight = autoMaxHeight != 0;
    const int tableMaxHeight = tailCode(11);
    if (tableMaxHeight > 0)
        item->maxHeight = tableMaxHeight;
    // Место дополнений поиска таблицы: три поля перед вложенными записями —
    // 25-е с конца SearchStringLocation, 24-е ViewStatusLocation, 23-е
    // SearchControlLocation. Платформа пишет в Form.xml строку None, когда
    // элемент не показывается; значение по умолчанию (0) не пишется.
    // Сверено с Form.xml по 54 таблицам корпуса: у 31 таблицы задан
    // SearchStringLocation=None, у 33 — ViewStatusLocation и
    // SearchControlLocation; расхождений нет.
    item->searchStringLocation = tailCode(25) == 1 ? 1 : 0;
    item->viewStatusLocation = tailCode(24) == 1 ? 1 : 0;
    item->searchControlLocation = tailCode(23) == 1 ? 1 : 0;
    // Признаки, которые платформа пишет в Form.xml только при отличии от
    // умолчания: DefaultItem (0/1), ChangeRowSet и ChangeRowOrder (0 — false,
    // 1 — умолчание) и SkipOnInput (0 — false, 2 — умолчание, флажок стоит
    // тридцатым с конца записи). Сверено с Form.xml по 54 таблицам корпуса.
    const V8TextNode *defaultItem = node.child(nameIndex + 11);
    if (defaultItem && defaultItem->isInteger())
        item->defaultItem = defaultItem->toInteger() == 1;
    const V8TextNode *changeRowSet = node.child(nameIndex + 12);
    if (changeRowSet && changeRowSet->isInteger())
        item->changeRowSet = static_cast<int>(changeRowSet->toInteger());
    const V8TextNode *changeRowOrder = node.child(nameIndex + 13);
    if (changeRowOrder && changeRowOrder->isInteger())
        item->changeRowOrder = static_cast<int>(changeRowOrder->toInteger());
    const V8TextNode *skipOnInput = node.child(static_cast<int>(node.children.size()) - 30);
    if (skipOnInput && skipOnInput->isInteger()) {
        const int skip = static_cast<int>(skipOnInput->toInteger());
        if (skip == 0 || skip == 1)
            item->skipOnInput = skip;
    }
    // Исключённые из панели команды: список [количество, GUID, …], где все
    // элементы после первого — GUID стандартных команд элемента. Платформа
    // пишет свойство только при непустом списке и сортирует имена по алфавиту.
    // Сверено с Form.xml по 16 таблицам корпуса с заданным списком.
    for (const V8TextNode &child : node.children) {
        if (!child.isList() || child.children.size() < 3)
            continue;
        const V8TextNode *count = child.child(0);
        if (!count || !count->isInteger())
            continue;
        const int size = static_cast<int>(count->toInteger());
        if (size < 2 || static_cast<int>(child.children.size()) != size + 1)
            continue;
        QStringList names;
        bool ok = true;
        for (int i = 1; i <= size && ok; ++i) {
            const V8TextNode *guid = child.child(i);
            if (!guid || !guid->isGuid()) {
                ok = false;
                break;
            }
            const QString name = itemStandardCommandName(guid->normalizedGuid());
            if (name.isEmpty())
                ok = false;
            else
                names.append(name);
        }
        if (!ok || names.isEmpty())
            continue;
        names.sort(Qt::CaseInsensitive);
        item->excludedCommands = names;
        break;
    }
    // Перетаскивание файлов в таблицу: 0 — AsFile, 1 — UseAsFile (по
    // умолчанию). Признак стоит вторым с конца записи, потому что после него
    // идут только необязательные вложенные узлы. Сверено с Form.xml по
    // 54 таблицам корпуса: 42 таблицы с AsFile совпали.
    const int count = static_cast<int>(node.children.size());
    if (count >= 3) {
        const V8TextNode *drag = node.child(count - 2);
        if (drag && drag->isInteger() && drag->toInteger() == 1)
            item->fileDragMode = 1;
        else if (drag && drag->isInteger())
            item->fileDragMode = 0;
    }
    readDynamicListSettings(node, item);
}

// Картинка кнопки лежит через 20 полей после имени. Сверено с Form.xml по
// кнопкам «Сформировать» и «Показать быстрые настройки» (10 кнопок корпуса).
void readButtonPictures(const V8TextNode &node, int nameIndex,
                        const FormPictureTable *pictures, FormItem *item)
{
    if (!pictures || nameIndex < 0)
        return;
    const V8TextNode *record = node.child(nameIndex + 20);
    if (!record)
        return;
    const ParsedPicture picture = pictureFromRecord(*record, pictures);
    if (!picture.reference.isEmpty()) {
        item->pictureRef = picture.reference;
    } else if (!picture.inlineData.isEmpty()) {
        item->inlinePictures.insert(QStringLiteral("Picture.bmp"), picture.inlineData);
        item->pictureRef = QStringLiteral("Abs:Picture.bmp");
    }
}

// Подменю (вид группы 1): картинка и отображение лежат в записи оформления
// {7, {4, …}, …}: первый элемент записи — код 7, второй — картинка в той же
// записи {4, вид, ссылка, …}, что у декораций и кнопок, пятый —
// Representation (0 — Text, 1 — Picture, 2 — PictureAndText, 3 — Auto,
// значение по умолчанию, платформа свойство не пишет). Запись ищется по
// составу, а не по смещению: у подменю её место зависит от вида группы.
// Сверено с Form.xml на форме «АвансовыйОтчет/ФормаДокумента»: «ПодменюПечать»
// → StdPicture.Print + PictureAndText, «ПодменюЗаполнить» →
// CommonPicture.ЗаполнитьФорму + PictureAndText, «КомандыЭДО» — без картинки
// и без свойства.
void readPopupAppearance(const V8TextNode &node, const FormPictureTable *pictures,
                         FormItem *item)
{
    for (const V8TextNode &child : node.children) {
        if (!child.isList() || child.children.size() < 5)
            continue;
        const V8TextNode *tag = child.child(0);
        if (!tag || !tag->isScalar() || tag->scalar().trimmed() != QLatin1String("7"))
            continue;
        const V8TextNode *picture = child.child(1);
        if (!picture || !picture->isList() || picture->children.size() < 3)
            continue;
        const V8TextNode *pictureTag = picture->child(0);
        if (!pictureTag || !pictureTag->isScalar()
            || pictureTag->scalar().trimmed() != QLatin1String("4"))
            continue;

        const ParsedPicture parsed = pictureFromRecord(*picture, pictures);
        if (!parsed.reference.isEmpty()) {
            item->pictureRef = parsed.reference;
        } else if (!parsed.inlineData.isEmpty()) {
            item->inlinePictures.insert(QStringLiteral("Picture.bmp"), parsed.inlineData);
            item->pictureRef = QStringLiteral("Abs:Picture.bmp");
        }

        const V8TextNode *representation = child.child(4);
        if (representation && representation->isInteger()) {
            const int value = static_cast<int>(representation->toInteger());
            if (value >= 0 && value <= 3)
                item->buttonRepresentation = value;
        }
        return;
    }
}

// Смещение признака видимости от расширенной подсказки в «хвосте» записи
// элемента. Перед подсказкой идут служебные поля, и их число зависит от вида
// элемента: у группы — три, у декорации — пять, у кнопки — шесть. Значения
// сверены с Form.xml на формах БП (265 групп, 326 декораций, 44 кнопки).
int visibleOffsetFromTooltip(int typeCode)
{
    switch (typeCode) {
    case 22:
        return 3;
    case 12:
        return 5;
    case 31:
        return 6;
    default:
        return -1;
    }
}

// Представление подсказки (ToolTipRepresentation) лежит в «хвосте» записи
// вторым полем перед расширенной подсказкой — одинаково у групп, декораций,
// кнопок и таблиц. Коды: 0 — умолчание (в Form.xml не пишется), 1 — None,
// 3 — Button, 5 — ShowTop, 7 — ShowBottom, 8 — ShowRight. Сверено с Form.xml
// по 34 элементам корпуса, у которых свойство задано (все значения совпали),
// и по 1600 элементам без свойства (там всегда 0). У полей то же значение
// стоит на фиксированном месте после имени (см. readFieldFlags).
int tooltipRepresentationCode(const V8TextNode &node)
{
    int tooltipIndex = -1;
    for (int i = 0; i < node.children.size(); ++i) {
        const V8TextNode *child = node.child(i);
        if (!child || !isItemNode(*child))
            continue;
        const QString childName = findName(*child, 2);
        if (childName.endsWith(QStringLiteral("РасширеннаяПодсказка"))
            || childName.endsWith(QStringLiteral("ExtendedTooltip"))) {
            tooltipIndex = i;
        }
    }
    if (tooltipIndex < 2)
        return -1;
    const V8TextNode *code = node.child(tooltipIndex - 2);
    if (!code || !code->isInteger())
        return -1;
    return static_cast<int>(code->toInteger());
}

// Значения стилей-цветов, на которые ссылается форма: рендерер получает их
// таблицей styleItems (имя стиля -> «#RRGGBB»), иначе по одной ссылке
// «style:Имя» цвет не восстановить.
void collectStyleItems(const QVector<FormItem> &items,
                       const FormStyleTable *styles,
                       QHash<QString, QString> *target)
{
    if (!styles)
        return;
    for (const FormItem &item : items) {
        const QString *values[] = {&item.textColor, &item.titleTextColor,
                                   &item.backColor, &item.borderColor,
                                   &item.font.reference, &item.titleFont.reference};
        for (const QString *value : values) {
            if (!value->startsWith(QStringLiteral("style:")))
                continue;
            const QString name = value->mid(6);
            const QString color = styles->colorValue(name);
            if (!color.isEmpty() && !target->contains(name))
                target->insert(name, color);
        }
        collectStyleItems(item.children, styles, target);
    }
}

// Общие картинки, на которые ссылается форма: рендерер получает их данными
// (base64 ZIP-архива), а по одной ссылке «CommonPicture.Имя» картинку не
// нарисовать — её байты лежат в контейнере отдельно.
void collectCommonPictures(const QVector<FormItem> &items,
                           const FormPictureTable *pictures,
                           FormDocument *document)
{
    if (!pictures)
        return;
    for (const FormItem &item : items) {
        const QString *references[] = {&item.valuesPictureRef, &item.headerPictureRef,
                                       &item.pictureRef};
        for (const QString *reference : references) {
            if (!reference->startsWith(QStringLiteral("CommonPicture.")))
                continue;
            const QString name = reference->mid(14);
            if (document->commonPictures.contains(name))
                continue;
            const QString data = pictures->dataByName(name);
            if (!data.isEmpty()) {
                document->commonPictures.insert(name, data);
                document->commonPictureMimes.insert(
                    name, QStringLiteral("application/zip"));
            }
        }
        for (auto it = item.inlinePictures.constBegin();
             it != item.inlinePictures.constEnd(); ++it) {
            const QString key = QStringLiteral("@item:") + item.name
                + QLatin1Char('/') + it.key();
            if (document->commonPictures.contains(key))
                continue;
            document->commonPictures.insert(key, it.value());
            document->commonPictureMimes.insert(key, QStringLiteral("image/bmp"));
        }
        collectCommonPictures(item.children, pictures, document);
    }
}

// Ссылки кнопок на команды разрешаются после разбора всего документа: имена
// команд формы лежат в другом разделе, а владелец стандартной команды
// элемента (таблица, поле форматированного документа) — это элемент с
// идентификатором из ссылки.
struct ButtonCommandContext
{
    QHash<int, QString> commandNames;    // идентификатор команды -> имя
    QSet<QString> collections;           // GUID коллекций команд формы
    QHash<int, QString> itemNames;       // идентификатор элемента -> имя
};

void collectItemNames(const QVector<FormItem> &items, QHash<int, QString> *target)
{
    for (const FormItem &item : items) {
        if (item.id > 0 && !item.name.isEmpty())
            target->insert(item.id, item.name);
        collectItemNames(item.children, target);
    }
}

void resolveButtonCommands(QVector<FormItem> *items,
                           const ButtonCommandContext &context,
                           const FormBindingResolver *resolver)
{
    for (FormItem &item : *items) {
        if (item.type == FormItem::Type::Button && item.commandRef >= 0
            && !item.commandGuid.isEmpty()) {
            if (item.commandRef == 0) {
                const QString standard = formStandardCommandName(item.commandGuid);
                if (!standard.isEmpty()) {
                    item.commandName = QStringLiteral("Form.StandardCommand.")
                        + standard;
                } else if (resolver) {
                    // Команда владельца формы (выполнение задачи и т. п.):
                    // GUID её описания есть в метаданных объекта.
                    const QString fqn = resolver->commandFqn(item.commandGuid);
                    if (!fqn.isEmpty())
                        item.commandName = fqn;
                }
            } else if (context.collections.contains(item.commandGuid.toLower())) {
                const QString name = context.commandNames.value(item.commandRef);
                if (!name.isEmpty())
                    item.commandName = QStringLiteral("Form.Command.") + name;
            } else {
                const QString suffix = itemStandardCommandName(item.commandGuid);
                const QString owner = context.itemNames.value(item.commandRef);
                if (!suffix.isEmpty() && !owner.isEmpty()) {
                    item.commandName = QStringLiteral("Form.Item.%1.StandardCommand.%2")
                                           .arg(owner, suffix);
                }
            }
        }
        resolveButtonCommands(&item.children, context, resolver);
    }
}

// Представление страниц (PagesRepresentation) группа-страницы держит в записи
// через 14 полей после имени: второй элемент записи — код представления
// (0 — None, 1 — TabsOnTop, 2 — TabsOnBottom). Сверено с Form.xml по всем
// 21 группе-страницам корпуса: расхождений нет.
void readPagesRepresentation(const V8TextNode &node, int nameIndex, FormItem *item)
{
    if (item->groupKind != 3 || nameIndex < 0)
        return;
    const V8TextNode *record = node.child(nameIndex + 14);
    if (!record || !record->isList() || record->children.size() < 2)
        return;
    const V8TextNode *code = record->child(1);
    if (code && code->isInteger()) {
        const int value = static_cast<int>(code->toInteger());
        if (value >= 0 && value <= 2)
            item->pagesRepresentation = value;
    }
}

FormItem parseItem(const V8TextNode &node, const FormStyleTable *styles,
                  const FormPictureTable *pictures)
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
    // Код вида группы и дополнения панели (строка поиска, состояние просмотра,
    // управление поиском) записан перед именем.
    if ((item.typeCode == 22 || item.typeCode == 5) && nameIndex > 2)
        item.groupKind = integerChild(node, nameIndex - 1);
    readPagesRepresentation(node, nameIndex, &item);
    item.synonym = localizedText(node);

    // Поля: сразу после имени — TitleLocation (0 — None, 1 — Auto,
    // 2 — Left, 3 — Top, 4 — Right, 5 — Bottom), через 37 полей — Visible.
    // Сверено с Form.xml по id (значение Top проверено на форме справочника
    // «Варианты отчетов»).
    if (isFieldCode(item.typeCode) && nameIndex >= 0) {
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
        // Заголовок и подсказка поля стоят на фиксированных местах после
        // TitleLocation: +3 — Title, +4 — ToolTip. Искать первый
        // локализованный текст по всему узлу нельзя: если заголовка в форме
        // нет, он подставит подсказку (так «Наценка» получала своё длинное
        // описание вместо названия).
        item.synonym.clear();
        if (const V8TextNode *titleNode = node.child(nameIndex + 3))
            item.synonym = localizedText(*titleNode);
        if (const V8TextNode *tooltipNode = node.child(nameIndex + 4))
            item.tooltip = localizedText(*tooltipNode);
    } else if (nameIndex >= 0) {
        // У групп, страниц, декораций, таблиц, кнопок и дополнений панели
        // заголовок и подсказка — две соседние записи сразу после имени.
        // Пустые записи тоже присутствуют, поэтому берём первые две по форме,
        // не заходя во вложенные элементы (контекстное меню, расширенная
        // подсказка) — иначе их тексты попадут в заголовок.
        bool haveTitle = false;
        for (int i = nameIndex + 1; i < node.children.size() && i <= nameIndex + 8; ++i) {
            const V8TextNode *child = node.child(i);
            if (!child || isItemNode(*child))
                break;
            QString text;
            if (!localizedRecord(*child, &text))
                continue;
            if (!haveTitle) {
                item.synonym = text;
                haveTitle = true;
            } else {
                item.tooltip = text;
                break;
            }
        }
    }

    // Вид поля (InputField, LabelField, CheckBoxField, PictureField, ...) лежит
    // целым числом непосредственно перед именем элемента. Сверено с Form.xml
    // по 583 полям 26 форм: коды 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 14, 15, 17,
    // 19, 20 совпали во всех полях без исключений.
    if (isFieldCode(item.typeCode) && nameIndex >= 2) {
        const V8TextNode *kind = node.child(nameIndex - 1);
        if (kind && kind->isInteger()) {
            item.fieldKindCode = static_cast<int>(kind->toInteger());
            item.fieldKind = formFieldKindFromCode(item.fieldKindCode);
        }
        // Подписи списка выбора (у переключателей — по одной на элемент).
        readChoiceBlock(node, &item.choiceItems);
    }

    // Видимость групп, декораций и кнопок записана не сразу после имени, а в
    // «хвосте» записи — перед расширенной подсказкой (она всегда последний
    // дочерний элемент). Смещение зависит от вида элемента.
    const int visibleOffset = visibleOffsetFromTooltip(item.typeCode);
    if (visibleOffset > 0) {
        int tooltipIndex = -1;
        for (int i = 0; i < node.children.size(); ++i) {
            const V8TextNode *child = node.child(i);
            if (!child || !isItemNode(*child))
                continue;
            const QString childName = findName(*child, 2);
            if (childName.endsWith(QStringLiteral("РасширеннаяПодсказка"))
                || childName.endsWith(QStringLiteral("ExtendedTooltip"))) {
                tooltipIndex = i;
            }
        }
        if (tooltipIndex >= visibleOffset) {
            const V8TextNode *visible = node.child(tooltipIndex - visibleOffset);
            if (visible && visible->isInteger())
                item.visible = visible->toInteger() != 0;
            else if (visible && visible->isScalar())
                item.visible = visible->scalar().trimmed() != QLatin1String("0");
        }
    }

    // Представление подсказки у групп, декораций, кнопок и таблиц: лежит
    // вторым полем перед расширенной подсказкой (у полей — фиксированное
    // место после имени, см. readFieldFlags).
    if (!isFieldCode(item.typeCode) && nameIndex >= 0) {
        const int toolTipCode = tooltipRepresentationCode(node);
        if (toolTipCode > 0)
            item.toolTipRepresentation = toolTipCode;
    }

    // Декорации: вид (надпись или картинка), признак гиперссылки и картинка.
    if (item.typeCode == 12 && nameIndex >= 1)
        readDecorationExtras(node, nameIndex, pictures, &item);
    // Ориентация группы — второй элемент узла макета (1 — горизонтальная).
    // По умолчанию платформа (и рендерер) считают обычную группу
    // горизонтальной, поэтому группа без узла макета тоже горизонтальная.
    if (item.typeCode == 22) {
        if (const V8TextNode *layout = findLayoutNode(node))
            item.orientation = layoutInteger(*layout, 1);
        // Подменю: картинка и отображение лежат не в узле макета, а в записи
        // оформления {7, …} — см. readPopupAppearance.
        if (item.groupKind == 1)
            readPopupAppearance(node, pictures, &item);
        // Отступы внутри группы: пятнадцатое поле записи макета —
        // HorizontalSpacing, шестнадцатое — VerticalSpacing (1 None, 2 Half,
        // 3 Single; 0 — Auto, значение по умолчанию). Сверено с Form.xml по
        // 8 группам с VerticalSpacing и 3 с HorizontalSpacing.
        if (const V8TextNode *layout = findLayoutNode(node)) {
            const int horizontal = layoutInteger(*layout, 15);
            if (horizontal > 0 && horizontal <= 5)
                item.horizontalSpacing = horizontal;
            const int vertical = layoutInteger(*layout, 16);
            if (vertical > 0 && vertical <= 5)
                item.verticalSpacing = vertical;
        }
        // Важность отображения (DisplayImportance) у группы записана
        // последним полем записи: 0 — Auto (умолчание), 3 — Usual. Сверено с
        // Form.xml по 1605 группам корпуса: 4 с Usual и 1601 без свойства.
        if (const V8TextNode *importance =
                node.child(static_cast<int>(node.children.size()) - 1)) {
            if (importance->isInteger())
                item.displayImportance = static_cast<int>(importance->toInteger());
        }
    }

    switch (item.typeCode) {
    case 22:
        item.type = FormItem::Type::Group;
        break;
    case 37:
    case 35:
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
    readUserVisible(node, nameIndex, &item);

    // Ориентация страницы: у тега Page нет узла макета {29,…} обычной группы,
    // вместо него в записи лежит блок {18,…}, семнадцатое поле которого и
    // задаёт Group: 0 — вертикальная (по умолчанию), 1 — Horizontal,
    // 2 — HorizontalIfPossible, 3 — AlwaysHorizontal. Сверено с Form.xml по
    // 76 страницам корпуса: расхождений нет.
    if (item.typeCode == 22 && item.groupKind == 4) {
        for (const V8TextNode &child : node.children) {
            if (!child.isList() || child.children.isEmpty())
                continue;
            const V8TextNode *tag = child.child(0);
            if (!tag || !tag->isScalar() || tag->scalar() != QLatin1String("18"))
                continue;
            if (const V8TextNode *mode = child.child(17)) {
                if (mode->isInteger())
                    item.pageMode = static_cast<int>(mode->toInteger());
            }
            // Показ заголовка страницы (ShowTitle) — седьмое поле блока: 1 —
            // значение по умолчанию (Form.xml свойства не пишет), 0 — заголовок
            // скрыт. У обычной группы тот же признак лежит в узле макета, а у
            // страницы своего узла макета нет. Сверено с Form.xml по всем 32
            // страницам корпуса, из них у двух заголовок скрыт.
            if (const V8TextNode *showTitle = child.child(6)) {
                if (showTitle->isInteger())
                    item.showTitle = showTitle->toInteger() != 0;
            }
            // Путь к данным заголовка страницы (TitleDataPath) — четвёртое поле
            // того же блока; пустой путь записан как {0}. Сверено с Form.xml по
            // десяти страницам корпуса с этим свойством и по 88 без него.
            if (const V8TextNode *path = child.child(4)) {
                if (isPathDescriptor(*path))
                    item.titleDataPath = parsePathDescriptor(*path);
            }
            // Выравнивание содержимого страницы (HorizontalAlign и
            // VerticalAlign) — двенадцатое и тринадцатое поля блока: 0 —
            // Left/Top, 1 — Center, 2 — Right/Bottom; 3 — Auto, значение по
            // умолчанию, платформа свойства не пишет. Сверено с Form.xml по
            // 71 странице корпуса: расхождений нет.
            if (const V8TextNode *horizontal = child.child(12)) {
                if (horizontal->isInteger()) {
                    const int value = static_cast<int>(horizontal->toInteger());
                    if (value >= 0 && value <= 2)
                        item.contentHorizontalAlign = value;
                }
            }
            if (const V8TextNode *vertical = child.child(13)) {
                if (vertical->isInteger()) {
                    const int value = static_cast<int>(vertical->toInteger());
                    if (value >= 0 && value <= 2)
                        item.contentVerticalAlign = value;
                }
            }
            break;
        }
    }

    // Прочие свойства полей и таблиц, которые платформа пишет в Form.xml
    // только при отличии от значения по умолчанию.
    if (isFieldCode(item.typeCode) && nameIndex >= 0) {
        readFieldFlags(node, nameIndex, &item);
        readFieldInputHint(node, &item);
        readFieldLayoutFlags(node, &item);
        readRadioButtonType(node, &item);
    } else if (item.typeCode == 55 && nameIndex >= 0) {
        readTableFlags(node, nameIndex, &item, pictures);
    }

    // Известный вид поля решает, есть ли у него редактор: у надписи и поля
    // картинки его нет. Запись макета (36/11) остаётся запасным вариантом для
    // сборок, где код вида не читается.
    if (item.fieldKind == FormFieldKind::Label || item.fieldKind == FormFieldKind::Picture)
        item.labelOnly = true;
    else if (item.fieldKind != FormFieldKind::Unknown)
        item.labelOnly = false;

    // Группа: в узле макета {29,...} лежат выделение (3), показ заголовка (4)
    // и поведение (10–12). Обычные группы БП дают 0/1, раскрываемые — 1/1/1,
    // всплывающие — 1/0/0; сверено с Form.xml на четырёх справочниках.
    if (item.type == FormItem::Type::Group) {
        // Разрешать изменение содержимого (EnableContentChange) — третье поле
        // после имени записи группы; платформа пишет свойство только true.
        if (nameIndex >= 1) {
            if (const V8TextNode *contentChange = node.child(nameIndex + 3)) {
                if (contentChange->isInteger())
                    item.enableContentChange = contentChange->toInteger() != 0;
            }
        }
        if (const V8TextNode *layout = findLayoutNode(node)) {
            // Режим ориентации группы: 0 — вертикально, 1 — горизонтально,
            // 2 — горизонтально, если помещается (Group в XML не пишется),
            // 3 — всегда горизонтально. Сверено с Form.xml по 161 группе
            // корпуса (см. groupMode в FormItem).
            const int mode = layoutInteger(*layout, 27);
            if (mode >= 0)
                item.groupMode = mode;
            const int representation = layoutInteger(*layout, 3);
            if (representation >= 0 && representation <= 3)
                item.representation = static_cast<FormGroupRepresentation>(representation);
            const int showTitle = layoutInteger(*layout, 4);
            if (showTitle >= 0)
                item.showTitle = showTitle != 0;
            const int behaviorFlag = layoutInteger(*layout, 10);
            const int collapsible = layoutInteger(*layout, 11);
            const int collapsedSwitch = layoutInteger(*layout, 12);
            // Начальное состояние раскрытия группы (Collapsed) — двенадцатое
            // поле узла макета: 1 — группа свёрнута при открытии формы, 0 —
            // раскрыта; платформа пишет свойство только значением true.
            // Сверено с Form.xml по 115 группам десяти форм корпуса (у 26
            // задано Collapsed=true) — расхождений нет.
            if (collapsedSwitch == 1)
                item.collapsed = true;
            // Поведение группы: всплывающее задано двенадцатым полем записи,
            // раскрываемое — им же вместе с одиннадцатым. Обычное поведение
            // платформа пишет в Form.xml только когда оно задано явно (десятое
            // поле равно нулю); у 24 групп корпуса десятое поле равно единице
            // и Behavior в XML отсутствует, поэтому умолчание не подставляем.
            if (collapsedSwitch == 1) {
                item.behavior = collapsible == 1 ? FormGroupBehavior::Collapsible
                                                 : FormGroupBehavior::PopUp;
                item.hasBehavior = true;
            } else if (behaviorFlag == 0) {
                item.behavior = FormGroupBehavior::Usual;
                item.hasBehavior = true;
            }
            // Выравнивание содержимого группы: 17-е поле — горизонтальное
            // (0 — Left, 1 — Center, 2 — Right), 18-е — вертикальное
            // (0 — Top, 1 — Center, 2 — Bottom); 3 — Auto, значение по
            // умолчанию, платформа свойства не пишет. Сверено с Form.xml по
            // 524 группам корпуса: 24 расхождения было, расхождений нет.
            const int contentHorizontal = layoutInteger(*layout, 17);
            if (contentHorizontal >= 0 && contentHorizontal <= 2)
                item.contentHorizontalAlign = contentHorizontal;
            const int contentVertical = layoutInteger(*layout, 18);
            if (contentVertical >= 0 && contentVertical <= 2)
                item.contentVerticalAlign = contentVertical;
            // Ширина дочерних элементов (ChildItemsWidth) — второе поле того
            // же узла макета: 1 — Equal, 2 — LeftWide, 3 — LeftWidest,
            // 4 — LeftNarrow, 5 — LeftNarrowest; 0 — умолчание, платформа
            // в Form.xml не пишет. Девятнадцатое поле — сквозное выравнивание
            // (ThroughAlign): 0 — Use, 2 — умолчание. Двадцать первое поле —
            // объединение группы (United): 0 — false, 1 — умолчание true.
            // Сверено с Form.xml по 237 обычным группам корпуса и по формам
            // «ВнешниеПользователи» и «ЗаявкаНаОткрытиеЛицевыхСчетовСотрудников».
            if (item.groupKind == 5) {
                const int childItemsWidth = layoutInteger(*layout, 2);
                if (childItemsWidth >= 1 && childItemsWidth <= 5)
                    item.childItemsWidth = childItemsWidth;
                if (layoutInteger(*layout, 19) == 0)
                    item.throughAlign = 0;
                if (layoutInteger(*layout, 21) == 0)
                    item.united = false;
            }
        }
        // Автозаполнение командной панели: третье поле списка из трёх чисел,
        // который стоит через 14 полей после имени панели. Ноль — разработчик
        // снял автозаполнение (Form.xml пишет Autofill=false), единица —
        // значение по умолчанию. Сверено с Form.xml по 80 панелям корпуса.
        if (item.groupKind == 9 && nameIndex >= 0) {
            const V8TextNode *flags = node.child(nameIndex + 14);
            if (flags && flags->isList() && flags->children.size() >= 3) {
                const V8TextNode *autoFill = flags->child(2);
                if (autoFill && autoFill->isInteger())
                    item.autoFillFlag = autoFill->toInteger() != 0 ? 1 : 0;
                // Выравнивание панели (HorizontalAlign) — второе поле того же
                // списка: 0 — значение по умолчанию (Form.xml свойства не
                // пишет), 1 — Center, 2 — Right, 3 — Auto (пишется явно).
                // Сверено с Form.xml по 24 панелям корпуса и по панели с
                // Auto: расхождений нет.
                const V8TextNode *align = flags->child(1);
                if (align && align->isInteger()) {
                    const int value = static_cast<int>(align->toInteger());
                    if (value >= 1 && value <= 3)
                        item.contentHorizontalAlign = value;
                }
            }
        }
        // Положение своей командной панели (HorizontalLocation): тот же список
        // из трёх чисел через 14 полей после имени, что и у панели,
        // заполняемой платформой, — второй элемент равен 2 при Right и нулю при
        // значении по умолчанию. Сверено с Form.xml по 27 командным панелям
        // корпуса, у пяти из них задано Right.
        if (item.groupKind == 0 && nameIndex >= 0) {
            const V8TextNode *flags = node.child(nameIndex + 14);
            if (flags && flags->isList() && flags->children.size() >= 3) {
                const V8TextNode *location = flags->child(1);
                if (location && location->isInteger())
                    item.commandBarHorizontalLocation =
                        static_cast<int>(location->toInteger());
            }
        }
        // Компактное отображение группы кнопок: блок {2, […], 2, вид} в записи
        // группы, четвёртое поле равно 2 при «компактно» и нулю при обычном
        // отображении. Сверено с Form.xml по 62 группам кнопок корпуса.
        if (item.groupKind == 6) {
            const auto isNumber = [](const V8TextNode &value, int code) {
                return (value.isInteger() && value.toInteger() == code)
                    || (value.isScalar()
                        && value.scalar().trimmed() == QString::number(code));
            };
            for (const V8TextNode &child : node.children) {
                if (!child.isList() || child.children.size() < 4)
                    continue;
                const V8TextNode *kind = child.child(0);
                const V8TextNode *payload = child.child(1);
                const V8TextNode *mode = child.child(2);
                const V8TextNode *view = child.child(3);
                if (!kind || !isNumber(*kind, 2))
                    continue;
                if (!payload || !payload->isList() || !mode || !mode->isInteger()
                    || !view || !view->isInteger())
                    continue;
                if (view->toInteger() == 2)
                    item.compactButtonGroup = true;
                // Источник команд группы (CommandSource) — второй элемент той
                // же записи {0, GUID}: GUID самой формы (он же второй элемент
                // записи имени каждого элемента) — Form, платформенный GUID
                // 2ef6d6fa-847a-485e-8684-d37a3ab5efb8 — глобальные команды.
                // Сверено с Form.xml по 62 группам кнопок корпуса: у 6 задан
                // источник, все 62 совпали.
                if (payload->children.size() == 2 && payload->child(1)->isGuid()) {
                    const QString source = payload->child(1)->normalizedGuid();
                    if (source == item.guid)
                        item.commandSource = 0;
                    else if (source == QLatin1String(
                                 "2ef6d6fa-847a-485e-8684-d37a3ab5efb8"))
                        item.commandSource = 1;
                }
                break;
            }
        }
    }

    // Группа колонок таблицы: в её записи {2, группировка, …, показ заголовка,
    // …, закрепление} второй элемент — вид группировки (0 — Horizontal, 2 —
    // InCell, 1 — значение по умолчанию, Group в Form.xml не пишется), четвёртый
    // — показ заголовка (по умолчанию заголовок группы колонок скрыт),
    // двенадцатый — закрепление (1 — Left, 2 — Right; 0 — по умолчанию).
    // Сверено с Form.xml по 75 группам колонок корпуса.
    if (item.typeCode == 22 && item.groupKind == 2) {
        const auto isCode = [](const V8TextNode &value, const char *text) {
            return (value.isInteger()
                    && QString::number(value.toInteger()) == QLatin1String(text))
                || (value.isScalar() && value.scalar().trimmed() == QLatin1String(text));
        };
        for (const V8TextNode &child : node.children) {
            if (!child.isList() || child.children.size() < 3)
                continue;
            const V8TextNode *recordKind = child.child(0);
            if (!recordKind || !isCode(*recordKind, "2"))
                continue;
            // Второй элемент записи — целое; так запись отличается от ссылки на
            // элемент {номер, GUID}, которая тоже начинается с двойки.
            const V8TextNode *mode = child.child(1);
            if (!mode || !mode->isInteger())
                continue;
            item.columnGroupMode = static_cast<int>(mode->toInteger());
            if (const V8TextNode *header = child.child(3); header && header->isInteger())
                item.showInHeader = header->toInteger() != 0;
            if (const V8TextNode *fixing = child.child(11);
                fixing && fixing->isInteger()) {
                const int value = static_cast<int>(fixing->toInteger());
                if (value == 1 || value == 2)
                    item.fixingInTable = value;
            }
            break;
        }
    }

    // Кнопка: через 10 полей после имени — LocationInCommandBar
    // (0 — только в подменю «Еще», 1 — на панели). Сверено с Form.xml
    // на 30 кнопках пяти объектов БП.
    if (item.typeCode == 31 && nameIndex >= 0) {
        const V8TextNode *onBar = node.child(nameIndex + 10);
        if (onBar && onBar->isInteger())
            item.onCommandBar = onBar->toInteger() != 0;
        // DefaultItem кнопки: восьмое поле после имени, 1 — true.
        if (const V8TextNode *defaultItem = node.child(nameIndex + 8);
            defaultItem && defaultItem->isInteger() && defaultItem->toInteger() == 1) {
            item.defaultItem = true;
        }
        // Кнопка по умолчанию (DefaultButton): шестое поле после имени, 1 —
        // true. Сверено с Form.xml по 294 кнопкам корпуса: 4 кнопки с
        // DefaultButton совпали, у остальных поле равно 0.
        if (const V8TextNode *defaultButton = node.child(nameIndex + 6);
            defaultButton && defaultButton->isInteger()
            && defaultButton->toInteger() == 1) {
            item.defaultButton = true;
        }
        // Вид кнопки (Type): 0 — CommandBarButton, 1 — UsualButton,
        // 2 — Hyperlink; через 41 поле после имени. Сверено с Form.xml по
        // 294 кнопкам корпуса.
        if (const V8TextNode *type = node.child(nameIndex + 41);
            type && type->isInteger()) {
            item.buttonType = static_cast<int>(type->toInteger());
        }
        // Отображение (Representation): 0 — Text, 1 — Picture,
        // 2 — PictureAndText, 3 — Auto (по умолчанию).
        if (const V8TextNode *representation = node.child(nameIndex + 5);
            representation && representation->isInteger()) {
            const int value = static_cast<int>(representation->toInteger());
            if (value >= 0 && value <= 3)
                item.buttonRepresentation = value;
        }
        // SkipOnInput: 0/1 — явно задано, 2 — значение по умолчанию.
        if (const V8TextNode *skip = node.child(nameIndex + 24);
            skip && skip->isInteger()) {
            const int value = static_cast<int>(skip->toInteger());
            if (value == 0 || value == 1)
                item.skipOnInput = value;
        }
        // DisplayImportance: 3 — Usual; по умолчанию 0.
        if (const V8TextNode *importance = node.child(nameIndex + 43);
            importance && importance->isInteger()) {
            item.displayImportance = static_cast<int>(importance->toInteger());
        }
        // Размещение в командной панели (LocationInCommandBar): 44-е поле
        // после имени — 0 — по умолчанию (на панели), 1 — только в подменю
        // «Ещё», 2 — InCommandBar, 3 — и на панели, и в подменю.
        // Сверено с Form.xml по 294 кнопкам корпуса: 77 из 79 кнопок с
        // заданным свойством совпали (у двух кнопок «Справка» запись на поле
        // короче и код лежит сдвинутым).
        if (const V8TextNode *location = node.child(nameIndex + 44);
            location && location->isInteger()) {
            const int value = static_cast<int>(location->toInteger());
            if (value >= 1 && value <= 3)
                item.commandBarLocationCode = value;
        }
        // Выравнивание кнопки в группе: 3 — Auto (по умолчанию).
        if (const V8TextNode *align = node.child(nameIndex + 36);
            align && align->isInteger()) {
            item.buttonGroupHorizontalAlign = static_cast<int>(align->toInteger());
        }
        if (const V8TextNode *align = node.child(nameIndex + 37);
            align && align->isInteger()) {
            item.buttonGroupVerticalAlign = static_cast<int>(align->toInteger());
        }
        // Пределы размера кнопки — блок из пяти полей через двадцать девять
        // после кнопки: AutoMaxWidth, MaxWidth, (поле не используется),
        // AutoMaxHeight, MaxHeight. Сверено с Form.xml по 11 кнопкам формы
        // «ЕдиницыИзмеренияВЕТИС/ФормаСписка» (5 с AutoMaxWidth=false, одна с
        // AutoMaxHeight=false) и по кнопке «ПоказатьБыстрыеНастройки» с MaxWidth.
        readFieldSizeLimits(node, nameIndex + 29, &item);
        // Ссылка на команду — через три поля после имени: {номер, GUID}.
        // Номер — идентификатор команды формы, 0 — стандартная/общая команда
        // платформы, иное — идентификатор элемента-владельца стандартной
        // команды. Имя команды восстанавливается после разбора всего
        // документа (см. resolveButtonCommands).
        const V8TextNode *command = node.child(nameIndex + 3);
        if (command && command->isList() && command->children.size() == 2
            && command->child(0)->isInteger() && command->child(1)->isGuid()) {
            item.commandRef = static_cast<int>(command->child(0)->toInteger());
            item.commandGuid = command->child(1)->normalizedGuid();
        }
    }

    // Путь к данным поля: в бинарной форме он записан ссылками на реквизиты
    // метаданных и всегда лежит в нескольких полях после имени элемента.
    // Таблица тоже привязана к данным («Список», «Объект.Товары»), но её
    // описание пути стоит дальше — сразу за заголовком и его расположением.
    if ((isFieldCode(item.typeCode) || item.typeCode == 55) && nameIndex >= 0) {
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

    // Оформление: цвета и шрифты элемента. Ссылки на стили записаны GUID-ами
    // и отрицательными кодами платформы; таблица стилей конфигурации переводит
    // первые в имена, вторые — по постоянной таблице платформы.
    switch (item.type) {
    case FormItem::Type::Field:
        readFieldStyles(node, nameIndex, styles, &item);
        readFieldPictures(node, nameIndex, pictures, &item);
        break;
    case FormItem::Type::Group:
        readGroupStyles(node, nameIndex, styles, &item);
        break;
    case FormItem::Type::Decoration:
        readDecorationStyles(node, nameIndex, styles, &item);
        break;
    case FormItem::Type::Table:
        readTableStyles(node, styles, &item);
        break;
    case FormItem::Type::Button:
        readButtonStyles(node, styles, &item);
        readButtonPictures(node, nameIndex, pictures, &item);
        break;
    default:
        break;
    }

    for (const V8TextNode &child : node.children) {
        if (isItemNode(child))
            item.children.append(parseItem(child, styles, pictures));
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
                            QString *error, const FormBindingResolver *resolver,
                            const FormStyleTable *styles,
                            const FormPictureTable *pictures)
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

    // Положение командной панели формы: семнадцатое поле заголовка —
    // 0 None, 1 Auto (по умолчанию), 2 Top, 3 Bottom. Сверено с Form.xml по
    // формам корпуса: «Новости/ФормаВводаГиперссылки» (Bottom) даёт 3,
    // «КонтактныеЛица/ФормаЛиды» (None) — 0, «Новости/ФормаДокумента» без
    // свойства — 1, формы с Top — 2. Это поле начала записи, поэтому у
    // коротких форм его просто нет: тогда остаётся значение по умолчанию.
    if (const V8TextNode *location = header->child(17)) {
        if (location->isInteger()) {
            const int value = static_cast<int>(location->toInteger());
            if (value >= 0 && value <= 3)
                document->commandBarLocation = value;
        }
    }

    // Заголовок окна: девятое поле заголовка — «Автозаголовок» (0 — false,
    // 1 — значение по умолчанию), десятое — собственный заголовок формы.
    // Сверено на тестовой базе контролов: «Заголовок» (Title + AutoTitle
    // false) даёт 0 и текст, «ПоляВвода» без свойств — 1 и пустую запись
    // ["1","0"]. Конфигуратор показывает в полоске окна только Title, поэтому
    // синонимы объекта и формы в заголовок не подставляются.
    if (const V8TextNode *autoTitle = header->child(9)) {
        if (autoTitle->isInteger())
            document->autoTitleFlag = autoTitle->toInteger() != 0 ? 1 : 0;
    }
    if (const V8TextNode *title = header->child(10))
        document->formTitle = localizedText(*title);

    const V8TextNode *module = root.child(2);
    if (module && module->isScalar())
        document->moduleText = module->scalar();

    // Элементы формы — прямые дочерние элементы заголовка формы.
    for (const V8TextNode &child : header->children) {
        if (isItemNode(child))
            document->items.append(parseItem(child, styles, pictures));
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
                        document->items.append(parseItem(nested, styles, pictures));
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
            // Номер реквизита лежит в записи {вид, {номер}, …} сразу после вида.
            if (const V8TextNode *reference = entry->child(1)) {
                if (reference->isList() && reference->children.size() >= 1
                    && reference->child(0)->isInteger()) {
                    attribute.id = static_cast<int>(reference->child(0)->toInteger());
                } else if (reference->isInteger()) {
                    attribute.id = static_cast<int>(reference->toInteger());
                }
            }
            attribute.synonym = localizedText(*entry);
            attribute.typeHint = typeHintFromAttributes(*entry);
            if (const V8TextNode *settings = findListSettings(*entry))
                readListSettings(*settings, &attribute);
            readTableColumns(*entry, &attribute);
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
            // У записи команды первым идёт {идентификатор, GUID коллекции}:
            // на идентификатор ссылаются кнопки, GUID — это коллекция команд
            // формы (у всех команд формы она одна).
            const V8TextNode *reference = entry->child(1);
            if (reference->children.size() >= 2) {
                if (reference->child(0)->isInteger())
                    command.id = static_cast<int>(reference->child(0)->toInteger());
                if (reference->child(1)->isGuid())
                    command.collectionGuid = reference->child(1)->normalizedGuid();
            }
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
    collectStyleItems(document->items, styles, &document->styleItems);
    collectCommonPictures(document->items, pictures, document);
    ButtonCommandContext commandContext;
    for (const FormCommand &command : document->commands) {
        if (command.id > 0)
            commandContext.commandNames.insert(command.id, command.name);
        if (!command.collectionGuid.isEmpty())
            commandContext.collections.insert(command.collectionGuid.toLower());
    }
    collectItemNames(document->items, &commandContext.itemNames);
    resolveButtonCommands(&document->items, commandContext, resolver);
    if (resolver && !resolver->isEmpty())
        resolver->applyToDocument(document);
    return true;
}

bool FormReader::parseFile(const QString &path, FormDocument *document,
                           QString *error, const FormBindingResolver *resolver,
                           const FormStyleTable *styles,
                           const FormPictureTable *pictures)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Не удалось открыть форму %1: %2")
                         .arg(QFileInfo(path).fileName(), file.errorString());
        return false;
    }
    return parseBytes(file.readAll(), document, error, resolver, styles, pictures);
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
        // Код вида поля стоит перед именем — отдаём и его, чтобы отладочный
        // дамп можно было сверять с Form.xml напрямую.
        if (nameIndex >= 1) {
            const V8TextNode *kind = child.child(nameIndex - 1);
            if (kind && kind->isInteger())
                item.insert(QStringLiteral("kindCode"),
                            static_cast<int>(kind->toInteger()));
        }
        if (child.child(1)->children.size() >= 1
            && child.child(1)->child(0)->isInteger()) {
            item.insert(QStringLiteral("id"),
                        static_cast<int>(child.child(1)->child(0)->toInteger()));
        }
        QJsonArray slotValues;
        // Глубина 6: до вложенных списков выбора и картинок (они лежат в
        // записи элемента на третьем-четвёртом уровне вложенности).
        for (const V8TextNode &slot : child.children)
            slotValues.append(slotToJson(slot, 6));
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

    // Заголовок формы — корневая запись, тоже вне дерева элементов: в ней лежат
    // свойства самой формы (положение командной панели, прокрутка, ширина).
    // Дамп с номерами полей нужен, чтобы находить их при разборе свойств.
    QJsonArray headerSlots;
    for (int i = 0; i < header->children.size(); ++i) {
        QJsonObject slot;
        slot.insert(QStringLiteral("index"), i);
        slot.insert(QStringLiteral("value"), slotToJson(header->children.at(i), 2));
        headerSlots.append(slot);
    }
    result.insert(QStringLiteral("header"), headerSlots);

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
    // Команды формы — тоже вне дерева элементов. Кнопки ссылаются на них номером,
    // поэтому для сверки «номер -> имя» нужен дамп сырых записей.
    const V8TextNode *commands = root.child(5);
    if (commands && commands->isList()) {
        QJsonArray commandList;
        for (int i = 2; i < commands->children.size(); ++i) {
            const V8TextNode *entry = commands->child(i);
            if (!entry || !entry->isList())
                continue;
            QJsonObject command;
            int nameIndex = -1;
            command.insert(QStringLiteral("name"), findName(*entry, 2, &nameIndex));
            if (nameIndex >= 0)
                command.insert(QStringLiteral("nameIndex"), nameIndex);
            QJsonArray slotValues;
            for (const V8TextNode &slot : entry->children)
                slotValues.append(slotToJson(slot, 4));
            command.insert(QStringLiteral("slots"), slotValues);
            commandList.append(command);
        }
        result.insert(QStringLiteral("commands"), commandList);
    }
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

} // namespace v8reader
