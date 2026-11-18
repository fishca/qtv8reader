#include "cli/CommandCommon.h"

namespace v8reader::cli {

int commandExtract(const QString &path, const QString &objectQuery,
                   const QString &output)
{
    if (output.isEmpty()) {
        err() << QStringLiteral("Ошибка: укажите каталог через -o\n");
        return 1;
    }

    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    QString error;
    MetadataObject *object = findObject(reader.configuration(), objectQuery, &error);
    if (!object) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    ExtractionOptions options;
    ExtractionReport report;
    Extractor extractor(reader);
    if (!extractor.extractObject(*object, output, options, &report, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    out() << QStringLiteral("Объект «%1» сохранён в %2\n")
                 .arg(object->displayName(), report.outputDirectory);
    printReport(report);
    return 0;
}


} // namespace v8reader::cli
