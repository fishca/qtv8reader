#ifndef QTV8READER_CLI_COMMANDUTILS_H
#define QTV8READER_CLI_COMMANDUTILS_H

#include "core/Configuration.h"
#include "core/ConfigurationReader.h"
#include "core/Extractor.h"

#include <QByteArray>
#include <QString>

namespace v8reader::cli {

MetadataObject *findObject(Configuration &configuration, const QString &query,
                           QString *error);
bool loadConfiguration(const QString &path, ConfigurationReader *reader);
bool writeOutputFile(const QString &path, const QByteArray &data, QString *error);
void printReport(const ExtractionReport &report);

} // namespace v8reader::cli

#endif // QTV8READER_CLI_COMMANDUTILS_H
