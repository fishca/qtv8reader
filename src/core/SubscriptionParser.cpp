#include "core/SubscriptionParser.h"

#include "core/MetadataTextUtils.h"

namespace v8reader {

SubscriptionInfo SubscriptionParser::parse(const V8TextNode &objectRoot,
                                           const V8TextNode *descriptor)
{
    SubscriptionInfo info;
    if (!descriptor)
        return info;

    const V8TextNode *holder = nullptr;
    int descriptorIndex = -1;
    if (!detail::findParentNode(objectRoot, descriptor, &holder, &descriptorIndex))
        return info;

    for (int i = descriptorIndex + 1; i < holder->children.size(); ++i) {
        const V8TextNode &node = holder->children.at(i);
        if (node.isList()) {
            if (node.children.size() < 2 || !node.children.at(0).isScalar()
                || node.children.at(0).value.trimmed() != QStringLiteral("Pattern")) {
                continue;
            }
            for (int k = 1; k < node.children.size(); ++k) {
                const V8TextNode &typeNode = node.children.at(k);
                if (!typeNode.isList() || typeNode.children.size() < 2
                    || !typeNode.children.at(0).isScalar()
                    || typeNode.children.at(0).value.trimmed() != QStringLiteral("#")) {
                    continue;
                }
                const QString typeGuid = detail::guidOfNode(typeNode.children.at(1));
                if (!typeGuid.isEmpty() && !info.sourceTypeGuids.contains(typeGuid))
                    info.sourceTypeGuids.append(typeGuid);
            }
            continue;
        }

        if (detail::isGuidNode(node)) {
            if (info.handlerModuleGuid.isEmpty())
                info.handlerModuleGuid = node.normalizedGuid();
            continue;
        }

        const QString value = node.value.trimmed();
        if (value.isEmpty())
            continue;
        if (info.eventName.isEmpty())
            info.eventName = value;
        else if (info.handlerProcedure.isEmpty())
            info.handlerProcedure = value;
    }
    return info;
}

} // namespace v8reader
