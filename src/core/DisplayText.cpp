#include "core/DisplayText.h"

namespace v8reader {

QString eventDisplayName(const QString &eventName)
{
    const int separator = eventName.indexOf(QLatin1Char('_'));
    if (separator <= 0)
        return eventName;
    const QString prefix = eventName.left(separator);
    for (const QChar ch : prefix) {
        if (!ch.isLetter() || ch.unicode() > 127)
            return eventName;
    }
    return eventName.mid(separator + 1);
}

QString compositionTypeLabel(const MetadataEntry &entry)
{
    if (entry.typeText.isEmpty())
        return entry.typeTitle;
    return QStringLiteral("%1 · %2").arg(entry.typeTitle, entry.typeText);
}

QString dateTimeText(const QDateTime &dateTime)
{
    return dateTime.isValid()
        ? dateTime.toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"))
        : QStringLiteral("—");
}

} // namespace v8reader
