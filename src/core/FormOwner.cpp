#include "core/FormOwner.h"

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

    if (text == QLatin1String("Булево")) {
        description.type = QStringLiteral("xs:boolean");
        return description;
    }
    if (text == QLatin1String("Дата")) {
        description.type = QStringLiteral("xs:dateTime");
        description.dateFraction = QStringLiteral("Date");
        return description;
    }
    if (text == QLatin1String("ДатаВремя")) {
        description.type = QStringLiteral("xs:dateTime");
        description.dateFraction = QStringLiteral("DateTime");
        return description;
    }
    if (text == QLatin1String("Время")) {
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
            }
        }
    }

    const QString objectName = object.name;
    const bool hasTable = containsListTable(document->items, objectName);
    const bool isDocument = sameTitle(kindTag, QStringLiteral("Document"));
    const bool isCatalog = sameTitle(kindTag, QStringLiteral("Catalog"));
    document->runtimeListCommands = isDocument && hasTable;
    document->runtimeDocumentCommands = isDocument && !hasTable;
    document->runtimeCatalogCommands = isCatalog && !hasTable;
    document->runtimeCatalogListCommands = isCatalog && hasTable;
}

} // namespace v8reader
