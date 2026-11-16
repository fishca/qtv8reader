#include "core/Configuration.h"
#include "core/ConfigurationReader.h"
#include "core/Extractor.h"
#include "core/ModuleExtractor.h"
#include "core/MxlDocument.h"
#include "core/MxlReader.h"
#include "core/V8Container.h"
#include "core/V8Text.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <algorithm>

using namespace v8reader;

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    static bool initialized = false;
    if (!initialized) {
        stream.setEncoding(QStringConverter::Utf8);
        initialized = true;
    }
    return stream;
}

QTextStream &err()
{
    static QTextStream stream(stderr);
    static bool initialized = false;
    if (!initialized) {
        stream.setEncoding(QStringConverter::Utf8);
        initialized = true;
    }
    return stream;
}

void printHelp()
{
    out() << QStringLiteral(
        "v8reader_cli — просмотр конфигураций и внешних обработок 1С:Предприятие 8\n"
        "\n"
        "Использование:\n"
        "  v8reader_cli <команда> <файл> [объект] [параметры]\n"
        "\n"
        "Команды:\n"
        "  info <файл>                 сведения о конфигурации\n"
        "  list <файл>                 список категорий и объектов метаданных\n"
        "  tree <файл>                 список элементов контейнера\n"
        "  dump <файл> <элемент>       данные элемента контейнера\n"
        "  metadata <файл> <объект>    текст метаданных объекта (-o — в файл)\n"
        "  modules <файл> <объект>     тексты модулей объекта (-o — в файл)\n"
        "  composition <файл> <объект> состав объекта (реквизиты, формы, макеты)\n"
        "  mxl <файл> [объект]         табличный документ MXL: сводка (или файл .mxl)\n"
        "  extract <файл> <объект>     сохранить объект в каталог\n"
        "  extract-all <файл>          сохранить все объекты\n"
        "\n"
        "Параметры:\n"
        "  -o, --output <путь>         каталог или файл для сохранения\n"
        "      --raw                   для dump: не распаковывать данные\n"
        "  -h, --help                  показать эту справку\n"
        "\n"
        "Объект можно указать по GUID, имени или синониму.\n");
}

bool isProbablyText(const QByteArray &data)
{
    if (data.isEmpty())
        return false;
    if (data.startsWith("\xef\xbb\xbf")
        || data.startsWith("\xff\xfe")
        || data.startsWith("\xfe\xff")) {
        return true;
    }

    const int sampleSize = static_cast<int>(std::min<qsizetype>(data.size(), 8192));
    int zeros = 0;
    int controls = 0;
    int printable = 0;
    for (int i = 0; i < sampleSize; ++i) {
        const auto byte = static_cast<unsigned char>(data.at(i));
        if (byte == 0)
            ++zeros;
        else if (byte < 0x09 || (byte > 0x0d && byte < 0x20) || byte == 0x7f)
            ++controls;
        else if (byte >= 0x20)
            ++printable;
    }
    return zeros * 20 <= sampleSize
        && controls * 20 <= sampleSize
        && printable * 10 >= sampleSize * 9;
}

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
    for (const MetadataObject *object : matches) {
        names.append(QStringLiteral("%1 [%2]").arg(object->displayName(), object->guid));
    }
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

QString formatDateTime(const QDateTime &dateTime)
{
    return dateTime.isValid()
        ? dateTime.toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"))
        : QStringLiteral("—");
}

int commandInfo(const QString &path)
{
    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    const Configuration &configuration = reader.configuration();
    out() << QStringLiteral("Конфигурация: %1\n").arg(configuration.displayName());
    out() << QStringLiteral("Тип файла: %1\n").arg(configuration.kindTitle);
    if (!configuration.name.isEmpty())
        out() << QStringLiteral("Имя: %1\n").arg(configuration.name);
    if (!configuration.synonym.isEmpty())
        out() << QStringLiteral("Синоним: %1\n").arg(configuration.synonym);
    if (!configuration.comment.isEmpty())
        out() << QStringLiteral("Комментарий: %1\n").arg(configuration.comment);
    if (!configuration.version.isEmpty())
        out() << QStringLiteral("Версия: %1\n").arg(configuration.version);
    out() << QStringLiteral("Формат: %1\n").arg(configuration.formatLabel);
    out() << QStringLiteral("Файл: %1\n").arg(configuration.sourcePath);
    out() << QStringLiteral("Размер: %1 байт\n").arg(configuration.fileSize);
    out() << QStringLiteral("GUID конфигурации: %1\n").arg(configuration.configGuid);
    out() << QStringLiteral("Категорий: %1\n").arg(configuration.categories.size());
    out() << QStringLiteral("Объектов: %1\n").arg(configuration.objectCount());

    for (const MetadataCategory &category : configuration.categories) {
        out() << QStringLiteral("  %1: %2\n")
                     .arg(category.title)
                     .arg(category.objects.size());
    }

    if (!configuration.warnings.isEmpty()) {
        out() << QStringLiteral("\nПредупреждения:\n");
        for (const QString &warning : configuration.warnings)
            out() << QStringLiteral("  - %1\n").arg(warning);
    }

    return 0;
}

int commandList(const QString &path)
{
    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    const Configuration &configuration = reader.configuration();
    out() << QStringLiteral("%1 (%2), объектов: %3\n\n")
                 .arg(configuration.displayName(), configuration.formatLabel)
                 .arg(configuration.objectCount());

    for (const MetadataCategory &category : configuration.categories) {
        out() << QStringLiteral("[%1] %2 объектов\n")
                     .arg(category.title)
                     .arg(category.objects.size());
        for (const MetadataObject &object : category.objects) {
            out() << QStringLiteral("  %1").arg(object.displayName());
            if (!object.synonym.isEmpty())
                out() << QStringLiteral(" / %1").arg(object.synonym);
            out() << QStringLiteral(" [%1]").arg(object.guid);
            if (!object.elementFound)
                out() << QStringLiteral(" (элемент не найден)");
            out() << QLatin1Char('\n');
        }
    }
    return 0;
}

int commandTree(const QString &path)
{
    V8Container container;
    QString error;
    if (!container.open(path, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    out() << QStringLiteral("Формат: %1, элементов: %2\n\n")
                 .arg(container.format().label())
                 .arg(container.count());
    out() << QStringLiteral(" №  Имя                                              "
                            "Изменён              Размер   Упак.\n");

    for (int i = 0; i < container.count(); ++i) {
        const V8Element *element = container.at(i);
        if (!element)
            continue;
        QByteArray payload;
        bool compressed = false;
        container.elementPayload(*element, &payload, &compressed, nullptr);
        out() << QStringLiteral("%1  %2  %3  %4  %5\n")
                     .arg(i, 3)
                     .arg(element->name, -48)
                     .arg(formatDateTime(element->modificationDate), -19)
                     .arg(payload.size(), 8)
                     .arg(compressed ? QStringLiteral("да") : QStringLiteral("нет"));
    }
    return 0;
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

int commandDump(const QString &path, const QString &elementName,
                const QString &output, bool raw)
{
    V8Container container;
    QString error;
    if (!container.open(path, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    const V8Element *element = container.find(elementName);
    if (!element) {
        err() << QStringLiteral("Элемент «%1» не найден\n").arg(elementName);
        return 1;
    }

    QByteArray data;
    const bool ok = raw
        ? container.elementData(*element, &data, &error)
        : container.elementPayload(*element, &data, nullptr, &error);
    if (!ok) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    if (output.isEmpty()) {
        if (isProbablyText(data)) {
            out() << V8Text::decode(data, nullptr, nullptr);
            if (!data.endsWith('\n'))
                out() << QLatin1Char('\n');
        } else {
            out() << QStringLiteral("Двоичные данные, %1 байт. Укажите -o для сохранения.\n")
                         .arg(data.size());
            const int sample = static_cast<int>(std::min<qint64>(data.size(), 256));
            QString hex;
            for (int i = 0; i < sample; ++i) {
                hex += QStringLiteral("%1 ").arg(
                    static_cast<unsigned char>(data.at(i)), 2, 16, QLatin1Char('0'));
                if ((i + 1) % 16 == 0)
                    hex += QLatin1Char('\n');
            }
            out() << hex << QLatin1Char('\n');
        }
        return 0;
    }

    QString target = output;
    if (QFileInfo(output).isDir() || output.endsWith('/') || output.endsWith('\\'))
        target = QDir(output).filePath(element->name);

    if (!writeOutputFile(target, data, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }
    out() << QStringLiteral("Сохранено: %1 (%2 байт)\n").arg(target).arg(data.size());
    return 0;
}

int commandMetadata(const QString &path, const QString &objectQuery,
                    const QString &output)
{
    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    QString error;
    MetadataObject *object = findObject(reader.configuration(), objectQuery, &error);
    if (!object) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    QString text;
    if (!reader.readObjectText(*object, &text, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    if (!output.isEmpty()) {
        if (!writeOutputFile(output, text.toUtf8(), &error)) {
            err() << QStringLiteral("Ошибка: %1\n").arg(error);
            return 1;
        }
        out() << QStringLiteral("Сохранено: %1 (%2 байт)\n")
                     .arg(output)
                     .arg(text.toUtf8().size());
        return 0;
    }

    out() << text;
    if (!text.endsWith('\n'))
        out() << QLatin1Char('\n');
    return 0;
}

int commandModules(const QString &path, const QString &objectQuery,
                   const QString &output)
{
    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    QString error;
    MetadataObject *object = findObject(reader.configuration(), objectQuery, &error);
    if (!object) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    reader.loadSections(*object);

    const ModuleExtractor extractor(reader.container());
    const QVector<ModuleInfo> modules = extractor.extractForObject(*object);
    if (modules.isEmpty()) {
        out() << QStringLiteral("Модули объекта «%1» не найдены\n")
                     .arg(object->displayName());
        return 0;
    }

    QString text;
    for (int i = 0; i < modules.size(); ++i) {
        const ModuleInfo &module = modules.at(i);
        text += QStringLiteral("// === %1 (%2) ===\n")
                    .arg(module.name, module.elementName);
        text += module.text;
        if (!module.text.endsWith('\n'))
            text += QLatin1Char('\n');
        if (i + 1 < modules.size())
            text += QLatin1Char('\n');
    }

    if (!output.isEmpty()) {
        if (!writeOutputFile(output, text.toUtf8(), &error)) {
            err() << QStringLiteral("Ошибка: %1\n").arg(error);
            return 1;
        }
        out() << QStringLiteral("Сохранено: %1 (%2 байт, модулей: %3)\n")
                     .arg(output)
                     .arg(text.toUtf8().size())
                     .arg(modules.size());
        return 0;
    }

    out() << text;
    return 0;
}

void printCompositionEntries(const QVector<MetadataEntry> &entries, int depth)
{
    const QString indent(depth * 2, QLatin1Char(' '));
    for (const MetadataEntry &entry : entries) {
        out() << QStringLiteral("%1%2").arg(indent, entry.displayName());
        if (!entry.synonym.isEmpty() && entry.synonym != entry.name)
            out() << QStringLiteral(" / %1").arg(entry.synonym);
        if (!entry.guid.isEmpty())
            out() << QStringLiteral(" [%1]").arg(entry.guid);
        if (!entry.comment.isEmpty())
            out() << QStringLiteral(" — %1").arg(entry.comment);
        out() << QLatin1Char('\n');
        if (!entry.children.isEmpty())
            printCompositionEntries(entry.children, depth + 1);
    }
}

int commandComposition(const QString &path, const QString &objectQuery)
{
    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    QString error;
    MetadataObject *object = findObject(reader.configuration(), objectQuery, &error);
    if (!object) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    reader.loadSections(*object);

    out() << QStringLiteral("%1 — %2\n").arg(object->displayName(), object->typeTitle);
    if (!object->synonym.isEmpty())
        out() << QStringLiteral("Синоним: %1\n").arg(object->synonym);
    out() << QStringLiteral("GUID: %1\n").arg(object->guid);

    int total = 0;
    for (const MetadataSection &section : object->sections) {
        out() << QStringLiteral("\n[%1] — %2\n")
                     .arg(section.title)
                     .arg(section.entries.size());
        printCompositionEntries(section.entries, 1);
        total += section.entries.size();
    }

    if (total == 0)
        out() << QStringLiteral("\nСостав объекта не найден\n");
    return 0;
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
int commandExtract(const QString &path, const QString &objectQuery,
                   const QString &output)
{
    if (output.isEmpty()) {
        err() << QStringLiteral("Ошибка: укажите каталог через -o\n");
        return 1;
    }

    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    QString error;
    MetadataObject *object = findObject(reader.configuration(), objectQuery, &error);
    if (!object) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    ExtractionOptions options;
    ExtractionReport report;
    Extractor extractor(reader);
    if (!extractor.extractObject(*object, output, options, &report, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    out() << QStringLiteral("Объект «%1» сохранён в %2\n")
                 .arg(object->displayName(), report.outputDirectory);
    printReport(report);
    return 0;
}

int commandExtractAll(const QString &path, const QString &output)
{
    if (output.isEmpty()) {
        err() << QStringLiteral("Ошибка: укажите каталог через -o\n");
        return 1;
    }

    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    ExtractionOptions options;
    ExtractionReport report;
    QString error;
    Extractor extractor(reader);
    if (!extractor.extractAll(output, options, &report, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    out() << QStringLiteral("Конфигурация сохранена в %1\n").arg(report.outputDirectory);
    printReport(report);
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("v8reader_cli"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.4.0"));

#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Просмотр конфигураций и внешних обработок 1С:Предприятие 8"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(
        {QStringLiteral("o"), QStringLiteral("output")},
        QStringLiteral("Каталог или файл для сохранения"), QStringLiteral("путь")));
    parser.addOption(QCommandLineOption(
        QStringLiteral("raw"), QStringLiteral("Не распаковывать данные элемента")));
    parser.addPositionalArgument(QStringLiteral("команда"),
                                 QStringLiteral("Команда: info, list, tree, dump, metadata, "
                                                "modules, composition, mxl, extract, extract-all"));
    parser.addPositionalArgument(QStringLiteral("файл"),
                                 QStringLiteral("Файл .cf, .cfe, .epf или .erf"));
    parser.addPositionalArgument(QStringLiteral("объект"),
                                 QStringLiteral("GUID, имя или синоним объекта (для dump — "
                                                "имя элемента контейнера)"),
                                 QStringLiteral("[объект]"));

    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        printHelp();
        return 1;
    }

    const QString command = positional.at(0).toLower();
    if (command == QStringLiteral("help")) {
        printHelp();
        return 0;
    }

    if (positional.size() < 2) {
        err() << QStringLiteral("Ошибка: не указан файл конфигурации\n");
        printHelp();
        return 1;
    }

    const QString path = positional.at(1);
    if (!QFileInfo::exists(path)) {
        err() << QStringLiteral("Ошибка: файл «%1» не найден\n").arg(path);
        return 1;
    }

    const QString argument = positional.size() >= 3 ? positional.at(2) : QString();
    const QString output = parser.value(QStringLiteral("output"));
    const bool raw = parser.isSet(QStringLiteral("raw"));

    if (command == QStringLiteral("info"))
        return commandInfo(path);
    if (command == QStringLiteral("list"))
        return commandList(path);
    if (command == QStringLiteral("tree"))
        return commandTree(path);
    if (command == QStringLiteral("dump")) {
        if (argument.isEmpty()) {
            err() << QStringLiteral("Ошибка: не указан элемент контейнера\n");
            return 1;
        }
        return commandDump(path, argument, output, raw);
    }
    if (command == QStringLiteral("metadata"))
        return commandMetadata(path, argument, output);
    if (command == QStringLiteral("modules"))
        return commandModules(path, argument, output);
    if (command == QStringLiteral("composition"))
        return commandComposition(path, argument);
    if (command == QStringLiteral("mxl"))
        return commandMxl(path, argument, output);
    if (command == QStringLiteral("extract"))
        return commandExtract(path, argument, output);
    if (command == QStringLiteral("extract-all"))
        return commandExtractAll(path, output);

    err() << QStringLiteral("Ошибка: неизвестная команда «%1»\n").arg(command);
    printHelp();
    return 1;
}
