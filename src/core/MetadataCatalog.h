#ifndef QTV8READER_METADATACATALOG_H
#define QTV8READER_METADATACATALOG_H

#include <QString>
#include <QVector>

namespace v8reader {

// Описание категории объектов метаданных 1С.
struct MetadataCategoryInfo
{
    QString guid;
    QString title;
    QString typeTitle;
};

// Вид таблицы состава объекта метаданных.
enum class MetadataSectionKind {
    Templates,
    Commands,
    TabularSections,
    Attributes,
    Forms,
    TabularAttributes,
    Dimensions,
    Resources,
    AccountingFlags,
    SubcontoFlags,
    Values,
    Columns,
    JournalColumns
};

// Положение таблицы состава в корневом списке объекта метаданных 1С.
// Индекс отсчитывается от корневого списка текста объекта
// (0 — служебный признак, 1 — свойства объекта, 2 — количество таблиц,
// далее идут сами таблицы состава).
struct SectionLayout
{
    MetadataSectionKind kind = MetadataSectionKind::Attributes;
    int tableIndex = -1;
};

// Раскладка таблиц состава для конкретного типа объекта метаданных.
struct MetadataLayout
{
    QVector<SectionLayout> sections;
    // У журналов документов и подобных объектов табличных частей нет.
    bool hasTabularSections = false;

    bool isValid() const { return !sections.isEmpty(); }
};

class MetadataCatalog
{
public:
    static const QVector<MetadataCategoryInfo> &categories();
    static const MetadataCategoryInfo *category(const QString &guid);

    static QString sectionGuid(MetadataSectionKind kind);
    static QString sectionTitle(MetadataSectionKind kind);
    static QString sectionEntryTitle(MetadataSectionKind kind);
    static bool sectionKindForGuid(const QString &guid, MetadataSectionKind *kind);

    // Известные таблицы состава, которые могут встречаться в объекте.
    static QVector<MetadataSectionKind> sectionKinds();

    // Раскладка таблиц состава для категории (nullptr, если тип не описан).
    static const MetadataLayout *layoutForCategory(const QString &categoryGuid);

    // Известный GUID таблицы состава (для проверки структуры объекта).
    static bool sectionKindForTableGuid(const QString &tableGuid, MetadataSectionKind *kind);

    // Признак "объект с полным составом" (реквизиты, формы, макеты...).
    static bool hasComposition(const QString &categoryGuid);

    static QString zeroGuid();

private:
    MetadataCatalog() = delete;
};

} // namespace v8reader

#endif // QTV8READER_METADATACATALOG_H
