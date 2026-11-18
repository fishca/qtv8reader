#ifndef QTV8READER_SUBSCRIPTIONPARSER_H
#define QTV8READER_SUBSCRIPTIONPARSER_H

#include "core/V8Text.h"

#include <QString>
#include <QStringList>

namespace v8reader {

struct SubscriptionInfo
{
    QStringList sourceTypeGuids;
    QString eventName;
    QString handlerModuleGuid;
    QString handlerProcedure;
};

// Разбор подписки на событие: источник (Pattern), событие, модуль и процедура.
class SubscriptionParser
{
public:
    static SubscriptionInfo parse(const V8TextNode &objectRoot,
                                  const V8TextNode *descriptor);
};

} // namespace v8reader

#endif // QTV8READER_SUBSCRIPTIONPARSER_H
