#ifndef QTV8READER_EXTRACTIONCONTROLLER_H
#define QTV8READER_EXTRACTIONCONTROLLER_H

#include "core/Configuration.h"
#include "core/Extractor.h"

#include <QString>

namespace v8reader {

class ConfigurationReader;

// Выгрузка объекта/конфигурации и форматирование отчёта.
// Диалоги выбора каталога остаются в MainWindow.
class ExtractionController
{
public:
    bool extractObject(ConfigurationReader &reader, MetadataObject &object,
                       const QString &directory, ExtractionReport *report,
                       QString *error) const;
    bool extractAll(ConfigurationReader &reader, const QString &directory,
                    ExtractionReport *report, QString *error) const;

    static QString objectResultMessage(const MetadataObject &object,
                                       const ExtractionReport &report);
    static QString allResultMessage(const ExtractionReport &report);

private:
    static QString warningsText(const ExtractionReport &report);
};

} // namespace v8reader

#endif // QTV8READER_EXTRACTIONCONTROLLER_H
