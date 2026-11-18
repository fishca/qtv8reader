#ifndef QTV8READER_METADATAICONS_H
#define QTV8READER_METADATAICONS_H

#include "core/MetadataCatalog.h"

#include <QIcon>
#include <QString>

namespace v8reader {

// Иконки metadata-icons: кэш, сопоставление категорий и разделов.
class MetadataIcons
{
public:
    static QIcon icon(const QString &name);
    static QString categoryIconName(const QString &guid);
    static QString sectionIconName(MetadataSectionKind kind);
};

} // namespace v8reader

#endif // QTV8READER_METADATAICONS_H
