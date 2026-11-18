#include "core/Extractor.h"

#include "core/BinaryText.h"
#include "core/MetadataCatalog.h"
#include "core/ModuleExtractor.h"
#include "core/V8Text.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace v8reader {
namespace {

const MetadataSection *findSection(const MetadataObject &object, MetadataSectionKind kind)
{
    for (const MetadataSection &section : object.sections) {
        if (section.kind == kind)
            return &section;
    }
    return nullptr;
}

QString entryFileName(const MetadataEntry &entry)
{
    QString name = entry.name;
    if (name.isEmpty())
        name = entry.synonym;
    if (name.isEmpty())
        name = entry.guid;
    if (name.isEmpty())
        name = QStringLiteral("element");
    return Extractor::safeFileName(name);
}

QString fileNameWithGuid(const MetadataEntry &entry)
{
    QString name = entryFileName(entry);
    if (!entry.guid.isEmpty() && name.compare(entry.guid, Qt::CaseInsensitive) != 0)
        name += QLatin1Char('_') + entry.guid;
    else if (!entry.guid.isEmpty())
        name = entry.guid;
    return name;
}

void collectBase64(const V8TextNode &node, QByteArray *best, int depth)
{
    if (!best || depth > 64)
        return;

    if (node.isScalar()) {
        if (!node.value.startsWith(QLatin1String("#base64:"))
            && !node.value.startsWith(QLatin1String("#data:"))) {
            return;
        }

        QByteArray decoded;
        if (V8Text::decodeBase64Value(node.value, &decoded)
            && decoded.size() > best->size()
            && decoded.size() <= 256 * 1024 * 1024) {
            *best = decoded;
        }
        return;
    }

    for (const V8TextNode &child : node.children)
        collectBase64(child, best, depth + 1);
}

QByteArray largestBase64(const QString &text)
{
    V8TextNode root;
    if (!V8Text::parse(text, root, nullptr))
        return QByteArray();

    QByteArray result;
    collectBase64(root, &result, 0);
    return result;
}

QString binaryExtension(const QByteArray &data)
{
    QByteArray probe = data.left(16);
    if (probe.startsWith("\xef\xbb\xbf"))
        probe.remove(0, 3);
    while (!probe.isEmpty()
           && (probe.at(0) == ' ' || probe.at(0) == '\t'
               || probe.at(0) == '\r' || probe.at(0) == '\n')) {
        probe.remove(0, 1);
    }

    if (probe.startsWith("<?xml") || probe.startsWith("<"))
        return QStringLiteral(".xml");
    if (data.startsWith("PK\x03\x04"))
        return QStringLiteral(".zip");
    if (data.startsWith("\x89PNG\r\n\x1a\n"))
        return QStringLiteral(".png");
    if (data.startsWith("\xff\xd8\xff"))
        return QStringLiteral(".jpg");
    if (data.startsWith("GIF8"))
        return QStringLiteral(".gif");
    if (data.startsWith("%PDF"))
        return QStringLiteral(".pdf");
    if (data.startsWith("\x1f\x8b"))
        return QStringLiteral(".gz");
    if (data.startsWith("\xd0\xcf\x11\xe0"))
        return QStringLiteral(".ole");
    return QStringLiteral(".bin");
}

QString decodeText(const QByteArray &data)
{
    if (!isProbablyText(data, true))
        return QString();
    return V8Text::decode(data, nullptr, nullptr);
}

} // namespace

Extractor::Extractor(ConfigurationReader &reader)
    : m_reader(reader)
{
}

bool Extractor::extractObject(MetadataObject &object, const QString &outputRoot,
                              const ExtractionOptions &options, ExtractionReport *report,
                              QString *error) const
{
    if (error)
        error->clear();
    if (report)
        *report = ExtractionReport();

    if (outputRoot.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("Не задан каталог для сохранения");
        return false;
    }

    m_reader.loadSections(object);

    QString categoryName = object.categoryTitle;
    if (categoryName.isEmpty())
        categoryName = object.typeTitle;
    if (categoryName.isEmpty())
        categoryName = QStringLiteral("Объекты");

    const QString directory = QDir(outputRoot).filePath(
        safeDirectoryName(categoryName) + QLatin1Char('/')
        + safeDirectoryName(object.displayName()));

    if (!QDir().mkpath(directory)) {
        if (error)
            *error = QStringLiteral("Не удалось создать каталог: %1").arg(directory);
        return false;
    }

    if (report)
        report->outputDirectory = QDir(directory).absolutePath();

    bool ok = true;
    if (!writeInfo(object, directory, report, error))
        ok = false;

    if (options.includeRawMetadata) {
        QString metadataText;
        QString readError;
        if (m_reader.readObjectText(object, &metadataText, &readError)
            && !metadataText.isEmpty()) {
            writeTextFile(QDir(directory).filePath(QStringLiteral("metadata.txt")),
                          metadataText, report, error);
        } else if (!readError.isEmpty()) {
            addWarning(report, readError);
        }
    }

    // Исходные данные объекта: текст уже сохранён в metadata.txt,
    // двоичные данные сохраняем отдельно.
    const QString elementName = object.elementName.isEmpty() ? object.guid : object.elementName;
    if (const V8Element *element = m_reader.container().find(elementName)) {
        QByteArray payload;
        if (m_reader.container().elementPayload(*element, &payload, nullptr, nullptr)
            && !payload.isEmpty() && !isProbablyText(payload, true)) {
            if (!writeFile(QDir(directory).filePath(QStringLiteral("object.bin")),
                           payload, report, error)) {
                ok = false;
            }
        }
    }

    if (!writeComposition(object, directory, report, error))
        ok = false;

    if (!writeModules(object, directory, options, report, error))
        ok = false;

    if (options.includeForms) {
        if (!writeNamedSection(object, MetadataSectionKind::Forms, directory,
                               QStringLiteral("forms"), options, report, error)) {
            ok = false;
        }
    }

    if (options.includeTemplates) {
        if (!writeNamedSection(object, MetadataSectionKind::Templates, directory,
                               QStringLiteral("templates"), options, report, error)) {
            ok = false;
        }
    }

    return ok;
}

bool Extractor::extractAll(const QString &outputRoot, const ExtractionOptions &options,
                           ExtractionReport *report, QString *error) const
{
    if (error)
        error->clear();
    if (report)
        *report = ExtractionReport();

    if (!QDir().mkpath(outputRoot)) {
        if (error)
            *error = QStringLiteral("Не удалось создать каталог: %1").arg(outputRoot);
        return false;
    }

    Configuration &configuration = m_reader.configuration();
    if (configuration.categories.isEmpty()) {
        if (error)
            *error = QStringLiteral("В конфигурации не найдено объектов метаданных");
        return false;
    }

    if (report)
        report->outputDirectory = QDir(outputRoot).absolutePath();

    bool ok = true;
    int extracted = 0;

    for (MetadataCategory &category : configuration.categories) {
        for (MetadataObject &object : category.objects) {
            ExtractionReport objectReport;
            QString objectError;
            if (!extractObject(object, outputRoot, options, &objectReport, &objectError)) {
                ok = false;
                addWarning(report, QStringLiteral("Объект «%1»: %2")
                                      .arg(object.displayName(), objectError));
                continue;
            }

            ++extracted;
            if (report) {
                report->files += objectReport.files;
                report->warnings += objectReport.warnings;
                report->moduleCount += objectReport.moduleCount;
                report->formCount += objectReport.formCount;
                report->templateCount += objectReport.templateCount;
            }
        }
    }

    if (extracted == 0) {
        if (error)
            *error = QStringLiteral("Не удалось сохранить ни одного объекта");
        return false;
    }

    if (!ok && report)
        addWarning(report, QStringLiteral("Некоторые объекты сохранены не полностью"));

    return true;
}

QString Extractor::safeFileName(const QString &name)
{
    QString result = name.trimmed();
    if (result.isEmpty())
        return QStringLiteral("element");

    const QString forbidden = QStringLiteral("<>:\"/\\|?*");
    for (int i = 0; i < result.size(); ++i) {
        const QChar ch = result.at(i);
        if (forbidden.contains(ch) || ch.unicode() < 0x20)
            result[i] = QLatin1Char('_');
    }

    while (result.endsWith(QLatin1Char('.')) || result.endsWith(QLatin1Char(' ')))
        result.chop(1);

    // Windows не допускает имена устройств.
    static const QStringList reserved = {
        QStringLiteral("CON"), QStringLiteral("PRN"), QStringLiteral("AUX"),
        QStringLiteral("NUL"), QStringLiteral("COM1"), QStringLiteral("COM2"),
        QStringLiteral("COM3"), QStringLiteral("COM4"), QStringLiteral("COM5"),
        QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"), QStringLiteral("LPT2"),
        QStringLiteral("LPT3"), QStringLiteral("LPT4"), QStringLiteral("LPT5"),
        QStringLiteral("LPT6"), QStringLiteral("LPT7"), QStringLiteral("LPT8"),
        QStringLiteral("LPT9"),
    };
    if (reserved.contains(result, Qt::CaseInsensitive))
        result.prepend(QLatin1Char('_'));

    if (result.size() > 120)
        result = result.left(120);

    return result;
}

QString Extractor::safeDirectoryName(const QString &name)
{
    return safeFileName(name);
}

bool Extractor::writeFile(const QString &path, const QByteArray &data,
                          ExtractionReport *report, QString *error) const
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
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

    const qint64 written = file.write(data);
    file.close();
    if (written != data.size()) {
        if (error)
            *error = QStringLiteral("Не удалось записать файл «%1»").arg(path);
        return false;
    }

    if (report) {
        ExtractionFile entry;
        entry.path = info.absoluteFilePath();
        entry.size = written;
        report->files.append(entry);
    }
    return true;
}

bool Extractor::writeTextFile(const QString &path, const QString &text,
                              ExtractionReport *report, QString *error) const
{
    return writeFile(path, text.toUtf8(), report, error);
}

bool Extractor::writeInfo(const MetadataObject &object, const QString &directory,
                          ExtractionReport *report, QString *error) const
{
    const Configuration &configuration = m_reader.configuration();

    QString text;
    text += QStringLiteral("Конфигурация: %1\n").arg(configuration.displayName());
    if (!configuration.version.isEmpty())
        text += QStringLiteral("Версия конфигурации: %1\n").arg(configuration.version);
    text += QStringLiteral("Формат контейнера: %1\n").arg(configuration.formatLabel);
    text += QStringLiteral("Файл: %1\n").arg(configuration.sourcePath);
    text += QLatin1Char('\n');
    text += QStringLiteral("Категория: %1\n").arg(object.categoryTitle);
    text += QStringLiteral("Тип: %1\n").arg(object.typeTitle);
    text += QStringLiteral("Имя: %1\n").arg(object.name);
    text += QStringLiteral("Синоним: %1\n").arg(object.synonym);
    text += QStringLiteral("Комментарий: %1\n").arg(object.comment);
    text += QStringLiteral("GUID: %1\n").arg(object.guid);
    text += QStringLiteral("Элемент контейнера: %1\n").arg(object.elementName);
    text += QStringLiteral("Элемент найден: %1\n")
                .arg(object.elementFound ? QStringLiteral("да") : QStringLiteral("нет"));
    text += QStringLiteral("Описание найдено: %1\n")
                .arg(object.descriptorFound ? QStringLiteral("да") : QStringLiteral("нет"));
    text += QStringLiteral("Таблиц состава: %1\n").arg(object.sections.size());

    for (const MetadataSection &section : object.sections) {
        text += QStringLiteral("  %1: %2\n")
                    .arg(section.title)
                    .arg(section.entries.size());
    }

    text += QStringLiteral("Сохранено: %1\n")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm:ss")));

    return writeTextFile(QDir(directory).filePath(QStringLiteral("info.txt")),
                         text, report, error);
}

bool Extractor::writeComposition(const MetadataObject &object, const QString &directory,
                                 ExtractionReport *report, QString *error) const
{
    QString text;
    text += QStringLiteral("Объект: %1\n").arg(object.displayName());
    text += QStringLiteral("GUID: %1\n").arg(object.guid);
    text += QStringLiteral("Категория: %1\n").arg(object.categoryTitle);
    text += QLatin1Char('\n');

    for (const MetadataSection &section : object.sections) {
        text += QStringLiteral("[%1] (%2) — %3\n")
                    .arg(section.title, section.guid)
                    .arg(section.entries.size());

        for (const MetadataEntry &entry : section.entries) {
            text += QStringLiteral("  %1: %2")
                        .arg(entry.typeTitle, entry.displayName());
            if (!entry.guid.isEmpty())
                text += QStringLiteral(" [%1]").arg(entry.guid);
            if (!entry.typeText.isEmpty())
                text += QStringLiteral(" · %1").arg(entry.typeText);
            if (!entry.synonym.isEmpty())
                text += QStringLiteral(" / %1").arg(entry.synonym);
            if (!entry.comment.isEmpty())
                text += QStringLiteral(" — %1").arg(entry.comment);
            text += QLatin1Char('\n');

            for (const MetadataEntry &child : entry.children) {
                text += QStringLiteral("    %1: %2")
                            .arg(child.typeTitle, child.displayName());
                if (!child.guid.isEmpty())
                    text += QStringLiteral(" [%1]").arg(child.guid);
                if (!child.typeText.isEmpty())
                    text += QStringLiteral(" · %1").arg(child.typeText);
                if (!child.synonym.isEmpty())
                    text += QStringLiteral(" / %1").arg(child.synonym);
                text += QLatin1Char('\n');
            }
        }
        text += QLatin1Char('\n');
    }

    return writeTextFile(QDir(directory).filePath(QStringLiteral("composition.txt")),
                         text, report, error);
}

bool Extractor::writeModules(const MetadataObject &object, const QString &directory,
                             const ExtractionOptions &options,
                             ExtractionReport *report, QString *error) const
{
    if (!options.includeModules)
        return true;

    const ModuleExtractor extractor(m_reader.container());
    const QVector<ModuleInfo> modules = extractor.extractForObject(object);

    bool ok = true;
    int index = 0;
    for (const ModuleInfo &module : modules) {
        ++index;
        QString baseName = module.name;
        if (baseName.isEmpty())
            baseName = QStringLiteral("Модуль");
        const QString path = QDir(directory).filePath(
            QStringLiteral("modules/%1_%2.bsl")
                .arg(index, 2, 10, QLatin1Char('0'))
                .arg(safeFileName(baseName)));

        if (!writeTextFile(path, module.text, report, error))
            ok = false;
    }

    if (report)
        report->moduleCount += modules.size();

    return ok;
}

bool Extractor::writeNamedSection(const MetadataObject &object, MetadataSectionKind kind,
                                  const QString &directory, const QString &subdirectory,
                                  const ExtractionOptions &options,
                                  ExtractionReport *report, QString *error) const
{
    const MetadataSection *section = findSection(object, kind);
    if (!section)
        return true;

    bool ok = true;
    int count = 0;

    for (const MetadataEntry &entry : section->entries) {
        if (entry.guid.isEmpty())
            continue;

        QString metadataText;
        if (m_reader.readElementText(entry.guid, &metadataText, nullptr)
            && !metadataText.isEmpty()) {
            const QString metaPath = QDir(directory).filePath(
                subdirectory + QLatin1Char('/') + fileNameWithGuid(entry)
                + QStringLiteral(".meta.txt"));
            if (!writeTextFile(metaPath, metadataText, report, error))
                ok = false;
        }

        const QString elementName = entry.guid + QStringLiteral(".0");
        if (const V8Element *element = m_reader.container().find(elementName)) {
            QByteArray payload;
            if (m_reader.container().elementPayload(*element, &payload, nullptr, nullptr)
                && !payload.isEmpty()) {
                if (options.includePayloads) {
                    const QString basePath = QDir(directory).filePath(
                        subdirectory + QLatin1Char('/') + fileNameWithGuid(entry));
                    if (!writeElementPayload(basePath, payload, report, error))
                        ok = false;
                }
            }
        }

        ++count;
    }

    if (report) {
        if (kind == MetadataSectionKind::Forms)
            report->formCount += count;
        else if (kind == MetadataSectionKind::Templates)
            report->templateCount += count;
    }

    return ok;
}

bool Extractor::writeElementPayload(const QString &basePath, const QByteArray &payload,
                                    ExtractionReport *report, QString *error) const
{
    bool ok = true;

    const QString text = decodeText(payload);
    if (!text.isEmpty()) {
        if (!writeTextFile(basePath + QStringLiteral(".txt"), text, report, error))
            ok = false;

        const QByteArray decoded = largestBase64(text);
        if (!decoded.isEmpty() && decoded.size() <= 256 * 1024 * 1024) {
            const QString decodedExtension = binaryExtension(decoded);
            if (!writeFile(basePath + QStringLiteral(".decoded") + decodedExtension,
                           decoded, report, error)) {
                ok = false;
            }
        }
    } else if (!writeFile(basePath + QStringLiteral(".bin"), payload, report, error)) {
        ok = false;
    }

    return ok;
}

void Extractor::addWarning(ExtractionReport *report, const QString &message) const
{
    if (report && !message.isEmpty() && !report->warnings.contains(message))
        report->warnings.append(message);
}

} // namespace v8reader
