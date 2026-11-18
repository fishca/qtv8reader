#include "cli/CommandCommon.h"

namespace v8reader::cli {

void printCompositionEntries(const QVector<MetadataEntry> &entries, int depth)
{
    const QString indent(depth * 2, QLatin1Char(' '));
    for (const MetadataEntry &entry : entries) {
        out() << QStringLiteral("%1%2").arg(indent, entry.displayName());
        if (!entry.synonym.isEmpty() && entry.synonym != entry.name)
            out() << QStringLiteral(" / %1").arg(entry.synonym);
        if (!entry.guid.isEmpty())
            out() << QStringLiteral(" [%1]").arg(entry.guid);
        if (!entry.typeText.isEmpty())
            out() << QStringLiteral(" · %1").arg(entry.typeText);
        if (!entry.comment.isEmpty())
            out() << QStringLiteral(" — %1").arg(entry.comment);
        out() << QLatin1Char('\n');
        if (!entry.children.isEmpty())
            printCompositionEntries(entry.children, depth + 1);
    }
}

int commandComposition(const QString &path, const QString &objectQuery)
{
    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    QString error;
    MetadataObject *object = findObject(reader.configuration(), objectQuery, &error);
    if (!object) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    reader.loadSections(*object);

    out() << QStringLiteral("%1 — %2\n").arg(object->displayName(), object->typeTitle);
    if (!object->synonym.isEmpty())
        out() << QStringLiteral("Синоним: %1\n").arg(object->synonym);
    if (!object->typeText.isEmpty())
        out() << QStringLiteral("Тип: %1\n").arg(object->typeText);
    out() << QStringLiteral("GUID: %1\n").arg(object->guid);

    if (!object->eventName.isEmpty() || !object->sourceTypeGuids.isEmpty()) {
        QStringList sources;
        for (const QString &typeGuid : object->sourceTypeGuids) {
            const QString name = reader.typeNameForGuid(typeGuid);
            sources << (name.isEmpty() ? typeGuid : name);
        }
        if (!sources.isEmpty())
            out() << QStringLiteral("Источник: %1\n")
                         .arg(sources.join(QStringLiteral(", ")));
        if (!object->eventName.isEmpty())
            out() << QStringLiteral("Событие: %1\n")
                         .arg(eventDisplayName(object->eventName));
        if (!object->handlerProcedure.isEmpty()) {
            QString moduleName;
            reader.readReferenceName(object->handlerModuleGuid, &moduleName,
                                     nullptr, nullptr);
            out() << QStringLiteral("Обработчик: %1\n")
                         .arg(moduleName.isEmpty()
                                  ? object->handlerProcedure
                                  : QStringLiteral("%1.%2")
                                        .arg(moduleName, object->handlerProcedure));
        }
    }

    int total = 0;
    for (const MetadataSection &section : object->sections) {
        out() << QStringLiteral("\n[%1] — %2\n")
                     .arg(section.title)
                     .arg(section.entries.size());
        printCompositionEntries(section.entries, 1);
        total += section.entries.size();
    }

    if (total == 0)
        out() << QStringLiteral("\nСостав объекта не найден\n");
    return 0;
}


} // namespace v8reader::cli
