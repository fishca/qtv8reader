#ifndef QTV8READER_SECTIONBUILDER_H
#define QTV8READER_SECTIONBUILDER_H

#include "core/Configuration.h"
#include "core/MetadataCatalog.h"
#include "core/MetadataTextUtils.h"
#include "core/V8Text.h"

#include <QString>
#include <QVector>

#include <functional>

namespace v8reader {

// Построение состава объекта метаданных: реквизиты, формы, макеты,
// табличные части и т. п. Не зависит от ConfigurationReader.
class SectionBuilder
{
public:
    struct Context
    {
        std::function<QString(const V8TextNode &)> typeTextForItem;
        std::function<detail::ReferenceName(const QString &)> referenceName;
    };

    explicit SectionBuilder(Context context = {});

    QVector<MetadataSection> build(const V8TextNode &objectRoot,
                                   const QString &categoryGuid) const;
    QVector<MetadataSection> byTableGuids(const V8TextNode &objectRoot,
                                          int maxDepth) const;
    QVector<MetadataSection> fallback(const V8TextNode &objectRoot) const;

private:
    bool fromTable(const V8TextNode &table, MetadataSectionKind kind,
                   MetadataSection *section) const;
    MetadataEntry entryFromItem(const V8TextNode &item,
                                MetadataSectionKind kind) const;
    MetadataEntry referenceEntry(const QString &guid,
                                 MetadataSectionKind kind) const;
    void attachTabularAttributes(const V8TextNode &item,
                                 MetadataEntry *entry) const;

    static bool hasSection(const QVector<MetadataSection> &sections,
                           MetadataSectionKind kind);
    static void sortSections(QVector<MetadataSection> *sections);

    Context m_context;
};

} // namespace v8reader

#endif // QTV8READER_SECTIONBUILDER_H
