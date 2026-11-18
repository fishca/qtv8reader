#include "cli/CommandCommon.h"

namespace v8reader::cli {

void printMxlSummary(const QString &title, const mxl::Document &document)
{
    const mxl::PrintSettings &settings = document.printSettings;
    QString printInfo = QStringLiteral("по умолчанию (A4, книжная, 100%)");
    if (settings.valid) {
        const QString pageSize = settings.pageSize.isEmpty()
            ? QStringLiteral("A4") : settings.pageSize;
        const QString orientation = settings.orientation == 2
            ? QStringLiteral("альбомная") : QStringLiteral("книжная");
        printInfo = QStringLiteral("%1, %2, масштаб %3%, поля %4/%5/%6/%7 мм")
            .arg(pageSize, orientation)
            .arg(settings.scale > 0 ? settings.scale : 100)
            .arg(settings.topMarginMm)
            .arg(settings.bottomMarginMm)
            .arg(settings.leftMarginMm)
            .arg(settings.rightMarginMm);
    }
    QStringList areaNames;
    for (const mxl::NamedArea &area : document.namedAreas)
        areaNames << area.name;

    out() << QStringLiteral("%1: %2 строк, %3 колонок, %4 форматов, %5 шрифтов, "
                            "%6 объединений, %7 цветов, %8 линий\n")
                 .arg(title)
                 .arg(document.rowCount())
                 .arg(document.maxColumn() + 1)
                 .arg(document.formats.size())
                 .arg(document.fonts.size())
                 .arg(document.merges.size())
                 .arg(document.colors.size())
                 .arg(document.lines.size());
    out() << QStringLiteral("  именованные области (%1): %2\n")
                 .arg(document.namedAreas.size())
                 .arg(areaNames.isEmpty() ? QStringLiteral("нет")
                                          : areaNames.join(QStringLiteral(", ")));
    out() << QStringLiteral("  печать: %1\n").arg(printInfo);
}

bool loadTemplatePayload(ConfigurationReader &reader, const MetadataEntry &entry,
                         QByteArray *payload, QString *error)
{
    const V8Container &container = reader.container();
    const V8Element *element = nullptr;
    if (!entry.guid.isEmpty())
        element = container.find(entry.guid + QStringLiteral(".0"));
    if (!element)
        element = container.find(entry.guid);
    if (!element && !entry.name.isEmpty())
        element = container.find(entry.name);
    if (!element && !entry.guid.isEmpty()) {
        const QString prefix = entry.guid + QLatin1Char('.');
        for (const V8Element &candidate : container.elements()) {
            if (candidate.name == entry.guid || candidate.name.startsWith(prefix)) {
                element = &candidate;
                break;
            }
        }
    }
    if (!element) {
        if (error)
            *error = QStringLiteral("макет не найден в контейнере");
        return false;
    }
    return container.elementPayload(*element, payload, nullptr, error);
}

int commandMxl(const QString &path, const QString &objectQuery, const QString &output)
{
    const QFileInfo info(path);
    if (info.suffix().compare(QStringLiteral("mxl"), Qt::CaseInsensitive) == 0) {
        mxl::Document document;
        QString error;
        if (!MxlReader::parseFile(path, &document, &error)) {
            err() << QStringLiteral("Ошибка: %1\n").arg(error);
            return 1;
        }
        printMxlSummary(info.fileName(), document);
        return 0;
    }

    ConfigurationReader reader;
    QString error;
    if (!reader.open(path, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    MetadataObject *object = findObject(reader.configuration(), objectQuery, &error);
    if (!object) {
        err() << QStringLiteral("Ошибка: %1 (укажите объект с макетами)\n").arg(error);
        return 1;
    }

    reader.loadSections(*object);
    const MetadataSection *section = object->section(MetadataSectionKind::Templates);
    if (!section || section->entries.isEmpty()) {
        err() << QStringLiteral("У объекта «%1» нет макетов\n").arg(object->displayName());
        return 1;
    }

    int parsedCount = 0;
    for (const MetadataEntry &entry : section->entries) {
        QByteArray payload;
        QString loadError;
        if (!loadTemplatePayload(reader, entry, &payload, &loadError)) {
            out() << QStringLiteral("%1: не прочитан (%2)\n")
                         .arg(entry.displayName(), loadError);
            continue;
        }
        if (!MxlReader::looksLikeMxl(payload)) {
            out() << QStringLiteral("%1: не табличный документ MXL\n")
                         .arg(entry.displayName());
            continue;
        }
        mxl::Document document;
        if (!MxlReader::parseBytes(payload, &document, &error)) {
            out() << QStringLiteral("%1: ошибка разбора (%2)\n")
                         .arg(entry.displayName(), error);
            continue;
        }
        printMxlSummary(entry.displayName(), document);
        ++parsedCount;

        if (!output.isEmpty()) {
            QDir directory(output);
            if (!directory.exists() && !QDir().mkpath(output)) {
                err() << QStringLiteral("Ошибка: не удалось создать каталог «%1»\n").arg(output);
                return 1;
            }
            QString safeName = entry.displayName();
            safeName.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")),
                             QStringLiteral("_"));
            const QString filePath = directory.filePath(safeName + QStringLiteral(".mxl"));
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly))
                file.write(payload);
        }
    }

    out() << QStringLiteral("Разобрано макетов: %1 из %2\n")
                 .arg(parsedCount)
                 .arg(section->entries.size());
    return parsedCount > 0 ? 0 : 1;
}

} // namespace v8reader::cli
