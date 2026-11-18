#ifndef QTV8READER_METADATATEXTUTILS_H
#define QTV8READER_METADATATEXTUTILS_H

#include "core/V8Text.h"

#include <QString>
#include <QStringList>

namespace v8reader::detail {

constexpr int kMaxSearchDepth = 64;

struct DescriptorInfo
{
    bool found = false;
    QString guid;
    QString name;
    QString synonym;
    QString comment;
};

struct ReferenceName
{
    QString name;
    QString synonym;
};

QString scalarAt(const V8TextNode &node, int index);
bool integerAt(const V8TextNode &node, int index, qint64 *value);
bool isGuidNode(const V8TextNode &node);
QString guidOfNode(const V8TextNode &node);
QString findFirstGuidDeep(const V8TextNode &node, int depth = 0);
QString pickLocalized(const V8TextNode &node);
QString synonymOf(const V8TextNode &descriptor);
bool descriptorGuid(const V8TextNode &node, QString *guid);
const V8TextNode *findDescriptorByGuid(const V8TextNode &node, const QString &guid,
                                       int depth = 0);
const V8TextNode *findFirstDescriptor(const V8TextNode &node, int depth = 0);
bool findParentNode(const V8TextNode &node, const V8TextNode *target,
                    const V8TextNode **parent, int *index);
DescriptorInfo descriptorFor(const V8TextNode &root);

} // namespace v8reader::detail

#endif // QTV8READER_METADATATEXTUTILS_H
