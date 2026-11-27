#include "cli/CommandCommon.h"

#include "cli/ViewCommand.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace v8reader::cli {

namespace {

// Версия схемы JSON-вывода команды view. Панель плагина DSH сравнивает её со
// своей ожидаемой и отказывается работать с неизвестной версией.
constexpr int kViewSchema = 1;

QString kindSlug(ConfigurationKind kind)
{
    switch (kind) {
    case ConfigurationKind::Configuration:
        return QStringLiteral("configuration");
    case ConfigurationKind::Extension:
        return QStringLiteral("extension");
    case ConfigurationKind::ExternalProcessing:
        return QStringLiteral("externalProcessing");
    case ConfigurationKind::ExternalReport:
        return QStringLiteral("externalReport");
    case ConfigurationKind::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

// Стабильные ключи разделов состава: имена перечисления меняются, имена в JSON
// не должны меняться вслед за ними.
QString sectionSlug(MetadataSectionKind kind)
{
    switch (kind) {
    case MetadataSectionKind::Templates:
        return QStringLiteral("templates");
    case MetadataSectionKind::Commands:
        return QStringLiteral("commands");
    case MetadataSectionKind::TabularSections:
        return QStringLiteral("tabularSections");
    case MetadataSectionKind::Attributes:
        return QStringLiteral("attributes");
    case MetadataSectionKind::Forms:
        return QStringLiteral("forms");
    case MetadataSectionKind::TabularAttributes:
        return QStringLiteral("tabularAttributes");
    case MetadataSectionKind::Dimensions:
        return QStringLiteral("dimensions");
    case MetadataSectionKind::Resources:
        return QStringLiteral("resources");
    case MetadataSectionKind::AccountingFlags:
        return QStringLiteral("accountingFlags");
    case MetadataSectionKind::SubcontoFlags:
        return QStringLiteral("subcontoFlags");
    case MetadataSectionKind::Values:
        return QStringLiteral("values");
    case MetadataSectionKind::Columns:
        return QStringLiteral("columns");
    case MetadataSectionKind::JournalColumns:
        return QStringLiteral("journalColumns");
    }
    return QStringLiteral("unknown");
}

QJsonObject entryJson(const MetadataEntry &entry)
{
    QJsonObject json;
    json.insert(QStringLiteral("guid"), entry.guid);
    json.insert(QStringLiteral("name"), entry.name);
    json.insert(QStringLiteral("synonym"), entry.synonym);
    json.insert(QStringLiteral("comment"), entry.comment);
    json.insert(QStringLiteral("typeTitle"), entry.typeTitle);
    json.insert(QStringLiteral("typeText"), entry.typeText);
    if (entry.fillChecking >= 0)
        json.insert(QStringLiteral("fillChecking"), entry.fillChecking);
    if (!entry.children.isEmpty()) {
        QJsonArray children;
        for (const MetadataEntry &child : entry.children)
            children.append(entryJson(child));
        json.insert(QStringLiteral("children"), children);
    }
    return json;
}

QJsonObject objectJson(const MetadataObject &object, bool withSections)
{
    QJsonObject json;
    json.insert(QStringLiteral("guid"), object.guid);
    json.insert(QStringLiteral("name"), object.name);
    json.insert(QStringLiteral("synonym"), object.synonym);
    json.insert(QStringLiteral("comment"), object.comment);
    json.insert(QStringLiteral("typeTitle"), object.typeTitle);
    json.insert(QStringLiteral("typeText"), object.typeText);
    json.insert(QStringLiteral("categoryTitle"), object.categoryTitle);
    json.insert(QStringLiteral("elementFound"), object.elementFound);
    if (!withSections)
        return json;

    QJsonArray sections;
    for (const MetadataSection &section : object.sections) {
        QJsonObject sectionJson;
        sectionJson.insert(QStringLiteral("kind"), sectionSlug(section.kind));
        sectionJson.insert(QStringLiteral("title"), section.title);
        sectionJson.insert(QStringLiteral("guid"), section.guid);
        QJsonArray entries;
        for (const MetadataEntry &entry : section.entries)
            entries.append(entryJson(entry));
        sectionJson.insert(QStringLiteral("entries"), entries);
        sections.append(sectionJson);
    }
    json.insert(QStringLiteral("sections"), sections);
    return json;
}

QJsonObject moduleJson(const ModuleInfo &module)
{
    QJsonObject json;
    json.insert(QStringLiteral("name"), module.name);
    json.insert(QStringLiteral("elementName"), module.elementName);
    json.insert(QStringLiteral("source"), module.source);
    json.insert(QStringLiteral("size"), static_cast<double>(module.text.toUtf8().size()));
    return json;
}

QJsonArray moduleListJson(const QVector<ModuleInfo> &modules, bool withText)
{
    QJsonArray list;
    for (const ModuleInfo &module : modules) {
        QJsonObject json = moduleJson(module);
        if (withText)
            json.insert(QStringLiteral("text"), module.text);
        list.append(json);
    }
    return list;
}

// Модуль по имени: сначала точное совпадение имени или элемента контейнера,
// затем вхождение в имя (пользователь панели кликает по подписи модуля).
const ModuleInfo *findModule(const QVector<ModuleInfo> &modules, const QString &query)
{
    for (const ModuleInfo &module : modules) {
        if (module.name.compare(query, Qt::CaseInsensitive) == 0
            || module.elementName.compare(query, Qt::CaseInsensitive) == 0)
            return &module;
    }
    for (const ModuleInfo &module : modules) {
        if (module.name.contains(query, Qt::CaseInsensitive))
            return &module;
    }
    return nullptr;
}

QJsonObject fileJson(const QString &path, const Configuration &configuration)
{
    const QFileInfo info(path);
    QJsonObject json;
    json.insert(QStringLiteral("path"), info.absoluteFilePath());
    json.insert(QStringLiteral("size"), static_cast<double>(info.size()));
    json.insert(QStringLiteral("modified"),
                info.lastModified().toUTC().toString(Qt::ISODate));
    json.insert(QStringLiteral("kind"), kindSlug(configuration.kind));
    json.insert(QStringLiteral("kindTitle"), configuration.kindTitle);
    json.insert(QStringLiteral("name"), configuration.name);
    json.insert(QStringLiteral("synonym"), configuration.synonym);
    json.insert(QStringLiteral("comment"), configuration.comment);
    json.insert(QStringLiteral("version"), configuration.version);
    json.insert(QStringLiteral("compatibilityMode"), configuration.compatibilityMode);
    json.insert(QStringLiteral("configGuid"), configuration.configGuid);
    json.insert(QStringLiteral("formatLabel"), configuration.formatLabel);
    json.insert(QStringLiteral("formatVersion"), configuration.formatVersion);
    json.insert(QStringLiteral("objectCount"), configuration.objectCount());
    QJsonArray warnings;
    for (const QString &warning : configuration.warnings)
        warnings.append(warning);
    json.insert(QStringLiteral("warnings"), warnings);
    return json;
}

void printTextSummary(const QString &path, const Configuration &configuration,
                      const MetadataObject *object, const ModuleInfo *module)
{
    out() << QStringLiteral("Конфигурация: %1\n").arg(configuration.displayName());
    out() << QStringLiteral("Тип файла: %1 (%2)\n")
                 .arg(configuration.kindTitle, kindSlug(configuration.kind));
    if (!configuration.version.isEmpty())
        out() << QStringLiteral("Версия: %1\n").arg(configuration.version);
    out() << QStringLiteral("Файл: %1\n").arg(path);
    out() << QStringLiteral("Формат: %1\n").arg(configuration.formatLabel);
    out() << QStringLiteral("Объектов: %1\n").arg(configuration.objectCount());
    for (const MetadataCategory &category : configuration.categories) {
        out() << QStringLiteral("  %1: %2\n")
                     .arg(category.title)
                     .arg(category.objects.size());
    }
    if (object) {
        out() << QStringLiteral("\nОбъект: %1 (%2)\n")
                     .arg(object->displayName(), object->typeTitle);
        if (!object->sections.isEmpty()) {
            for (const MetadataSection &section : object->sections) {
                if (section.isEmpty())
                    continue;
                out() << QStringLiteral("  %1: %2\n")
                             .arg(section.title)
                             .arg(section.entries.size());
            }
        }
    }
    if (module) {
        out() << QStringLiteral("\nМодуль: %1 (%2) — %3 байт\n")
                     .arg(module->name, module->elementName)
                     .arg(module->text.toUtf8().size());
    }
}

} // namespace

int commandView(const QString &path, const QString &objectQuery,
                const QString &moduleName, bool json)
{
    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    const Configuration &configuration = reader.configuration();

    MetadataObject *object = nullptr;
    if (!objectQuery.trimmed().isEmpty()) {
        QString error;
        object = findObject(reader.configuration(), objectQuery, &error);
        if (!object) {
            err() << QStringLiteral("Ошибка: %1\n").arg(error);
            return 1;
        }
        reader.loadSections(*object);
    }

    // Модули: у объекта — его собственные, без объекта — корневые модули
    // конфигурации (то же, что делает команда modules).
    const ModuleExtractor extractor(reader.container());
    QVector<ModuleInfo> modules;
    if (object)
        modules = extractor.extractForObject(*object);
    else if (!moduleName.isEmpty())
        modules = reader.configurationModules();

    const ModuleInfo *module = nullptr;
    if (!moduleName.trimmed().isEmpty()) {
        module = findModule(modules, moduleName.trimmed());
        if (!module) {
            err() << QStringLiteral("Ошибка: модуль «%1» не найден%2\n")
                         .arg(moduleName,
                              object ? QStringLiteral(" у объекта «%1»")
                                           .arg(object->displayName())
                                     : QString());
            return 1;
        }
    }

    if (!json) {
        printTextSummary(path, configuration, object, module);
        return 0;
    }

    QJsonObject root;
    root.insert(QStringLiteral("schema"), kViewSchema);
    root.insert(QStringLiteral("appVersion"),
                QCoreApplication::applicationVersion());
    root.insert(QStringLiteral("file"), fileJson(path, configuration));

    QJsonArray categories;
    for (const MetadataCategory &category : configuration.categories) {
        QJsonObject categoryJson;
        categoryJson.insert(QStringLiteral("guid"), category.guid);
        categoryJson.insert(QStringLiteral("title"), category.title);
        categoryJson.insert(QStringLiteral("typeTitle"), category.typeTitle);
        categoryJson.insert(QStringLiteral("count"), category.objects.size());
        QJsonArray objects;
        for (const MetadataObject &categoryObject : category.objects)
            objects.append(objectJson(categoryObject, false));
        categoryJson.insert(QStringLiteral("objects"), objects);
        categories.append(categoryJson);
    }
    root.insert(QStringLiteral("categories"), categories);

    if (object) {
        QJsonObject objectResult = objectJson(*object, true);
        objectResult.insert(QStringLiteral("modules"), moduleListJson(modules, false));
        root.insert(QStringLiteral("object"), objectResult);
    } else {
        root.insert(QStringLiteral("object"), QJsonValue::Null);
    }

    if (module) {
        QJsonObject content;
        content.insert(QStringLiteral("kind"), QStringLiteral("module"));
        content.insert(QStringLiteral("name"), module->name);
        content.insert(QStringLiteral("elementName"), module->elementName);
        content.insert(QStringLiteral("text"), module->text);
        root.insert(QStringLiteral("content"), content);
    } else {
        root.insert(QStringLiteral("content"), QJsonValue::Null);
    }

    out() << QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact))
          << QLatin1Char('\n');
    return 0;
}

} // namespace v8reader::cli
