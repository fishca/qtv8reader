#include "cli/CommandUtils.h"

#include "cli/Output.h"
#include "core/V8Text.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace v8reader::cli {

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

    QVector<MetadataObject *> exactMatches;
    for (MetadataCategory &category : configuration.categories) {
        for (MetadataObject &object : category.objects) {
            if (object.name.compare(query, Qt::CaseInsensitive) == 0
                || object.displayName().compare(query, Qt::CaseInsensitive) == 0
                || object.synonym.compare(query, Qt::CaseInsensitive) == 0) {
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
                if (object.name.contains(query, Qt::CaseInsensitive)
                    || object.synonym.contains(query, Qt::CaseInsensitive)) {
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
    for (const MetadataObject *object : matches)
        names.append(QStringLiteral("%1 [%2]").arg(object->displayName(), object->guid));
    if (error) {
        *error = QStringLiteral("Уточните объект, найдено несколько совпадений: %1")
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
