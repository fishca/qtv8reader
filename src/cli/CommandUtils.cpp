#include "cli/CommandUtils.h"

#include "cli/Output.h"
#include "core/V8Text.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace v8reader::cli {

namespace {

// Вид объекта для уточнения имени: «Документ.АвансовыйОтчет». Принимаются и
// единственное число («Документ»), как в 1С, и название категории
// («Документы»).
bool matchesKindFilter(const MetadataObject &object, const QString &filter)
{
    return object.typeTitle.compare(filter, Qt::CaseInsensitive) == 0
        || object.categoryTitle.compare(filter, Qt::CaseInsensitive) == 0;
}

bool isKnownKind(const Configuration &configuration, const QString &prefix)
{
    for (const MetadataCategory &category : configuration.categories) {
        if (category.title.compare(prefix, Qt::CaseInsensitive) == 0
            || category.typeTitle.compare(prefix, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

} // namespace

MetadataObject *findObject(Configuration &configuration, const QString &query,
                           QString *error)
{
    if (query.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("Не указан объект метаданных");
        return nullptr;
    }

    const QString normalized = V8Text::normalizeGuid(query);
    if (!normalized.isEmpty()) {
        if (MetadataObject *object = configuration.object(normalized))
            return object;
    }

    // Уточнение вида: «Документ.АвансовыйОтчет» — имена объектов в больших
    // конфигурациях повторяются (например, «ВидыКонтактнойИнформации» есть и
    // у справочника, и у обработки), и без вида поиск просит уточнить запрос.
    QString nameQuery = query;
    QString kindQuery;
    const int dot = query.indexOf(QLatin1Char('.'));
    if (dot > 0) {
        const QString prefix = query.left(dot).trimmed();
        if (isKnownKind(configuration, prefix)) {
            kindQuery = prefix;
            nameQuery = query.mid(dot + 1).trimmed();
        }
    }

    const auto kindMatches = [&kindQuery](const MetadataObject &object) {
        return kindQuery.isEmpty() || matchesKindFilter(object, kindQuery);
    };

    QVector<MetadataObject *> exactMatches;
    for (MetadataCategory &category : configuration.categories) {
        for (MetadataObject &object : category.objects) {
            if (!kindMatches(object))
                continue;
            if (object.name.compare(nameQuery, Qt::CaseInsensitive) == 0
                || object.displayName().compare(nameQuery, Qt::CaseInsensitive) == 0
                || object.synonym.compare(nameQuery, Qt::CaseInsensitive) == 0) {
                exactMatches.append(&object);
            }
        }
    }

    if (exactMatches.size() == 1)
        return exactMatches.first();

    QVector<MetadataObject *> matches = exactMatches;
    if (matches.isEmpty()) {
        for (MetadataCategory &category : configuration.categories) {
            for (MetadataObject &object : category.objects) {
                if (!kindMatches(object))
                    continue;
                if (object.name.contains(nameQuery, Qt::CaseInsensitive)
                    || object.synonym.contains(nameQuery, Qt::CaseInsensitive)) {
                    matches.append(&object);
                }
            }
        }
    }

    if (matches.size() == 1)
        return matches.first();

    if (matches.isEmpty()) {
        if (error)
            *error = QStringLiteral("Объект «%1» не найден").arg(query);
        return nullptr;
    }

    QStringList names;
    for (const MetadataObject *object : matches) {
        const QString kind = object->typeTitle.isEmpty() ? object->categoryTitle
                                                         : object->typeTitle;
        names.append(QStringLiteral("%1 %2 [%3]")
                         .arg(kind, object->displayName(), object->guid));
    }
    if (error) {
        *error = QStringLiteral("Уточните объект, найдено несколько совпадений: %1. "
                                "Вид можно указать в запросе: «Документ.Имя».")
                     .arg(names.join(QStringLiteral("; ")));
    }
    return nullptr;
}

bool loadConfiguration(const QString &path, ConfigurationReader *reader)
{
    QString error;
    if (!reader->open(path, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return false;
    }
    return true;
}

bool writeOutputFile(const QString &path, const QByteArray &data, QString *error)
{
    QFileInfo info(path);
    if (!info.absolutePath().isEmpty() && !QDir().mkpath(info.absolutePath())) {
        if (error)
            *error = QStringLiteral("Не удалось создать каталог: %1").arg(info.absolutePath());
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("Не удалось открыть файл «%1»: %2")
                         .arg(path, file.errorString());
        return false;
    }
    if (file.write(data) != data.size()) {
        if (error)
            *error = QStringLiteral("Не удалось записать файл «%1»").arg(path);
        return false;
    }
    return true;
}

void printReport(const ExtractionReport &report)
{
    out() << QStringLiteral("Сохранено файлов: %1\n").arg(report.files.size());
    out() << QStringLiteral("Модулей: %1, форм: %2, макетов: %3\n")
                 .arg(report.moduleCount)
                 .arg(report.formCount)
                 .arg(report.templateCount);
    for (const ExtractionFile &file : report.files)
        out() << QStringLiteral("  %1 (%2 байт)\n").arg(file.path).arg(file.size);
    for (const QString &warning : report.warnings)
        out() << QStringLiteral("  ! %1\n").arg(warning);
}

} // namespace v8reader::cli
