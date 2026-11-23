#include "core/FormBindings.h"

#include "core/FormOwner.h"

#include <QRegularExpression>

#include <cmath>
#include <functional>

namespace v8reader {
namespace {

// Разделяет «Строка(50), Число(15,2)» на составной тип, не путая запятую
// внутри квалификаторов числа.
bool looksComposite(const QString &typeText)
{
    int depth = 0;
    for (const QChar ch : typeText) {
        if (ch == QLatin1Char('('))
            ++depth;
        else if (ch == QLatin1Char(')'))
            --depth;
        else if (ch == QLatin1Char(',') && depth == 0)
            return true;
    }
    return false;
}

// Реквизит формы по номеру, на который ссылается сегмент пути: в бинарной
// форме путь записан номером реквизита, а не порядком объявления (у
// «Авансового отчёта» «Объект» — 1, «Итого предыдущий аванс» — 3,
// «Состояние документа» — 8).
const FormAttribute *attributeById(const FormDocument &document, int id)
{
    if (id < 0)
        return nullptr;
    for (const FormAttribute &attribute : document.attributes) {
        if (attribute.id == id)
            return &attribute;
    }
    return nullptr;
}

// Реквизит формы, с которого начинается путь: сначала по номеру реквизита,
// затем по имени элемента (поле обычно названо как реквизит) и только потом
// по порядку объявления.
const FormAttribute *findRootAttribute(const FormDocument &document, int id,
                                       const QString &fallbackName)
{
    if (const FormAttribute *byId = attributeById(document, id))
        return byId;
    if (!fallbackName.isEmpty()) {
        for (const FormAttribute &attribute : document.attributes) {
            if (attribute.name.compare(fallbackName, Qt::CaseInsensitive) == 0)
                return &attribute;
        }
    }
    const int index = id - 1;
    if (index >= 0 && index < document.attributes.size())
        return &document.attributes.at(index);
    return nullptr;
}

} // namespace

void FormBindingResolver::clear()
{
    m_ownerKind.clear();
    m_ownerName.clear();
    m_commandNames.clear();
    m_byGuid.clear();
    m_byName.clear();
}

void FormBindingResolver::addObject(const MetadataObject &object)
{
    // Вид объекта нужен расшифровке стандартных реквизитов: код -2 — это «Код»
    // у справочников и «Номер» у документов, код -3 — «Наименование» и «Дата»
    // соответственно. Владелец формы известен только здесь, поэтому вид
    // запоминаем сразу: заголовок владельца заполняется позже разбора формы.
    m_ownerKind = ownerKindTag(object.typeTitle);
    m_ownerName = object.name;
    auto insert = [this](const MetadataEntry &entry, bool tabularSection) {
        if (entry.guid.isEmpty())
            return;
        Entry value;
        value.name = entry.name;
        value.synonym = entry.synonym;
        value.typeText = entry.typeText;
        value.tabularSection = tabularSection;
        m_byGuid.insert(entry.guid.toLower(), value);
    };
    // Поле динамического списка адресуется номером, поэтому имя — единственный
    // ключ, по которому его можно сопоставить с метаданными. В индекс попадают
    // только реквизиты верхнего уровня: колонки табличных частей адресуются
    // путём «ТабличнаяЧасть.Колонка» и одноимённым полям списка не мешают.
    auto insertName = [this](const MetadataEntry &entry) {
        if (entry.name.isEmpty() || entry.typeText.isEmpty())
            return;
        Entry value;
        value.name = entry.name;
        value.synonym = entry.synonym;
        value.typeText = entry.typeText;
        value.tabularSection = false;
        m_byName.insert(entry.name, value);
    };
    for (const MetadataSection &section : object.sections) {
        const bool tabular = section.kind == MetadataSectionKind::TabularSections;
        const bool commands = section.kind == MetadataSectionKind::Commands;
        for (const MetadataEntry &entry : section.entries) {
            // Команды объекта — целиком отдельная таблица: кнопки ссылаются на
            // них GUID-ом, а в CommandName нужно имя команды и её владелец.
            if (commands && !entry.guid.isEmpty() && !entry.name.isEmpty())
                m_commandNames.insert(entry.guid.toLower(), entry.name);
            // Табличные части в таблицу поиска входят ради имён: путь колонки
            // адресуется через них («Объект.Товары.Количество»), хотя типа
            // у самой табличной части нет.
            insert(entry, tabular);
            if (!tabular)
                insertName(entry);
            for (const MetadataEntry &child : entry.children)
                insert(child, false);
        }
    }
}

QString FormBindingResolver::commandFqn(const QString &guid) const
{
    if (guid.isEmpty() || m_ownerKind.isEmpty() || m_ownerName.isEmpty())
        return QString();
    const QString name = m_commandNames.value(guid.toLower());
    if (name.isEmpty())
        return QString();
    return m_ownerKind + QLatin1Char('.') + m_ownerName
        + QStringLiteral(".Command.") + name;
}

QString FormBindingResolver::typeTextForStandardCode(int code)
{
    switch (code) {
    case -2: // Номер / Код / Номер строки — строковый стандартный реквизит
        return QStringLiteral("Строка(9)");
    case -3: // Дата
        return QStringLiteral("Дата");
    default:
        return QString();
    }
}

FormValueKind FormBindingResolver::kindForTypeText(const QString &typeText)
{
    const QString text = typeText.trimmed();
    if (text.isEmpty())
        return FormValueKind::Unknown;
    if (looksComposite(text))
        return FormValueKind::Composite;
    if (text.startsWith(QStringLiteral("Строка")))
        return FormValueKind::String;
    if (text.startsWith(QStringLiteral("Число")))
        return FormValueKind::Number;
    if (text.startsWith(QStringLiteral("Булево")))
        return FormValueKind::Boolean;
    if (text.startsWith(QStringLiteral("Дата")) || text.startsWith(QStringLiteral("Время")))
        return FormValueKind::Date;
    if (text.contains(QStringLiteral("ХранилищеЗначения"))
        || text.startsWith(QStringLiteral("Картинка"))) {
        return FormValueKind::Picture;
    }
    if (text.contains(QStringLiteral("Ссылка"))) {
        if (text.startsWith(QStringLiteral("Перечисление")))
            return FormValueKind::Enum;
        return FormValueKind::Reference;
    }
    if (text.startsWith(QStringLiteral("Составной")))
        return FormValueKind::Composite;
    return FormValueKind::Unknown;
}

int FormBindingResolver::charsForType(const QString &typeText, FormValueKind kind)
{
    const QString text = typeText.trimmed();
    switch (kind) {
    case FormValueKind::String: {
        static const QRegularExpression pattern(
            QStringLiteral("Строка\\((\\d+)\\)"));
        const QRegularExpressionMatch match = pattern.match(text);
        int length = 0;
        if (match.hasMatch())
            length = match.captured(1).toInt();
        if (length <= 0)
            return 20;
        return qBound(6, length, 40);
    }
    case FormValueKind::Number: {
        static const QRegularExpression pattern(
            QStringLiteral("Число\\((\\d+)(?:\\s*,\\s*(\\d+))?\\)"));
        const QRegularExpressionMatch match = pattern.match(text);
        if (!match.hasMatch())
            return 12;
        const int digits = match.captured(1).toInt();
        const int fraction = match.captured(2).toInt();
        if (digits <= 0)
            return 15;
        // Формула «LogFormElementsDefFilter» из платформы (как в BslEdit).
        int length = digits;
        if (fraction != 0) {
            ++length;
            if (digits == fraction)
                ++length;
        }
        const int whole = digits - fraction;
        int triads = whole / 3;
        if (triads != 0 && whole % 3 == 0)
            --triads;
        length += triads;
        const double converted = length * (length >= 6 ? 5.4 : 5.5) / 8.0;
        return static_cast<int>(std::ceil(converted));
    }
    case FormValueKind::Date:
        if (text.startsWith(QStringLiteral("ДатаВремя")))
            return 17;
        if (text.startsWith(QStringLiteral("Время")))
            return 8;
        return 9;
    case FormValueKind::Boolean:
        return 10;
    case FormValueKind::Reference:
    case FormValueKind::Enum:
    case FormValueKind::Composite:
        return 15;
    case FormValueKind::Picture:
        return 2;
    default:
        return 0;
    }
}

FormBinding FormBindingResolver::resolve(const QVector<FormPathSegment> &path,
                                         const FormDocument &document,
                                         const QString &fallbackName) const
{
    FormBinding binding;
    if (path.isEmpty())
        return binding;

    // Односегментный путь адресует реквизит формы; номер в пути — это номер
    // реквизита в контейнере формы, он же лежит в записи реквизита. Номер не
    // совпадает с порядком объявления, поэтому расшифровываем по номеру, а имя
    // элемента — лишь вторая зацепка (поле «ПереносОстатков1» привязано к
    // реквизиту «ПереносОстатков»).
    if (path.size() == 1 && path.at(0).kind == FormPathSegment::Kind::Index) {
        const FormAttribute *attribute =
            findRootAttribute(document, path.at(0).code, fallbackName);
        if (attribute) {
            binding.text = attribute->name;
            binding.title = attribute->synonym;
            if (!attribute->typeHint.isEmpty()) {
                binding.typeText = attribute->typeHint;
                binding.kind = kindForTypeText(attribute->typeHint);
                binding.chars = charsForType(attribute->typeHint, binding.kind);
            }
            return binding;
        }
        // Реквизит не опознан: рендерер показывает поле только с путём
        // к данным, поэтому оставляем путь по имени элемента — тип остаётся
        // неизвестным, но поле и его заголовок не пропадают.
        binding.text = fallbackName;
        return binding;
    }

    QStringList parts;
    const Entry *last = nullptr;
    // Признак того, что предыдущий сегмент — табличная часть: стандартный
    // реквизит «-2» внутри неё означает номер строки, а не номер/код.
    bool insideTable = false;
    // Реквизит формы, с которого начинается путь: для динамического списка
    // в нём лежит карта полей, по которой расшифровываются номера.
    const FormAttribute *rootAttribute = nullptr;
    const QString kind = m_ownerKind.isEmpty() ? ownerKindTag(document.ownerTitle)
                                               : m_ownerKind;
    for (int i = 0; i < path.size(); ++i) {
        const FormPathSegment &segment = path.at(i);
        switch (segment.kind) {
        case FormPathSegment::Kind::Index: {
            if (i == 0) {
                // Корневой реквизит формы («Объект», «Список», «Отчет»):
                // номер сегмента — это номер реквизита в контейнере формы.
                const FormAttribute *root = findRootAttribute(document, segment.code, QString());
                if (root) {
                    rootAttribute = root;
                    parts.append(root->name);
                    if (!root->typeHint.isEmpty() && path.size() == 1)
                        binding.typeText = root->typeHint;
                } else {
                    parts.append(QStringLiteral("Объект"));
                }
            } else {
                // Второй и следующие сегменты-номера адресуют поле
                // динамического списка: номер расшифровывается картой полей
                // реквизита формы. Иначе остаётся номер — лучше показать его,
                // чем потерять привязку колонки.
                QString fieldName;
                if (rootAttribute && !rootAttribute->listFields.isEmpty())
                    fieldName = rootAttribute->listFields.value(segment.code);
                const bool tableColumn = rootAttribute
                    && rootAttribute->columnNames.contains(segment.code);
                if (fieldName.isEmpty() && tableColumn)
                    fieldName = rootAttribute->columnNames.value(segment.code);
                if (fieldName.isEmpty()) {
                    parts.append(QStringLiteral("#%1").arg(segment.code));
                    break;
                }
                parts.append(fieldName);
                if (tableColumn) {
                    // Колонка таблицы значений: заголовок и тип объявлены
                    // в самой форме, метаданные объекта тут не при чём.
                    binding.title = rootAttribute->columnTitles.value(segment.code);
                    const QString columnType =
                        rootAttribute->columnTypes.value(segment.code);
                    if (!columnType.isEmpty()) {
                        binding.typeText = columnType;
                        binding.kind = kindForTypeText(columnType);
                        binding.chars = charsForType(columnType, binding.kind);
                    }
                    last = nullptr;
                    break;
                }
                // Тип и заголовок поля — из метаданных объекта, если такое
                // имя там есть (стандартные реквизиты — «Ссылка», «Код» —
                // описаны в stdCaptions и здесь не находятся).
                const auto byName = m_byName.constFind(fieldName);
                if (byName != m_byName.constEnd()) {
                    binding.title = byName->synonym;
                    binding.typeText = byName->typeText;
                    binding.kind = kindForTypeText(byName->typeText);
                    binding.chars = charsForType(byName->typeText, binding.kind);
                    last = &byName.value();
                }
            }
            break;
        }
        case FormPathSegment::Kind::Attribute: {
            const auto it = m_byGuid.constFind(segment.guid.toLower());
            if (it == m_byGuid.constEnd()) {
                if (i == path.size() - 1 && !fallbackName.isEmpty()) {
                    // Колонка, реквизит которой не попал в индекс метаданных
                    // (табличные части читаются не у всех объектов): в XML
                    // путь — «Объект.ТЧ.Колонка», а имя элемента собрано из
                    // имён сегментов пути («ТЧКолонка» либо
                    // «ТЧСсылка.Колонка»). Поэтому последовательно снимаем
                    // с имени уже разобранные сегменты пути.
                    QString column = fallbackName;
                    for (int part = 0; part < parts.size(); ++part) {
                        const QString &name = parts.at(part);
                        if (!name.isEmpty() && column.startsWith(name)
                            && column.size() > name.size()) {
                            column = column.mid(name.size());
                        }
                    }
                    parts.append(column.isEmpty() ? fallbackName : column);
                } else {
                    parts.append(QStringLiteral("?"));
                }
                insideTable = false;
                break;
            }
            parts.append(it->name);
            insideTable = it->tabularSection;
            if (i == path.size() - 1) {
                last = &it.value();
            } else if (!it->typeText.isEmpty()) {
                // Табличная часть: тип у неё не объявлен, но синоним нужен
                // (заголовок самой таблицы).
                last = &it.value();
            }
            break;
        }
        case FormPathSegment::Kind::StandardAttribute: {
            // Коды стандартных реквизитов нумеруются в общем для всех объектов
            // пространстве, а имя зависит от вида объекта: у справочника пара
            // — «Код»/«Наименование», у документа — «Номер»/«Дата».
            // Сверено с Form.xml по 26 формам: код -2 даёт Code/Number,
            // код -3 — Description/Date, в таблице -2 — LineNumber.
            const bool catalogLike = kind == QLatin1String("Catalog")
                || kind == QLatin1String("ChartOfCharacteristicTypes");
            const bool documentLike = kind == QLatin1String("Document")
                || kind == QLatin1String("DocumentJournal")
                || kind == QLatin1String("BusinessProcess")
                || kind == QLatin1String("Task");
            // Стандартный реквизит вложенного объекта (путь идёт через ссылку,
            // например «Объект.Товары.Номенклатура.Code»): имя зависит от вида
            // того объекта, на который ссылается предыдущий сегмент, а не от
            // вида владельца формы.
            const QString referencedType = last ? last->typeText : QString();
            const bool referencedCatalog =
                referencedType.contains(QStringLiteral("СправочникСсылка"))
                || referencedType.contains(QStringLiteral("ПланВидовХарактеристикСсылка"))
                || referencedType.contains(QStringLiteral("ПланСчетовСсылка"))
                || referencedType.contains(QStringLiteral("ПланВидовРасчетаСсылка"));
            const bool referencedDocument =
                referencedType.contains(QStringLiteral("ДокументСсылка"))
                || referencedType.contains(QStringLiteral("БизнесПроцессСсылка"))
                || referencedType.contains(QStringLiteral("ЗадачаСсылка"));
            const bool catalogNames = referencedCatalog
                || (!referencedDocument && catalogLike);
            const bool documentNames = referencedDocument
                || (!referencedCatalog && documentLike);
            QString standardName;
            if (segment.code == -2) {
                if (insideTable)
                    standardName = QStringLiteral("LineNumber");
                else if (catalogNames)
                    standardName = QStringLiteral("Code");
                else
                    standardName = QStringLiteral("Number");
            } else if (segment.code == -3) {
                standardName = (!documentNames)
                    ? QStringLiteral("Description")
                    : QStringLiteral("Date");
            } else {
                standardName = typeTextForStandardCode(segment.code);
            }
            if (standardName.isEmpty())
                standardName = QStringLiteral("Стандартный%1").arg(segment.code);
            parts.append(standardName);

            const QString standardType = typeTextForStandardCode(segment.code);
            if (!standardType.isEmpty()) {
                binding.typeText = standardType;
                binding.kind = kindForTypeText(standardType);
                binding.chars = charsForType(standardType, binding.kind);
            }
            last = nullptr;
            insideTable = false;
            break;
        }
        }
    }

    binding.text = parts.join(QLatin1Char('.'));
    if (last && !last->tabularSection) {
        binding.title = last->synonym;
        if (binding.typeText.isEmpty()) {
            binding.typeText = last->typeText;
            binding.kind = kindForTypeText(binding.typeText);
            binding.chars = charsForType(binding.typeText, binding.kind);
        }
    }
    return binding;
}

void FormBindingResolver::applyToDocument(FormDocument *document) const
{
    if (!document)
        return;

    std::function<void(QVector<FormItem> &)> apply = [&](QVector<FormItem> &items) {
        for (FormItem &item : items) {
            if (!item.dataPath.isEmpty()) {
                const FormBinding binding = resolve(item.dataPath, *document, item.name);
                item.dataPathText = binding.text;
                if (!binding.title.isEmpty())
                    item.dataTitle = binding.title;
                if (!binding.typeText.isEmpty()) {
                    item.typeText = binding.typeText;
                    item.valueKind = binding.kind;
                    item.valueChars = binding.chars;
                }
            }
            apply(item.children);
        }
    };
    apply(document->items);
}

} // namespace v8reader
