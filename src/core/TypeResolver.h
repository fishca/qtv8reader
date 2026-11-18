#ifndef QTV8READER_TYPERESOLVER_H
#define QTV8READER_TYPERESOLVER_H

#include "core/Configuration.h"
#include "core/V8Text.h"

#include <QHash>
#include <QString>
#include <QStringList>

namespace v8reader {

// Расшифровка внутренних идентификаторов типов 1С и текстов типов
// («Строка(50)», «СправочникСсылка.Валюты» и т. п.).
class TypeResolver
{
public:
    void clear();
    void registerObject(const MetadataObject &object);

    QString typeNameForGuid(const QString &typeGuid) const;
    QStringList collectTypeIds(const V8TextNode &objectRoot,
                               const V8TextNode *descriptor) const;
    QString typeTextFromNode(const V8TextNode &typeNode) const;
    QString typeTextForDescriptor(const V8TextNode &root,
                                  const V8TextNode *descriptor) const;
    QString typeTextForItem(const V8TextNode &item) const;

private:
    struct TypeObject
    {
        QString typeTitle;
        QString name;
    };

    QString referenceTypeName(const QString &typeGuid) const;

    QHash<QString, TypeObject> m_typeIndex;
};

} // namespace v8reader

#endif // QTV8READER_TYPERESOLVER_H
