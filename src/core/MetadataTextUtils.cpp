#include "core/MetadataTextUtils.h"

#include "core/MetadataCatalog.h"

namespace v8reader::detail {

QString scalarAt(const V8TextNode &node, int index)
{
    const V8TextNode *child = node.child(index);
    return child && child->isScalar() ? child->value.trimmed() : QString();
}

bool integerAt(const V8TextNode &node, int index, qint64 *value)
{
    const V8TextNode *child = node.child(index);
    if (!child || child->isList())
        return false;

    bool ok = false;
    const qint64 parsed = child->toInteger(&ok);
    if (!ok)
        return false;
    if (value)
        *value = parsed;
    return true;
}

bool isGuidNode(const V8TextNode &node)
{
    return node.isScalar() && V8Text::isGuid(node.value);
}

QString guidOfNode(const V8TextNode &node)
{
    return isGuidNode(node) ? node.normalizedGuid() : QString();
}

QString findFirstGuidDeep(const V8TextNode &node, int depth)
{
    if (depth > kMaxSearchDepth)
        return QString();

    if (node.isScalar()) {
        const QString guid = V8Text::normalizeGuid(node.value);
        if (!guid.isEmpty() && guid != MetadataCatalog::zeroGuid())
            return guid;
        return QString();
    }

    for (const V8TextNode &child : node.children) {
        const QString guid = findFirstGuidDeep(child, depth + 1);
        if (!guid.isEmpty())
            return guid;
    }
    return QString();
}

QString pickLocalized(const V8TextNode &node)
{
    if (node.isScalar())
        return node.value;

    if (node.children.size() >= 3) {
        for (int i = 1; i + 1 < node.children.size(); i += 2) {
            const V8TextNode &language = node.children.at(i);
            const V8TextNode &text = node.children.at(i + 1);
            if (language.isScalar()
                && language.value.compare(QStringLiteral("ru"), Qt::CaseInsensitive) == 0
                && text.isScalar()) {
                return text.value;
            }
        }
    }

    if (node.children.size() >= 3 && node.children.at(2).isScalar())
        return node.children.at(2).value;
    if (node.children.size() >= 2 && node.children.at(1).isScalar())
        return node.children.at(1).value;
    return QString();
}

QString synonymOf(const V8TextNode &descriptor)
{
    if (!descriptor.isList() || descriptor.children.size() <= 3)
        return QString();
    return pickLocalized(descriptor.children.at(3));
}

bool descriptorGuid(const V8TextNode &node, QString *guid)
{
    if (!node.isList() || node.children.size() < 3)
        return false;

    const V8TextNode &kind = node.children.at(0);
    if (kind.isList())
        return false;

    bool kindOk = false;
    const qint64 kindValue = kind.toInteger(&kindOk);
    if (!kindOk || kindValue < 0 || kindValue > 20)
        return false;

    const V8TextNode &identity = node.children.at(1);
    if (!identity.isList() || identity.children.size() != 3)
        return false;
    if (!identity.children.at(0).isScalar() || !identity.children.at(1).isScalar())
        return false;
    bool firstOk = false;
    bool secondOk = false;
    const qint64 identityFlag = identity.children.at(0).toInteger(&firstOk);
    const qint64 identitySubFlag = identity.children.at(1).toInteger(&secondOk);
    if (!firstOk || !secondOk)
        return false;
    if (identityFlag < 0 || identityFlag > 2 || identitySubFlag < 0
        || identitySubFlag > 2) {
        return false;
    }

    const V8TextNode &guidNode = identity.children.at(2);
    if (!isGuidNode(guidNode))
        return false;

    if (node.children.at(2).isList())
        return false;

    const QString normalized = guidNode.normalizedGuid();
    if (normalized.isEmpty() || normalized == MetadataCatalog::zeroGuid())
        return false;

    if (guid)
        *guid = normalized;
    return true;
}

const V8TextNode *findDescriptorByGuid(const V8TextNode &node, const QString &guid,
                                       int depth)
{
    if (depth > kMaxSearchDepth)
        return nullptr;

    QString nodeGuid;
    if (descriptorGuid(node, &nodeGuid) && nodeGuid == guid)
        return &node;

    for (const V8TextNode &child : node.children) {
        if (const V8TextNode *found = findDescriptorByGuid(child, guid, depth + 1))
            return found;
    }
    return nullptr;
}

const V8TextNode *findFirstDescriptor(const V8TextNode &node, int depth)
{
    if (depth > kMaxSearchDepth)
        return nullptr;

    QString nodeGuid;
    if (descriptorGuid(node, &nodeGuid) && !scalarAt(node, 2).isEmpty())
        return &node;

    for (const V8TextNode &child : node.children) {
        if (const V8TextNode *found = findFirstDescriptor(child, depth + 1))
            return found;
    }
    return nullptr;
}

bool findParentNode(const V8TextNode &node, const V8TextNode *target,
                    const V8TextNode **parent, int *index)
{
    for (int i = 0; i < node.children.size(); ++i) {
        const V8TextNode &child = node.children.at(i);
        if (&child == target) {
            if (parent)
                *parent = &node;
            if (index)
                *index = i;
            return true;
        }
        if (findParentNode(child, target, parent, index))
            return true;
    }
    return false;
}

DescriptorInfo descriptorFor(const V8TextNode &root)
{
    DescriptorInfo info;
    const V8TextNode *descriptor = findFirstDescriptor(root, 0);
    if (!descriptor)
        return info;

    info.found = true;
    QString guid;
    descriptorGuid(*descriptor, &guid);
    info.guid = guid;
    info.name = scalarAt(*descriptor, 2);
    info.synonym = synonymOf(*descriptor);
    info.comment = scalarAt(*descriptor, 4);
    return info;
}

} // namespace v8reader::detail
