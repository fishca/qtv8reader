#include "core/SectionBuilder.h"

#include <algorithm>
#include <functional>

namespace v8reader {

using namespace detail;

namespace {

constexpr int kMaxTableScanDepth = 10;

// Проверка заполнения реквизита лежит в записи его свойств (узел с видом 27):
// тринадцатое поле этой записи — 0 (не проверять), 1 (выдавать ошибку) или
// 2 (выводить предупреждение). Запись ищется в элементе таблицы состава,
// потому что у справочников она вложена в обёртку. Сверено с выгрузкой
// конфигурации в XML: по 136 реквизитам документа «АвансовыйОтчет»
// (28 реквизитов и их колонки), справочника «Банки» и регистра
// сведений «КурсыВалют» расхождений нет.
int fillCheckingFromEntry(const V8TextNode &node)
{
    if (node.isList() && node.children.size() > 13) {
        const V8TextNode &recordKind = node.children.at(0);
        const V8TextNode &head = node.children.at(1);
        if (recordKind.isScalar() && recordKind.scalar().trimmed() == QLatin1String("27")
            && head.isList() && head.children.size() >= 2) {
            const V8TextNode &headKind = head.children.at(0);
            const V8TextNode &descriptor = head.children.at(1);
            const bool descriptorKind = descriptor.isList()
                && descriptor.children.size() >= 3
                && descriptor.children.at(0).isScalar()
                && (descriptor.children.at(0).scalar().trimmed() == QLatin1String("2")
                    || descriptor.children.at(0).scalar().trimmed() == QLatin1String("3"))
                && descriptor.children.at(2).isString();
            if (headKind.isScalar() && headKind.scalar().trimmed() == QLatin1String("2")
                && descriptorKind) {
                const V8TextNode &value = node.children.at(13);
                if (value.isInteger()) {
                    const qint64 code = value.toInteger();
                    if (code >= 0 && code <= 2)
                        return static_cast<int>(code);
                }
            }
        }
    }
    for (const V8TextNode &child : node.children) {
        const int found = fillCheckingFromEntry(child);
        if (found >= 0)
            return found;
    }
    return -1;
}

bool hasFillChecking(MetadataSectionKind kind)
{
    return kind == MetadataSectionKind::Attributes
        || kind == MetadataSectionKind::TabularAttributes
        || kind == MetadataSectionKind::Dimensions
        || kind == MetadataSectionKind::Resources;
}

int sectionRank(MetadataSectionKind kind)
{
    switch (kind) {
    case MetadataSectionKind::Dimensions:
        return 0;
    case MetadataSectionKind::Resources:
        return 1;
    case MetadataSectionKind::Attributes:
        return 2;
    case MetadataSectionKind::AccountingFlags:
        return 3;
    case MetadataSectionKind::SubcontoFlags:
        return 4;
    case MetadataSectionKind::Values:
        return 5;
    case MetadataSectionKind::JournalColumns:
        return 6;
    case MetadataSectionKind::TabularSections:
        return 7;
    case MetadataSectionKind::Forms:
        return 8;
    case MetadataSectionKind::Commands:
        return 9;
    case MetadataSectionKind::Templates:
        return 10;
    case MetadataSectionKind::TabularAttributes:
        return 11;
    case MetadataSectionKind::Columns:
        return 12;
    }
    return 13;
}

} // namespace

SectionBuilder::SectionBuilder(Context context)
    : m_context(std::move(context))
{
}

bool SectionBuilder::hasSection(const QVector<MetadataSection> &sections,
                                MetadataSectionKind kind)
{
    for (const MetadataSection &section : sections) {
        if (section.kind == kind)
            return true;
    }
    return false;
}

void SectionBuilder::sortSections(QVector<MetadataSection> *sections)
{
    std::stable_sort(sections->begin(), sections->end(),
                     [](const MetadataSection &left, const MetadataSection &right) {
                         return sectionRank(left.kind) < sectionRank(right.kind);
                     });
}

QVector<MetadataSection> SectionBuilder::build(const V8TextNode &objectRoot,
                                               const QString &categoryGuid) const
{
    QVector<MetadataSection> sections = byTableGuids(objectRoot, kMaxTableScanDepth);

    const MetadataLayout *layout = MetadataCatalog::layoutForCategory(categoryGuid);
    if (layout) {
        for (const SectionLayout &spec : layout->sections) {
            if (spec.tableIndex < 0 || hasSection(sections, spec.kind))
                continue;

            const V8TextNode *table = objectRoot.child(spec.tableIndex);
            if (!table || !table->isList() || table->children.size() < 2)
                continue;

            const QString tableGuid = scalarAt(*table, 0);
            MetadataSectionKind knownKind = spec.kind;
            if (!tableGuid.isEmpty()
                && MetadataCatalog::sectionKindForTableGuid(tableGuid, &knownKind)
                && knownKind != spec.kind) {
                continue;
            }

            MetadataSection section;
            if (fromTable(*table, spec.kind, &section))
                sections.append(section);
        }

        for (const SectionLayout &spec : layout->sections) {
            if (hasSection(sections, spec.kind))
                continue;

            MetadataSection empty;
            empty.kind = spec.kind;
            empty.guid = MetadataCatalog::sectionGuid(spec.kind);
            empty.title = MetadataCatalog::sectionTitle(spec.kind);
            sections.append(empty);
        }
    }

    sortSections(&sections);
    return sections;
}

QVector<MetadataSection> SectionBuilder::byTableGuids(const V8TextNode &objectRoot,
                                                       int maxDepth) const
{
    QVector<MetadataSection> sections;

    std::function<void(const V8TextNode &, int)> scan = [&](const V8TextNode &node,
                                                           int depth) {
        if (depth > maxDepth)
            return;

        if (node.isList() && node.children.size() >= 2) {
            const QString tableGuid = guidOfNode(node.children.at(0));
            MetadataSectionKind kind = MetadataSectionKind::Attributes;
            if (!tableGuid.isEmpty()
                && MetadataCatalog::sectionKindForTableGuid(tableGuid, &kind)
                && kind != MetadataSectionKind::TabularAttributes
                && !hasSection(sections, kind)) {
                MetadataSection section;
                if (fromTable(node, kind, &section))
                    sections.append(section);
            }
        }

        for (const V8TextNode &child : node.children)
            scan(child, depth + 1);
    };
    scan(objectRoot, 0);

    sortSections(&sections);
    return sections;
}

QVector<MetadataSection> SectionBuilder::fallback(const V8TextNode &objectRoot) const
{
    return byTableGuids(objectRoot, kMaxTableScanDepth);
}

bool SectionBuilder::fromTable(const V8TextNode &table, MetadataSectionKind kind,
                               MetadataSection *section) const
{
    if (!section)
        return false;

    *section = MetadataSection();
    section->kind = kind;
    section->guid = MetadataCatalog::sectionGuid(kind);
    section->title = MetadataCatalog::sectionTitle(kind);

    if (!table.isList() || table.children.size() < 2)
        return false;

    qint64 count = 0;
    const qint64 available = static_cast<qint64>(table.children.size()) - 2;
    if (!integerAt(table, 1, &count))
        return false;
    if (count < 0 || count > available)
        count = available;

    const bool referenceSection = kind == MetadataSectionKind::Forms
        || kind == MetadataSectionKind::Templates;

    for (qint64 i = 0; i < count; ++i) {
        const V8TextNode &item = table.children.at(static_cast<int>(i) + 2);

        MetadataEntry entry;
        if (referenceSection) {
            const QString guid = isGuidNode(item) ? item.normalizedGuid()
                                                  : findFirstGuidDeep(item);
            if (guid.isEmpty())
                continue;
            entry = referenceEntry(guid, kind);
        } else {
            entry = entryFromItem(item, kind);
        }

        if (entry.name.isEmpty() && entry.guid.isEmpty())
            continue;

        if (kind == MetadataSectionKind::TabularSections)
            attachTabularAttributes(item, &entry);

        section->entries.append(entry);
    }

    return true;
}

MetadataEntry SectionBuilder::entryFromItem(const V8TextNode &item,
                                            MetadataSectionKind kind) const
{
    MetadataEntry entry;
    entry.typeTitle = MetadataCatalog::sectionEntryTitle(kind);

    const DescriptorInfo descriptor = descriptorFor(item);
    if (descriptor.found) {
        entry.guid = descriptor.guid;
        entry.name = descriptor.name;
        entry.synonym = descriptor.synonym;
        entry.comment = descriptor.comment;
        if (m_context.typeTextForItem)
            entry.typeText = m_context.typeTextForItem(item);
        // Проверка заполнения читается только у тех таблиц состава, где
        // раскладка записи сверена с выгрузкой в XML: у табличных частей,
        // форм, макетов и команд запись устроена иначе.
        if (hasFillChecking(kind))
            entry.fillChecking = fillCheckingFromEntry(item);
        return entry;
    }

    const QString guid = isGuidNode(item) ? item.normalizedGuid()
                                          : findFirstGuidDeep(item);
    if (!guid.isEmpty() && m_context.referenceName) {
        const ReferenceName name = m_context.referenceName(guid);
        entry.guid = guid;
        entry.name = name.name;
        entry.synonym = name.synonym;
    }
    return entry;
}

MetadataEntry SectionBuilder::referenceEntry(const QString &guid,
                                             MetadataSectionKind kind) const
{
    MetadataEntry entry;
    entry.guid = guid;
    entry.typeTitle = MetadataCatalog::sectionEntryTitle(kind);

    if (m_context.referenceName) {
        const ReferenceName name = m_context.referenceName(guid);
        entry.name = name.name;
        entry.synonym = name.synonym;
    }
    return entry;
}

void SectionBuilder::attachTabularAttributes(const V8TextNode &item,
                                             MetadataEntry *entry) const
{
    if (!entry)
        return;

    const V8TextNode *table = nullptr;
    std::function<void(const V8TextNode &, int)> scan = [&](const V8TextNode &node,
                                                           int depth) {
        if (table || depth > kMaxTableScanDepth)
            return;
        if (node.isList() && node.children.size() >= 2) {
            const QString tableGuid = guidOfNode(node.children.at(0));
            MetadataSectionKind kind = MetadataSectionKind::Attributes;
            if (!tableGuid.isEmpty()
                && MetadataCatalog::sectionKindForTableGuid(tableGuid, &kind)
                && kind == MetadataSectionKind::TabularAttributes) {
                table = &node;
                return;
            }
        }
        for (const V8TextNode &child : node.children) {
            scan(child, depth + 1);
            if (table)
                return;
        }
    };
    scan(item, 0);

    if (!table)
        return;

    MetadataSection section;
    if (fromTable(*table, MetadataSectionKind::TabularAttributes, &section))
        entry->children = section.entries;
}

} // namespace v8reader
