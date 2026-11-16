#ifndef QTV8READER_CONFIGURATIONREADER_H
#define QTV8READER_CONFIGURATIONREADER_H

#include "core/Configuration.h"
#include "core/V8Container.h"
#include "core/V8Text.h"

#include <QHash>

#include <memory>
#include <vector>

namespace v8reader {

// Читает контейнер 1C:V8 и строит модель метаданных конфигурации,
// расширения, внешней обработки или внешнего отчета.
class ConfigurationReader
{
public:
    ConfigurationReader();

    bool open(const QString &filePath, QString *error = nullptr);
    void clear();
    bool isOpen() const { return m_container.isOpen(); }

    const Configuration &configuration() const { return m_configuration; }
    Configuration &configuration() { return m_configuration; }

    const V8Container &container() const { return m_container; }
    V8Container &container() { return m_container; }

    bool readElementText(const QString &elementName, QString *text,
                         QString *error = nullptr) const;
    bool readPayloadText(const QByteArray &payload, QString *text,
                         QString *error = nullptr) const;
    bool readObjectText(const MetadataObject &object, QString *text,
                        QString *error = nullptr) const;

    // Разбирает состав объекта (реквизиты, формы, макеты...) по требованию.
    // Результат кэшируется в самом объекте.
    bool loadSections(MetadataObject &object) const;

    // Иерархия подсистем: заполняет список вложенных подсистем объекта.
    bool loadSubsystemChildren(MetadataObject &subsystem);

    // Объект вложенной подсистемы по GUID (создаётся по требованию).
    MetadataObject *subsystemObject(const QString &guid);

    // Уже разобранная вложенная подсистема по GUID (без чтения контейнера).
    MetadataObject *cachedSubsystem(const QString &guid) const;

    // Имя и синоним объекта, на который ссылается элемент контейнера
    // (используется для форм, макетов и прочих ссылочных таблиц состава).
    bool readReferenceName(const QString &guid, QString *name,
                           QString *synonym = nullptr,
                           QString *error = nullptr) const;

private:
    struct ReferenceName
    {
        QString name;
        QString synonym;
    };

    struct DescriptorInfo
    {
        bool found = false;
        QString guid;
        QString name;
        QString synonym;
        QString comment;
    };

    bool readRootGuid(QString *rootGuid);
    bool parseElementTree(const QString &elementName, V8TextNode *root) const;
    void buildConfigurationMetadata(const V8TextNode &configRoot);
    void buildExternalObject(const V8TextNode &objectRoot, const QString &rootGuid);
    MetadataObject buildObject(const QString &guid,
                               const MetadataCategoryInfo &category) const;
    MetadataObject objectFromRoot(const QString &guid,
                                  const MetadataCategoryInfo &category,
                                  const V8TextNode &objectRoot) const;
    QVector<MetadataSection> buildSections(const V8TextNode &objectRoot,
                                           const QString &categoryGuid) const;
    QVector<MetadataSection> buildSectionsByLayout(const V8TextNode &objectRoot,
                                                   const MetadataLayout &layout) const;
    QVector<MetadataSection> buildSectionsByTableGuids(const V8TextNode &objectRoot,
                                                       int maxDepth) const;
    QVector<MetadataSection> buildSectionsFallback(const V8TextNode &objectRoot) const;
    bool buildSectionFromTable(const V8TextNode &table, MetadataSectionKind kind,
                               MetadataSection *section) const;
    MetadataEntry buildEntryFromItem(const V8TextNode &item,
                                     MetadataSectionKind kind) const;
    MetadataEntry buildReferenceEntry(const QString &guid,
                                      MetadataSectionKind kind) const;
    void attachTabularAttributes(const V8TextNode &item, MetadataEntry *entry) const;
    const V8TextNode *findDescriptor(const V8TextNode &root, const QString &guid) const;
    DescriptorInfo descriptorFor(const V8TextNode &root) const;
    ReferenceName referenceName(const QString &guid) const;
    void addWarning(const QString &message);

    V8Container m_container;
    Configuration m_configuration;
    mutable QHash<QString, ReferenceName> m_referenceNames;

    // Кэш вложенных подсистем (объекты живут до закрытия файла).
    std::vector<std::unique_ptr<MetadataObject>> m_subsystemObjects;
    QHash<QString, MetadataObject *> m_subsystemIndex;
};

} // namespace v8reader

#endif // QTV8READER_CONFIGURATIONREADER_H
