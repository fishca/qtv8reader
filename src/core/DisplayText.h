#ifndef QTV8READER_DISPLAYTEXT_H
#define QTV8READER_DISPLAYTEXT_H

#include "core/Configuration.h"

#include <QDateTime>
#include <QString>

namespace v8reader {

// «Posting_ОбработкаПроведения» → «ОбработкаПроведения».
QString eventDisplayName(const QString &eventName);

// «Реквизит · Строка(50)»; при пустом typeText — только typeTitle.
QString compositionTypeLabel(const MetadataEntry &entry);

// Единый формат даты для GUI и CLI; «—» для невалидной даты.
QString dateTimeText(const QDateTime &dateTime);

} // namespace v8reader

#endif // QTV8READER_DISPLAYTEXT_H
