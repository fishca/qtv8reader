#include "cli/CommandCommon.h"

namespace v8reader::cli {

int commandExtractAll(const QString &path, const QString &output)
{
    if (output.isEmpty()) {
        err() << QStringLiteral("Ошибка: укажите каталог через -o\n");
        return 1;
    }

    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    ExtractionOptions options;
    ExtractionReport report;
    QString error;
    Extractor extractor(reader);
    if (!extractor.extractAll(output, options, &report, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    out() << QStringLiteral("Конфигурация сохранена в %1\n").arg(report.outputDirectory);
    printReport(report);
    return 0;
}


} // namespace v8reader::cli
