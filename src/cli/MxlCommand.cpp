#include "cli/CommandCommon.h"

#include "core/MxlHtmlExporter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace v8reader::cli {
namespace {

// Разобранный макет: имя для выбора и текст сводки, документ и исходные байты
// (для -o — сохранить .mxl как есть).
struct TemplateDocument
{
    QString name;
    mxl::Document document;
    QByteArray payload;
};

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

QString templateNameList(const QVector<TemplateDocument> &templates)
{
    QStringList names;
    for (const TemplateDocument &item : templates)
        names << item.name;
    return names.join(QStringLiteral(", "));
}

QJsonObject fileInfoJson(const QString &path)
{
    const QFileInfo info(path);
    QJsonObject file;
    file.insert(QStringLiteral("path"), info.absoluteFilePath());
    file.insert(QStringLiteral("name"), info.fileName());
    file.insert(QStringLiteral("size"), static_cast<double>(info.size()));
    file.insert(QStringLiteral("modified"),
                info.lastModified().toString(Qt::ISODate));
    return file;
}

void printMxlJson(const QString &path, const QVector<TemplateDocument> &templates,
                  int selected, const QStringList &warnings)
{
    QJsonObject root;
    root.insert(QStringLiteral("schema"), 1);
    root.insert(QStringLiteral("file"), fileInfoJson(path));
    root.insert(QStringLiteral("selected"),
                selected >= 0 ? QJsonValue(templates.at(selected).name)
                              : QJsonValue(QJsonValue::Null));

    QJsonArray array;
    for (const TemplateDocument &item : templates) {
        QJsonObject summary = mxl::MxlHtmlExporter::summarize(item.document);
        summary.insert(QStringLiteral("name"), item.name);
        array.append(summary);
    }
    root.insert(QStringLiteral("templates"), array);

    if (!warnings.isEmpty()) {
        QJsonArray messages;
        for (const QString &warning : warnings)
            messages.append(warning);
        root.insert(QStringLiteral("warnings"), messages);
    }

    out() << QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact))
          << QLatin1Char('\n');
}

// Читает макеты контейнера. Ошибки чтения отдельного макета не фатальны:
// они попадают в warnings, как и раньше в текстовом выводе.
bool collectContainerTemplates(const QString &path, const QString &objectQuery,
                               QVector<TemplateDocument> *templates,
                               QStringList *warnings, int *totalEntries,
                               QString *error)
{
    ConfigurationReader reader;
    if (!reader.open(path, error))
        return false;

    MetadataObject *object = findObject(reader.configuration(), objectQuery, error);
    if (!object) {
        if (error)
            *error += QStringLiteral(" (укажите объект с макетами)");
        return false;
    }

    reader.loadSections(*object);
    const MetadataSection *section = object->section(MetadataSectionKind::Templates);
    if (!section || section->entries.isEmpty()) {
        if (error) {
            *error = QStringLiteral("У объекта «%1» нет макетов")
                         .arg(object->displayName());
        }
        return false;
    }
    *totalEntries = section->entries.size();

    for (const MetadataEntry &entry : section->entries) {
        QByteArray payload;
        QString loadError;
        if (!loadTemplatePayload(reader, entry, &payload, &loadError)) {
            warnings->append(QStringLiteral("%1: не прочитан (%2)")
                                 .arg(entry.displayName(), loadError));
            continue;
        }
        if (!MxlReader::looksLikeMxl(payload)) {
            warnings->append(QStringLiteral("%1: не табличный документ MXL")
                                 .arg(entry.displayName()));
            continue;
        }
        mxl::Document document;
        QString parseError;
        if (!MxlReader::parseBytes(payload, &document, &parseError)) {
            warnings->append(QStringLiteral("%1: ошибка разбора (%2)")
                                 .arg(entry.displayName(), parseError));
            continue;
        }
        templates->append({entry.displayName(), document, payload});
    }
    return true;
}

bool readMxlFile(const QString &path, TemplateDocument *document, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Не удалось открыть файл «%1»: %2")
                         .arg(path, file.errorString());
        return false;
    }
    const QByteArray payload = file.readAll();
    mxl::Document parsed;
    if (!MxlReader::parseBytes(payload, &parsed, error))
        return false;
    document->name = QFileInfo(path).completeBaseName();
    document->document = parsed;
    document->payload = payload;
    return true;
}

} // namespace

int commandMxl(const QString &path, const QString &objectQuery, const QString &output,
               const MxlCommandOptions &options)
{
    const QFileInfo info(path);
    const bool directFile =
        info.suffix().compare(QStringLiteral("mxl"), Qt::CaseInsensitive) == 0;

    QVector<TemplateDocument> templates;
    QStringList warnings;
    int totalEntries = 0;
    QString error;

    if (directFile) {
        TemplateDocument document;
        if (!readMxlFile(path, &document, &error)) {
            err() << QStringLiteral("Ошибка: %1\n").arg(error);
            return 1;
        }
        templates.append(document);
        totalEntries = 1;
    } else if (!collectContainerTemplates(path, objectQuery, &templates, &warnings,
                                          &totalEntries, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    const bool textMode =
        options.htmlOutput.isEmpty() && !options.json;
    if (textMode) {
        for (const QString &warning : warnings)
            out() << warning << QLatin1Char('\n');
    }

    int selected = -1;
    if (!options.templateName.isEmpty()) {
        for (int index = 0; index < templates.size(); ++index) {
            if (templates.at(index).name.compare(options.templateName,
                                                 Qt::CaseInsensitive) == 0) {
                selected = index;
                break;
            }
        }
        if (selected < 0) {
            err() << QStringLiteral("Ошибка: макет «%1» не найден")
                         .arg(options.templateName);
            if (!templates.isEmpty()) {
                err() << QStringLiteral(" (есть: %1)").arg(templateNameList(templates));
            }
            err() << QLatin1Char('\n');
            return 1;
        }
    } else if (templates.size() == 1) {
        selected = 0;
    }

    const bool htmlToStdout = options.htmlOutput == QStringLiteral("-");
    if (options.json && htmlToStdout) {
        err() << QStringLiteral("Ошибка: --json и --html - несовместимы\n");
        return 1;
    }

    if (!options.htmlOutput.isEmpty()) {
        if (selected < 0) {
            if (templates.isEmpty()) {
                err() << QStringLiteral("Ошибка: макетов MXL не найдено\n");
                return 1;
            }
            err() << QStringLiteral("Ошибка: у объекта несколько макетов, "
                                    "укажите --template (есть: %1)\n")
                         .arg(templateNameList(templates));
            return 1;
        }

        const TemplateDocument &chosen = templates.at(selected);
        mxl::HtmlExportOptions htmlOptions;
        htmlOptions.title = chosen.name;
        const QString html = mxl::MxlHtmlExporter::toHtml(chosen.document, htmlOptions);

        if (htmlToStdout) {
            out() << html;
        } else {
            QString writeError;
            if (!writeOutputFile(options.htmlOutput, html.toUtf8(), &writeError)) {
                err() << QStringLiteral("Ошибка: %1\n").arg(writeError);
                return 1;
            }
            out() << QStringLiteral("Макет «%1»: HTML сохранён в «%2»\n")
                         .arg(chosen.name, options.htmlOutput);
        }

        if (options.json && !htmlToStdout)
            printMxlJson(path, templates, selected, warnings);
        return 0;
    }

    if (options.json) {
        printMxlJson(path, templates, selected, warnings);
        return 0;
    }

    QVector<int> toPrint;
    if (!options.templateName.isEmpty()) {
        toPrint.append(selected);
    } else {
        for (int index = 0; index < templates.size(); ++index)
            toPrint.append(index);
    }

    for (const int index : toPrint) {
        const TemplateDocument &item = templates.at(index);
        printMxlSummary(item.name, item.document);

        if (output.isEmpty())
            continue;
        QDir directory(output);
        if (!directory.exists() && !QDir().mkpath(output)) {
            err() << QStringLiteral("Ошибка: не удалось создать каталог «%1»\n")
                         .arg(output);
            return 1;
        }
        QString safeName = item.name;
        safeName.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")),
                         QStringLiteral("_"));
        const QString filePath = directory.filePath(safeName + QStringLiteral(".mxl"));
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly))
            file.write(item.payload);
    }

    out() << QStringLiteral("Разобрано макетов: %1 из %2\n")
                 .arg(templates.size())
                 .arg(totalEntries);
    return templates.isEmpty() ? 1 : 0;
}

} // namespace v8reader::cli
