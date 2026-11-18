#include "gui/ExtractionController.h"

#include "core/ConfigurationReader.h"

#include <algorithm>

namespace v8reader {

bool ExtractionController::extractObject(ConfigurationReader &reader,
                                         MetadataObject &object,
                                         const QString &directory,
                                         ExtractionReport *report,
                                         QString *error) const
{
    Extractor extractor(reader);
    return extractor.extractObject(object, directory, ExtractionOptions(), report,
                                   error);
}

bool ExtractionController::extractAll(ConfigurationReader &reader,
                                      const QString &directory,
                                      ExtractionReport *report,
                                      QString *error) const
{
    Extractor extractor(reader);
    return extractor.extractAll(directory, ExtractionOptions(), report, error);
}

QString ExtractionController::warningsText(const ExtractionReport &report)
{
    if (report.warnings.isEmpty())
        return QString();

    QString text = QStringLiteral("\n\nПредупреждения:\n");
    const int count = std::min<int>(report.warnings.size(), 10);
    for (int i = 0; i < count; ++i)
        text += QStringLiteral("• %1\n").arg(report.warnings.at(i));
    if (report.warnings.size() > count)
        text += QStringLiteral("…\n");
    return text;
}

QString ExtractionController::objectResultMessage(const MetadataObject &object,
                                                  const ExtractionReport &report)
{
    QString message = QStringLiteral(
        "Объект «%1» сохранён.\n\n"
        "Файлов: %2\nМодулей: %3\nФорм: %4\nМакетов: %5\n\n"
        "Каталог:\n%6")
        .arg(object.displayName())
        .arg(report.files.size())
        .arg(report.moduleCount)
        .arg(report.formCount)
        .arg(report.templateCount)
        .arg(report.outputDirectory);
    return message + warningsText(report);
}

QString ExtractionController::allResultMessage(const ExtractionReport &report)
{
    QString message = QStringLiteral(
        "Сохранено файлов: %1\nМодулей: %2\nФорм: %3\nМакетов: %4\n\nКаталог:\n%5")
        .arg(report.files.size())
        .arg(report.moduleCount)
        .arg(report.formCount)
        .arg(report.templateCount)
        .arg(report.outputDirectory);
    return message + warningsText(report);
}

} // namespace v8reader
