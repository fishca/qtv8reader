#ifndef QTV8READER_CONFIGURATION_H
#define QTV8READER_CONFIGURATION_H

#include "core/MetadataCatalog.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace v8reader {

// Что находится в открытом файле 1С.
enum class ConfigurationKind {
    Unknown,
    Configuration,      // .cf — конфигурация
    Extension,          // .cfe — расширение конфигурации
    ExternalProcessing, // .epf — внешняя обработка
    ExternalReport      // .erf — внешний отчет
};

struct MetadataEntry
{
    QString guid;
    QString name;
    QString synonym;
    QString comment;
    QString typeTitle;
    QVector<MetadataEntry> children;

    QString displayName() const;
};

struct MetadataSection
{
    MetadataSectionKind kind = MetadataSectionKind::Attributes;
    QString guid;
    QString title;
    QVector<MetadataEntry> entries;

    bool isEmpty() const { return entries.isEmpty(); }
};

struct MetadataObject
{
    QString guid;
    QString name;
    QString synonym;
    QString comment;
    QString categoryGuid;
    QString categoryTitle;
    QString typeTitle;
    QString elementName;
    bool elementFound = false;
    bool descriptorFound = false;
    bool rootObject = false;
    bool sectionsLoaded = false;
    // Вложенные подсистемы (заполняется по требованию для объектов-подсистем).
    bool childrenLoaded = false;
    QStringList childGuids;
    QVector<MetadataSection> sections;

    QString displayName() const;
    bool hasSections() const;
    const MetadataSection *section(MetadataSectionKind kind) const;
    QStringList sectionSummaries() const;
};

struct MetadataCategory
{
    QString guid;
    QString title;
    QString typeTitle;
    QVector<MetadataObject> objects;

    bool isEmpty() const { return objects.isEmpty(); }
};

struct Configuration
{
    QString name;
    QString synonym;
    QString comment;
    QString version;
    QString compatibilityMode;
    QString configGuid;
    QString sourcePath;
    QString formatLabel;
    QString kindTitle;
    ConfigurationKind kind = ConfigurationKind::Unknown;
    int formatVersion = 0;
    qint64 fileSize = 0;
    QVector<MetadataCategory> categories;
    QStringList warnings;

    QString displayName() const;
    int objectCount() const;
    const MetadataCategory *category(const QString &guid) const;
    MetadataCategory *category(const QString &guid);
    const MetadataObject *object(const QString &guid) const;
    MetadataObject *object(const QString &guid);
    QStringList categoryTitles() const;
};

} // namespace v8reader

#endif // QTV8READER_CONFIGURATION_H
