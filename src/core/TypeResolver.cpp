#include "core/TypeResolver.h"

#include "core/MetadataTextUtils.h"

namespace v8reader {

using namespace detail;

void TypeResolver::clear()
{
    m_typeIndex.clear();
}

void TypeResolver::registerObject(const MetadataObject &object)
{
    const QString name = object.displayName();
    if (name.isEmpty())
        return;

    for (const QString &typeId : object.typeIds) {
        if (typeId.isEmpty() || m_typeIndex.contains(typeId))
            continue;
        TypeObject entry;
        entry.typeTitle = object.typeTitle;
        entry.name = name;
        m_typeIndex.insert(typeId, entry);
    }
}

QString TypeResolver::typeNameForGuid(const QString &typeGuid) const
{
    return referenceTypeName(typeGuid);
}

QString TypeResolver::referenceTypeName(const QString &typeGuid) const
{
    const auto it = m_typeIndex.constFind(typeGuid);
    if (it == m_typeIndex.constEnd())
        return QString();

    static const QHash<QString, QString> prefixes = {
        {QStringLiteral("Справочник"), QStringLiteral("СправочникСсылка")},
        {QStringLiteral("Документ"), QStringLiteral("ДокументСсылка")},
        {QStringLiteral("Перечисление"), QStringLiteral("ПеречислениеСсылка")},
        {QStringLiteral("План обмена"), QStringLiteral("ПланОбменаСсылка")},
        {QStringLiteral("План счетов"), QStringLiteral("ПланСчетовСсылка")},
        {QStringLiteral("План видов характеристик"),
         QStringLiteral("ПланВидовХарактеристикСсылка")},
        {QStringLiteral("План видов расчета"), QStringLiteral("ПланВидовРасчетаСсылка")},
        {QStringLiteral("Бизнес-процесс"), QStringLiteral("БизнесПроцессСсылка")},
        {QStringLiteral("Задача"), QStringLiteral("ЗадачаСсылка")},
        {QStringLiteral("Внешний источник данных"),
         QStringLiteral("ВнешнийИсточникДанныхСсылка")},
        {QStringLiteral("Определяемый тип"), QStringLiteral("ОпределяемыйТип")},
    };

    const QString prefix = prefixes.value(it->typeTitle);
    if (prefix.isEmpty())
        return QStringLiteral("%1.%2").arg(it->typeTitle, it->name);
    return QStringLiteral("%1.%2").arg(prefix, it->name);
}

QStringList TypeResolver::collectTypeIds(const V8TextNode &objectRoot,
                                         const V8TextNode *descriptor) const
{
    QStringList ids;
    if (!descriptor)
        return ids;

    const V8TextNode *current = descriptor;
    const V8TextNode *parent = nullptr;
    int index = -1;
    while (findParentNode(objectRoot, current, &parent, &index)) {
        for (int i = 0; i < index; ++i) {
            const V8TextNode &child = parent->children.at(i);
            if (!isGuidNode(child))
                continue;
            const QString guid = child.normalizedGuid();
            if (!guid.isEmpty() && !ids.contains(guid))
                ids.append(guid);
        }
        current = parent;
    }
    return ids;
}

QString TypeResolver::typeTextFromNode(const V8TextNode &typeNode) const
{
    if (!typeNode.isList() || typeNode.children.isEmpty())
        return QString();

    const QString code = typeNode.children.at(0).isScalar()
                             ? typeNode.children.at(0).value.trimmed()
                             : QString();

    if (code == QStringLiteral("S")) {
        qint64 length = 0;
        if (integerAt(typeNode, 1, &length) && length > 0)
            return QStringLiteral("Строка(%1)").arg(length);
        return QStringLiteral("Строка");
    }
    if (code == QStringLiteral("N")) {
        qint64 precision = 0;
        qint64 scale = 0;
        integerAt(typeNode, 1, &precision);
        integerAt(typeNode, 2, &scale);
        if (precision > 0 && scale > 0)
            return QStringLiteral("Число(%1,%2)").arg(precision).arg(scale);
        if (precision > 0)
            return QStringLiteral("Число(%1)").arg(precision);
        return QStringLiteral("Число");
    }
    if (code == QStringLiteral("B"))
        return QStringLiteral("Булево");
    if (code == QStringLiteral("D"))
        return QStringLiteral("Дата");
    if (code == QStringLiteral("#")) {
        const QString typeGuid = typeNode.children.size() > 1
                                     ? guidOfNode(typeNode.children.at(1))
                                     : QString();
        const QString name = referenceTypeName(typeGuid);
        return name.isEmpty() ? QStringLiteral("Ссылка") : name;
    }
    return QString();
}

QString TypeResolver::typeTextForDescriptor(const V8TextNode &root,
                                            const V8TextNode *descriptor) const
{
    if (!descriptor)
        return QString();

    const V8TextNode *holder = nullptr;
    int descriptorIndex = -1;
    if (!findParentNode(root, descriptor, &holder, &descriptorIndex))
        return QString();

    for (int i = descriptorIndex + 1; i < holder->children.size(); ++i) {
        const V8TextNode &candidate = holder->children.at(i);
        if (!candidate.isList() || candidate.children.size() < 2)
            continue;
        if (!candidate.children.at(0).isScalar()
            || candidate.children.at(0).value.trimmed() != QStringLiteral("Pattern"))
            continue;

        QStringList parts;
        for (int k = 1; k < candidate.children.size(); ++k) {
            const QString text = typeTextFromNode(candidate.children.at(k));
            if (!text.isEmpty() && !parts.contains(text))
                parts.append(text);
        }
        return parts.join(QStringLiteral(", "));
    }
    return QString();
}

QString TypeResolver::typeTextForItem(const V8TextNode &item) const
{
    return typeTextForDescriptor(item, findFirstDescriptor(item, 0));
}

} // namespace v8reader
