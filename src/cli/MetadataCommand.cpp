#include "cli/CommandCommon.h"

namespace v8reader::cli {

int commandMetadata(const QString &path, const QString &objectQuery,
                    const QString &output)
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

    QString text;
    if (!reader.readObjectText(*object, &text, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    if (!output.isEmpty()) {
        if (!writeOutputFile(output, text.toUtf8(), &error)) {
            err() << QStringLiteral("Ошибка: %1\n").arg(error);
            return 1;
        }
        out() << QStringLiteral("Сохранено: %1 (%2 байт)\n")
                     .arg(output)
                     .arg(text.toUtf8().size());
        return 0;
    }

    out() << text;
    if (!text.endsWith('\n'))
        out() << QLatin1Char('\n');
    return 0;
}


} // namespace v8reader::cli
