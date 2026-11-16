#ifndef QTV8READER_EXTRACTOR_H
#define QTV8READER_EXTRACTOR_H

#include "core/Configuration.h"
#include "core/ConfigurationReader.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace v8reader {

struct ExtractionFile
{
    QString path;
    qint64 size = 0;
};

struct ExtractionReport
{
    QString outputDirectory;
    QVector<ExtractionFile> files;
    QStringList warnings;
    int moduleCount = 0;
    int formCount = 0;
    int templateCount = 0;

    bool isEmpty() const { return files.isEmpty(); }
};

struct ExtractionOptions
{
    bool includeModules = true;
    bool includeForms = true;
    bool includeTemplates = true;
    bool includePayloads = true;
    bool includeRawMetadata = true;
};

// Сохраняет выбранный объект метаданных в каталог на диске.
class Extractor
{
public:
    explicit Extractor(ConfigurationReader &reader);

    bool extractObject(MetadataObject &object, const QString &outputRoot,
                       const ExtractionOptions &options, ExtractionReport *report,
                       QString *error = nullptr) const;

    bool extractAll(const QString &outputRoot, const ExtractionOptions &options,
                    ExtractionReport *report, QString *error = nullptr) const;

    static QString safeFileName(const QString &name);
    static QString safeDirectoryName(const QString &name);

private:
    bool writeFile(const QString &path, const QByteArray &data,
                   ExtractionReport *report, QString *error) const;
    bool writeTextFile(const QString &path, const QString &text,
                       ExtractionReport *report, QString *error) const;
    bool writeInfo(const MetadataObject &object, const QString &directory,
                   ExtractionReport *report, QString *error) const;
    bool writeComposition(const MetadataObject &object, const QString &directory,
                          ExtractionReport *report, QString *error) const;
    bool writeModules(const MetadataObject &object, const QString &directory,
                      const ExtractionOptions &options,
                      ExtractionReport *report, QString *error) const;
    bool writeNamedSection(const MetadataObject &object, MetadataSectionKind kind,
                           const QString &directory, const QString &subdirectory,
                           const ExtractionOptions &options,
                           ExtractionReport *report, QString *error) const;
    bool writeElementPayload(const QString &basePath, const QByteArray &payload,
                             ExtractionReport *report, QString *error) const;
    void addWarning(ExtractionReport *report, const QString &message) const;

    ConfigurationReader &m_reader;
};

} // namespace v8reader

#endif // QTV8READER_EXTRACTOR_H
