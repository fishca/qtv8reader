#include "core/FormOwner.h"

#include "core/ConfigurationReader.h"
#include "core/V8Text.h"

#include <QHash>
#include <QRegularExpression>
#include <QStringList>

#include <functional>

namespace v8reader {

namespace {

// Разбор «Строка(50)», «Число(15,2)», «СправочникСсылка.Валюты» на то, что
// ожидает рендерер в objectMeta: тип XSD, квалификаторы, длину строки и вид
// даты. Составной тип перечисляется через запятую — так же, как в Form.xml.
struct TypeDescription
{
    QString type;
    QString numberQ;    // «15:2»
    QString stringLen;  // «50»
    QString dateFraction; // Date/DateTime/Time
};

bool hasTopLevelComma(const QString &text)
{
    int depth = 0;
    for (const QChar ch : text) {
        if (ch == QLatin1Char('('))
            ++depth;
        else if (ch == QLatin1Char(')'))
            --depth;
        else if (ch == QLatin1Char(',') && depth == 0)
            return true;
    }
    return false;
}

QStringList splitComposite(const QString &text)
{
    QStringList parts;
    int depth = 0;
    int start = 0;
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('('))
            ++depth;
        else if (ch == QLatin1Char(')'))
            --depth;
        else if (ch == QLatin1Char(',') && depth == 0) {
            parts.append(text.mid(start, i - start).trimmed());
            start = i + 1;
        }
    }
    parts.append(text.mid(start).trimmed());
    return parts;
}

QString referenceTypeName(const QString &typeText)
{
    static const QVector<QPair<QString, QString>> prefixes = {
        {QStringLiteral("СправочникСсылка."), QStringLiteral("cfg:CatalogRef.")},
        {QStringLiteral("ДокументСсылка."), QStringLiteral("cfg:DocumentRef.")},
        {QStringLiteral("ПеречислениеСсылка."), QStringLiteral("cfg:EnumRef.")},
        {QStringLiteral("ПланОбменаСсылка."), QStringLiteral("cfg:ExchangePlanRef.")},
        {QStringLiteral("ПланСчетовСсылка."), QStringLiteral("cfg:ChartOfAccountsRef.")},
        {QStringLiteral("ПланВидовХарактеристикСсылка."),
         QStringLiteral("cfg:ChartOfCharacteristicTypesRef.")},
        {QStringLiteral("ПланВидовРасчетаСсылка."),
         QStringLiteral("cfg:ChartOfCalculationTypesRef.")},
        {QStringLiteral("БизнесПроцессСсылка."), QStringLiteral("cfg:BusinessProcessRef.")},
        {QStringLiteral("ЗадачаСсылка."), QStringLiteral("cfg:TaskRef.")},
        {QStringLiteral("ВнешнийИсточникДанныхСсылка."),
         QStringLiteral("cfg:ExternalDataSourceRef.")},
        {QStringLiteral("ОпределяемыйТип."), QStringLiteral("cfg:DefinedType.")},
        {QStringLiteral("ХранилищеЗначения"), QStringLiteral("xs:base64Binary")},
        // Динамический список — не тип значения, а вид реквизита формы:
        // рендерер узнаёт его по имени типа и включает режим списка.
        {QStringLiteral("ДинамическийСписок"), QStringLiteral("cfg:DynamicList")},
    };
    for (const auto &entry : prefixes) {
        if (typeText.startsWith(entry.first))
            return entry.second + typeText.mid(entry.first.size());
    }
    return QString();
}

TypeDescription describeType(const QString &typeText)
{
    TypeDescription description;
    const QString text = typeText.trimmed();
    if (text.isEmpty())
        return description;

    if (hasTopLevelComma(text)) {
        QStringList mapped;
        for (const QString &part : splitComposite(text)) {
            const TypeDescription item = describeType(part);
            if (!item.type.isEmpty())
                mapped.append(item.type);
        }
        description.type = mapped.join(QStringLiteral(", "));
        return description;
    }

    if (text == QStringLiteral("Булево")) {
        description.type = QStringLiteral("xs:boolean");
        return description;
    }
    if (text == QStringLiteral("Дата")) {
        description.type = QStringLiteral("xs:dateTime");
        description.dateFraction = QStringLiteral("Date");
        return description;
    }
    if (text == QStringLiteral("ДатаВремя")) {
        description.type = QStringLiteral("xs:dateTime");
        description.dateFraction = QStringLiteral("DateTime");
        return description;
    }
    if (text == QStringLiteral("Время")) {
        description.type = QStringLiteral("xs:dateTime");
        description.dateFraction = QStringLiteral("Time");
        return description;
    }
    if (text.startsWith(QStringLiteral("Строка"))) {
        description.type = QStringLiteral("xs:string");
        static const QRegularExpression pattern(QStringLiteral("Строка\\((\\d+)\\)"));
        const QRegularExpressionMatch match = pattern.match(text);
        description.stringLen = match.hasMatch() ? match.captured(1)
                                                 : QStringLiteral("0");
        return description;
    }
    if (text.startsWith(QStringLiteral("Число"))) {
        description.type = QStringLiteral("xs:decimal");
        static const QRegularExpression pattern(
            QStringLiteral("Число\\((\\d+)(?:\\s*,\\s*(\\d+))?\\)"));
        const QRegularExpressionMatch match = pattern.match(text);
        if (match.hasMatch()) {
            const QString digits = match.captured(1);
            const QString fraction = match.captured(2).isEmpty()
                ? QStringLiteral("0")
                : match.captured(2);
            description.numberQ = digits + QLatin1Char(':') + fraction;
        }
        return description;
    }
    const QString reference = referenceTypeName(text);
    if (!reference.isEmpty()) {
        description.type = reference;
        return description;
    }
    return description;
}

void mergeType(FormOwnerMeta *meta, const QString &key, const QString &typeText)
{
    if (key.isEmpty() || typeText.isEmpty())
        return;
    const TypeDescription description = describeType(typeText);
    if (description.type.isEmpty())
        return;
    meta->types.insert(key, description.type);
    if (!description.numberQ.isEmpty())
        meta->numberQ.insert(key, description.numberQ);
    if (!description.stringLen.isEmpty())
        meta->stringLen.insert(key, description.stringLen);
    if (!description.dateFraction.isEmpty())
        meta->dateFraction.insert(key, description.dateFraction);
}

bool sameTitle(const QString &left, const QString &right)
{
    return left.compare(right, Qt::CaseInsensitive) == 0;
}

// Значение проверки заполнения в терминах Form.xml.
QString fillCheckingText(int code)
{
    switch (code) {
    case 0:
        return QStringLiteral("DontCheck");
    case 1:
        return QStringLiteral("ShowError");
    case 2:
        return QStringLiteral("ShowWarning");
    default:
        return QString();
    }
}

bool containsListTable(const QVector<FormItem> &items, const QString &objectName)
{
    for (const FormItem &item : items) {
        if (item.type == FormItem::Type::Table
            && (item.name == QStringLiteral("Список")
                || item.synonym == QStringLiteral("Список")
                || (!objectName.isEmpty()
                    && (item.name == objectName || item.synonym == objectName)))) {
            return true;
        }
        if (containsListTable(item.children, objectName))
            return true;
    }
    return false;
}

// Список свойств объекта метаданных: запись объекта в контейнере устроена как
// {1, {<свойства>}, …}, поэтому свойства — второй элемент корня. Их число
// зависит от вида объекта (у документа 40 полей, у справочника 48), а
// смещения внутри списка постоянны для вида: длина номера документа — 12-е
// поле, длина кода справочника — 17-е, наименования — 19-е. Проверено по
// выгрузке в XML: 7 документов (длины 0, 9, 11, 12, 13) и 7 справочников
// (коды 3…11, наименования 10…150) — расхождений нет.
//
// Автонумерация — 15-е поле и у документа, и у справочника: проверено
// сверкой с XML-выгрузкой на 16 документах и 9 справочниках, у которых
// независимо меняются проверка уникальности (14-е поле), периодичность
// номера (13-е) и режим проведения. Значение — 0/1.
const V8TextNode *objectPropertyList(const V8TextNode &root)
{
    if (!root.isList() || root.children.size() < 2)
        return nullptr;
    const V8TextNode *properties = root.child(1);
    return properties && properties->isList() ? properties : nullptr;
}

int propertyInteger(const V8TextNode *properties, int index)
{
    if (!properties)
        return 0;
    const V8TextNode *node = properties->child(index);
    if (!node || node->isList())
        return 0;
    bool ok = false;
    const qint64 value = node->toInteger(&ok);
    return ok ? int(value) : 0;
}

// Логические свойства лежат в списке числами 0/1, а рендерер ждёт строки
// «true»/«false»: пустая строка означает «в метаданных не записано».
QString propertyFlag(const V8TextNode *properties, int index)
{
    if (!properties)
        return QString();
    const V8TextNode *node = properties->child(index);
    if (!node || node->isList())
        return QString();
    const QString value = node->scalar().trimmed();
    if (value == QLatin1String("1"))
        return QStringLiteral("true");
    if (value == QLatin1String("0"))
        return QStringLiteral("false");
    return QString();
}

} // namespace

QString ownerKindTag(const QString &typeTitle)
{
    static const QVector<QPair<QString, QString>> titles = {
        {QStringLiteral("Документ"), QStringLiteral("Document")},
        {QStringLiteral("Справочник"), QStringLiteral("Catalog")},
        {QStringLiteral("Обработка"), QStringLiteral("DataProcessor")},
        {QStringLiteral("Отчет"), QStringLiteral("Report")},
        {QStringLiteral("План видов характеристик"),
         QStringLiteral("ChartOfCharacteristicTypes")},
        {QStringLiteral("План счетов"), QStringLiteral("ChartOfAccounts")},
        {QStringLiteral("План обмена"), QStringLiteral("ExchangePlan")},
        {QStringLiteral("План видов расчета"), QStringLiteral("ChartOfCalculationTypes")},
        {QStringLiteral("Задача"), QStringLiteral("Task")},
        {QStringLiteral("Бизнес-процесс"), QStringLiteral("BusinessProcess")},
        {QStringLiteral("Регистр сведений"), QStringLiteral("InformationRegister")},
        {QStringLiteral("Регистр накопления"), QStringLiteral("AccumulationRegister")},
        {QStringLiteral("Регистр бухгалтерии"), QStringLiteral("AccountingRegister")},
        {QStringLiteral("Регистр расчета"), QStringLiteral("CalculationRegister")},
        {QStringLiteral("Журнал документов"), QStringLiteral("DocumentJournal")},
        {QStringLiteral("Константа"), QStringLiteral("Constant")},
        {QStringLiteral("Перечисление"), QStringLiteral("Enum")},
        {QStringLiteral("План обмена"), QStringLiteral("ExchangePlan")},
    };
    for (const auto &entry : titles) {
        if (typeTitle.startsWith(entry.first))
            return entry.second;
    }
    return QString();
}

QString rendererTypeFor(const QString &typeText)
{
    return describeType(typeText).type;
}

QString rendererDateFractionFor(const QString &typeText)
{
    return describeType(typeText).dateFraction;
}

RendererTypeParts rendererTypeParts(const QString &typeText)
{
    const TypeDescription description = describeType(typeText);
    RendererTypeParts parts;
    parts.type = description.type;
    parts.stringLen = description.stringLen;
    parts.numberQ = description.numberQ;
    parts.dateFraction = description.dateFraction;
    return parts;
}

void applyOwnerMetadata(const MetadataObject &object, FormDocument *document)
{
    if (!document)
        return;

    const QString kindTag = ownerKindTag(object.typeTitle);
    FormOwnerMeta &meta = document->ownerMeta;
    meta = FormOwnerMeta();
    meta.valid = true;
    meta.kindTag = kindTag;
    meta.name = object.name;
    meta.synonym = object.synonym;
    meta.listPresentation = object.synonym;

    document->ownerTitle = object.typeTitle.isEmpty()
        ? object.name
        : QStringLiteral("%1.%2").arg(object.typeTitle, object.name);
    document->ownerSynonym = object.synonym;

    // Табличные части отделены от остальных таблиц состава: их реквизиты
    // адресуются с префиксом «ТабличнаяЧасть.», как в objectMeta рендерера.
    for (const MetadataSection &section : object.sections) {
        const bool tabular = section.kind == MetadataSectionKind::TabularSections;
        for (const MetadataEntry &entry : section.entries) {
            if (entry.name.isEmpty())
                continue;
            if (!entry.synonym.isEmpty()) {
                meta.captions.insert(entry.name, entry.synonym);
                document->titles.insert(entry.name, entry.synonym);
            }
            mergeType(&meta, entry.name, entry.typeText);
            const QString checking = fillCheckingText(entry.fillChecking);
            if (!checking.isEmpty())
                meta.fillChecking.insert(entry.name, checking);
            if (!tabular)
                continue;
            for (const MetadataEntry &child : entry.children) {
                if (child.name.isEmpty())
                    continue;
                const QString key = entry.name + QLatin1Char('.') + child.name;
                if (!child.synonym.isEmpty()) {
                    meta.captions.insert(key, child.synonym);
                    document->titles.insert(key, child.synonym);
                }
                mergeType(&meta, key, child.typeText);
                const QString childChecking = fillCheckingText(child.fillChecking);
                if (!childChecking.isEmpty())
                    meta.fillChecking.insert(key, childChecking);
            }
        }
    }

    const QString objectName = object.name;
    const bool hasTable = containsListTable(document->items, objectName);
    const bool isDocument = sameTitle(kindTag, QStringLiteral("Document"));
    const bool isCatalog = sameTitle(kindTag, QStringLiteral("Catalog"));
    // Стандартный реквизит «Дата» документа платформа проверяет на заполнение
    // («Показывать ошибку»), поэтому в формах документов пустое поле даты
    // подчёркнуто красным пунктиром. Сам реквизит в метаданных не записан:
    // в выгрузке в XML значение одинаково у всех 402 документов конфигурации.
    if (isDocument && !meta.fillChecking.contains(QStringLiteral("Date")))
        meta.fillChecking.insert(QStringLiteral("Date"), QStringLiteral("ShowError"));
    document->runtimeListCommands = isDocument && hasTable;
    document->runtimeDocumentCommands = isDocument && !hasTable;
    document->runtimeCatalogCommands = isCatalog && !hasTable;
    document->runtimeCatalogListCommands = isCatalog && hasTable;
}

void applyOwnerStandardAttributes(const QString &metadataText, FormDocument *document)
{
    if (!document || metadataText.trimmed().isEmpty() || !document->ownerMeta.valid)
        return;

    V8TextNode root;
    if (!V8Text::parse(metadataText, root, nullptr))
        return;

    const V8TextNode *properties = objectPropertyList(root);
    if (!properties)
        return;

    FormOwnerMeta &meta = document->ownerMeta;
    const QString kindTag = meta.kindTag;
    if (kindTag == QLatin1String("Document") || kindTag == QLatin1String("Catalog"))
        meta.autonumbering = propertyFlag(properties, 15);
    if (kindTag == QLatin1String("Document")) {
        // Номер документа платформы — строка длины NumberLength. Все документы
        // БП нумеруются строками; для числовой нумерации ширина отличалась бы
        // на один символ, но такого вида в корпусе нет.
        const int numberLength = propertyInteger(properties, 12);
        if (numberLength > 0) {
            meta.numberLength = numberLength;
            meta.types.insert(QStringLiteral("Number"), QStringLiteral("xs:string"));
            meta.stringLen.insert(QStringLiteral("Number"), QString::number(numberLength));
        }
        // Дата документа хранится как дата-время: от вида значения зависит
        // ширина поля и точечная маска в конфигураторе.
        meta.types.insert(QStringLiteral("Date"), QStringLiteral("xs:dateTime"));
        meta.dateFraction.insert(QStringLiteral("Date"), QStringLiteral("DateTime"));
    } else if (kindTag == QLatin1String("Catalog")) {
        const int codeLength = propertyInteger(properties, 17);
        if (codeLength > 0) {
            meta.codeLength = codeLength;
            meta.codeType = QStringLiteral("String");
            meta.types.insert(QStringLiteral("Code"), QStringLiteral("xs:string"));
            meta.stringLen.insert(QStringLiteral("Code"), QString::number(codeLength));
        }
        const int descriptionLength = propertyInteger(properties, 19);
        if (descriptionLength > 0) {
            meta.descriptionLength = descriptionLength;
            meta.types.insert(QStringLiteral("Description"), QStringLiteral("xs:string"));
            meta.stringLen.insert(QStringLiteral("Description"),
                                  QString::number(descriptionLength));
        }
    }
}

// Имена справочников, на которые ссылаются типы реквизитов формы. Составной
// тип перечисляет их через запятую: «cfg:CatalogRef.ФизическиеЛица,
// cfg:CatalogRef.ОтдельныеЛица».
QStringList referencedCatalogNames(const FormOwnerMeta &meta)
{
    static const QString prefix = QStringLiteral("CatalogRef.");
    QStringList names;
    for (auto it = meta.types.constBegin(); it != meta.types.constEnd(); ++it) {
        const QStringList parts = it.value().split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            QString type = part.trimmed();
            if (type.startsWith(QLatin1String("cfg:")))
                type = type.mid(4);
            if (!type.startsWith(prefix))
                continue;
            const QString name = type.mid(prefix.size());
            if (!name.isEmpty() && !names.contains(name))
                names.append(name);
        }
    }
    return names;
}

void applyReferencedCatalogMeta(const ConfigurationReader &reader, FormDocument *document)
{
    if (!document || !document->ownerMeta.valid)
        return;

    const QStringList names = referencedCatalogNames(document->ownerMeta);
    if (names.isEmpty())
        return;

    FormOwnerMeta &meta = document->ownerMeta;
    for (const MetadataCategory &category : reader.configuration().categories) {
        for (const MetadataObject &object : category.objects) {
            if (!names.contains(object.name))
                continue;
            // Имя может совпасть у объектов разных видов, а длина
            // представления, по которой платформа считает ширину поля
            // ссылки, есть только у справочника.
            if (ownerKindTag(object.typeTitle) != QLatin1String("Catalog"))
                continue;
            if (meta.refMeta.contains(object.name))
                continue;
            QString text;
            if (!reader.readObjectText(object, &text, nullptr))
                continue;
            V8TextNode root;
            if (!V8Text::parse(text, root, nullptr))
                continue;
            const V8TextNode *properties = objectPropertyList(root);
            FormRefMeta reference;
            reference.name = object.name;
            reference.descriptionLength = propertyInteger(properties, 19);
            reference.codeLength = propertyInteger(properties, 17);
            if (reference.descriptionLength <= 0 && reference.codeLength <= 0)
                continue;
            reference.codeType = QStringLiteral("String");
            meta.refMeta.insert(object.name, reference);
        }
    }
}

} // namespace v8reader
