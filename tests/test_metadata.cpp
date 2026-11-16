#include "TestContainerBuilder.h"
#include "core/ConfigurationReader.h"
#include "core/Extractor.h"
#include "core/MetadataCatalog.h"
#include "core/ModuleExtractor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace v8reader;
using namespace v8reader::test;

namespace {

const QString kConfigGuid = QStringLiteral("11111111-1111-1111-1111-111111111111");
const QString kCatalogGuid = QStringLiteral("22222222-2222-2222-2222-222222222222");
const QString kLanguageGuid = QStringLiteral("33333333-3333-3333-3333-333333333333");
const QString kMissingGuid = QStringLiteral("44444444-4444-4444-4444-444444444444");
const QString kAttributeGuid = QStringLiteral("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
const QString kTabularGuid = QStringLiteral("bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb");
const QString kTabularAttributeGuid =
    QStringLiteral("cccccccc-cccc-cccc-cccc-cccccccccccc");
const QString kFormGuid = QStringLiteral("dddddddd-dddd-dddd-dddd-dddddddddddd");
const QString kTemplateGuid = QStringLiteral("eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee");

const QString kModuleText = QStringLiteral(
    "// Модуль объекта\n"
    "Процедура ПриСозданииОбъекта()\n"
    "    Сообщить(\"Тестовый модуль\");\n"
    "КонецПроцедуры\n");

const QByteArray kFormXml =
    QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\"?><Form><Name>")
    + QStringLiteral("Форма1").toUtf8()
    + QByteArray("</Name></Form>");

QString configText()
{
    return QStringLiteral(
               "{3,{1,0,%1},\"ТестКонфигурация\",{0,\"ru\",\"Тестовая конфигурация\"},"
               "\"Комментарий конфигурации\",\n"
               "{cf4abea6-37b2-11d4-940f-008048da11f9,2,%2,%3},\n"
               "{9cd510ce-abfc-11d4-9434-004095e12fc7,1,%4}}")
        .arg(kConfigGuid)
        .arg(kCatalogGuid)
        .arg(kMissingGuid)
        .arg(kLanguageGuid);
}

QString catalogText()
{
    return QStringLiteral(
               "{3,{1,0,%1},\"Справочник1\",{0,\"ru\",\"Справочник тестовый\"},"
               "\"Комментарий справочника\",\n"
               "{cf4abea7-37b2-11d4-940f-008048da11f9,1,"
               "{3,{1,0,%2},\"Реквизит1\",{0,\"ru\",\"Реквизит 1\"},"
               "\"Описание реквизита\"}},\n"
               "{932159f9-95b2-4e76-a8dd-8849fe5c5ded,1,"
               "{3,{1,0,%3},\"ТабличнаяЧасть1\",{0,\"ru\",\"Строки\"},\"\","
               "{888744e1-b616-11d4-9436-004095e12fc7,1,"
               "{3,{1,0,%4},\"РеквизитТЧ\",{0,\"ru\",\"Реквизит ТЧ\"},\"\"}}}},\n"
               "{fdf816d2-1ead-11d5-b975-0050bae0a95d,1,%5},\n"
               "{3daea016-69b7-4ed4-9453-127911372fe6,1,%6},\n"
               "{4fe87c89-9ad4-43f6-9fdb-9dc83b3879c6,0}}")
        .arg(kCatalogGuid)
        .arg(kAttributeGuid)
        .arg(kTabularGuid)
        .arg(kTabularAttributeGuid)
        .arg(kFormGuid)
        .arg(kTemplateGuid);
}

QString languageText()
{
    return QStringLiteral(
               "{3,{1,0,%1},\"Русский\",{0,\"ru\",\"Русский\"},\"\"}")
        .arg(kLanguageGuid);
}

QString formText()
{
    return QStringLiteral(
               "{3,{1,0,%1},\"Форма1\",{0,\"ru\",\"Форма 1\"},\"\"}")
        .arg(kFormGuid);
}

QString templateText()
{
    return QStringLiteral(
               "{3,{1,0,%1},\"Макет1\",{0,\"ru\",\"Макет 1\"},\"\"}")
        .arg(kTemplateGuid);
}

QString formPayloadText()
{
    return QStringLiteral("{2,0,{1,0},\"#base64:%1\"}")
        .arg(QString::fromLatin1(kFormXml.toBase64()));
}

QByteArray buildContainer(int version, QString *error)
{
    const QVector<TestElement> elements = {
        {QStringLiteral("root"), QStringLiteral("{2,%1,{1,0,0,0}}")
                                     .arg(kConfigGuid).toUtf8(), true},
        {QStringLiteral("version"), QStringLiteral("{216,0,{80327,0}}").toUtf8(),
         false},
        {kConfigGuid, configText().toUtf8(), true},
        {kCatalogGuid, catalogText().toUtf8(), true},
        {kLanguageGuid, languageText().toUtf8(), true},
        {kCatalogGuid + QStringLiteral(".0"), kModuleText.toUtf8(), true},
        {kFormGuid, formText().toUtf8(), true},
        {kFormGuid + QStringLiteral(".0"), formPayloadText().toUtf8(), true},
        {kTemplateGuid, templateText().toUtf8(), true},
        {kTemplateGuid + QStringLiteral(".0"),
         QStringLiteral("ТабличныйДокумент").toUtf8(), true},
    };

    return TestContainerBuilder::build(version, elements, error);
}

// Синтетический контейнер расширения конфигурации (.cfe).
QByteArray buildExtensionContainer(QString *error)
{
    const QString extensionGuid = QStringLiteral("abababab-abab-abab-abab-abababababab");
    const QString configInfo = QStringLiteral("{0,{216,0,{80327,0}}},{2,%1,AAAA}")
                                   .arg(extensionGuid);
    const QString configText = QStringLiteral(
        "{2,{%1},2,"
        "{9cd510cd-abfc-11d4-9434-004095e12fc7,{1,{52,{0,{1,{0,0,%1},"
        "\"РасширениеТест\",{1,\"ru\",\"Расширение тестовое\"},\"\"}}}}},"
        "{9cd510ce-abfc-11d4-9434-004095e12fc7,1,%2}}")
                                   .arg(extensionGuid, kLanguageGuid);

    const QVector<TestElement> elements = {
        {QStringLiteral("configinfo"), configInfo.toUtf8(), false},
        {extensionGuid, configText.toUtf8(), true},
        {kLanguageGuid, languageText().toUtf8(), true},
    };
    return TestContainerBuilder::build(15, elements, error);
}

// Синтетическая внешняя обработка (.epf).
QByteArray buildExternalContainer(QString *error)
{
    const QString rootGuid = QStringLiteral("11111111-2222-3333-4444-555555555555");
    const QString objectGuid = QStringLiteral("66666666-7777-8888-9999-000000000000");

    QString rootText = QStringLiteral("{1,{%1},1,{c3831ec8-d8d5-4f93-8a22-f9bfae07327f,{1,")
                           .arg(rootGuid);
    rootText += QStringLiteral("{4,%1,%1,{0,{0,{0,0,%1},\"Обработка1\","
                               "{1,\"ru\",\"Обработка тестовая\"},\"\"}},"
                               "%1,\"\",00000000-0000-0000-0000-000000000000},4,")
                    .arg(objectGuid);
    rootText += QStringLiteral("{2bcef0d1-0981-11d6-b9b8-0050bae0a95d,0},");
    rootText += QStringLiteral("{3daea016-69b7-4ed4-9453-127911372fe6,1,%1},")
                    .arg(kTemplateGuid);
    rootText += QStringLiteral("{d5b0e5ed-256d-401c-9c36-f630cafd8a62,1,%1},")
                    .arg(kFormGuid);
    rootText += QStringLiteral("{ec6bb5e5-b7a8-4d75-bec9-658107a699cf,1,"
                               "{{0,{0,0,%1},\"Реквизит1\",{1,\"ru\",\"Реквизит 1\"},\"\"},0}}}}}")
                    .arg(kAttributeGuid);

    const QVector<TestElement> elements = {
        {QStringLiteral("root"), QStringLiteral("{2,%1,}").arg(rootGuid).toUtf8(), true},
        {rootGuid, rootText.toUtf8(), true},
        {objectGuid + QStringLiteral(".0"), kModuleText.toUtf8(), true},
        {kFormGuid, formText().toUtf8(), true},
        {kTemplateGuid, templateText().toUtf8(), true},
    };
    return TestContainerBuilder::build(15, elements, error);
}

bool writeFile(const QString &path, const QByteArray &data, QString *error){
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (file.write(data) != data.size()) {
        if (error)
            *error = QStringLiteral("Не удалось записать тестовый контейнер");
        return false;
    }
    return true;
}

const MetadataSection *sectionByKind(const MetadataObject &object,
                                     MetadataSectionKind kind)
{
    for (const MetadataSection &section : object.sections) {
        if (section.kind == kind)
            return &section;
    }
    return nullptr;
}

QString nativePath(const QString &path)
{
    return QDir::fromNativeSeparators(path);
}

} // namespace

class TestMetadata : public QObject
{
    Q_OBJECT

private slots:
    void parseConfiguration();
    void objectComposition();
    void findsModules();
    void extractsObject();
    void extractsAll();
    void extensionFile();
    void externalProcessingFile();
    void legacyFormatNames();
    void lazySections();
    void missingRootWarning();
    void missingConfigurationObject();
    void realSampleMetadata();
    void realBpDocument();
    void realExternalProcessing();
    void realExtension();
};

void TestMetadata::parseConfiguration()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString error;
    const QByteArray data = buildContainer(16, &error);
    QVERIFY2(!data.isEmpty(), qPrintable(error));

    const QString path = temp.filePath(QStringLiteral("synthetic.cf"));
    QVERIFY2(writeFile(path, data, &error), qPrintable(error));

    ConfigurationReader reader;
    QString openError;
    QVERIFY2(reader.open(path, &openError), qPrintable(openError));

    const Configuration &configuration = reader.configuration();
    QCOMPARE(reader.container().format().version, 16);
    QCOMPARE(configuration.name, QStringLiteral("ТестКонфигурация"));
    QCOMPARE(configuration.synonym, QStringLiteral("Тестовая конфигурация"));
    QCOMPARE(configuration.comment, QStringLiteral("Комментарий конфигурации"));
    QCOMPARE(configuration.version, QStringLiteral("80327"));
    QCOMPARE(configuration.configGuid, kConfigGuid);
    QCOMPARE(configuration.formatVersion, 16);
    QVERIFY(configuration.fileSize > 0);
    QVERIFY2(configuration.warnings.isEmpty(),
             qPrintable(configuration.warnings.join(QStringLiteral("; "))));
    QCOMPARE(configuration.categories.size(), 2);
    QCOMPARE(configuration.objectCount(), 3);

    const MetadataCategory *catalogs = configuration.category(
        QStringLiteral("cf4abea6-37b2-11d4-940f-008048da11f9"));
    QVERIFY(catalogs);
    QCOMPARE(catalogs->title, QStringLiteral("Справочники"));
    QCOMPARE(catalogs->objects.size(), 2);

    const MetadataObject *catalog = configuration.object(kCatalogGuid);
    QVERIFY(catalog);
    QCOMPARE(catalog->name, QStringLiteral("Справочник1"));
    QCOMPARE(catalog->synonym, QStringLiteral("Справочник тестовый"));
    QCOMPARE(catalog->comment, QStringLiteral("Комментарий справочника"));
    QCOMPARE(catalog->categoryTitle, QStringLiteral("Справочники"));
    QCOMPARE(catalog->typeTitle, QStringLiteral("Справочник"));
    QCOMPARE(catalog->elementName, kCatalogGuid);
    QVERIFY(catalog->elementFound);
    QVERIFY(catalog->descriptorFound);
    // Состав объекта разбирается по требованию (лениво).
    QVERIFY(!catalog->sectionsLoaded);
    QVERIFY(catalog->sections.isEmpty());

    const MetadataObject *missing = configuration.object(kMissingGuid);
    QVERIFY(missing);
    QCOMPARE(missing->guid, kMissingGuid);
    QCOMPARE(missing->name, kMissingGuid);
    QVERIFY(!missing->elementFound);
    QVERIFY(!missing->descriptorFound);

    const MetadataCategory *languages = configuration.category(
        QStringLiteral("9cd510ce-abfc-11d4-9434-004095e12fc7"));
    QVERIFY(languages);
    QCOMPARE(languages->title, QStringLiteral("Языки"));
    QCOMPARE(languages->objects.size(), 1);
    QCOMPARE(languages->objects.at(0).name, QStringLiteral("Русский"));
    QCOMPARE(languages->objects.at(0).synonym, QStringLiteral("Русский"));
}

void TestMetadata::objectComposition()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString error;
    const QString path = temp.filePath(QStringLiteral("synthetic.cf"));
    QVERIFY2(writeFile(path, buildContainer(15, &error), &error), qPrintable(error));

    ConfigurationReader reader;
    QVERIFY2(reader.open(path, &error), qPrintable(error));

    MetadataObject *catalog = reader.configuration().object(kCatalogGuid);
    QVERIFY(catalog);
    QVERIFY(reader.loadSections(*catalog));
    QCOMPARE(catalog->sections.size(), 5);

    const MetadataSection *templates = sectionByKind(
        *catalog, MetadataSectionKind::Templates);
    QVERIFY(templates);
    QCOMPARE(templates->entries.size(), 1);
    QCOMPARE(templates->entries.at(0).name, QStringLiteral("Макет1"));
    QCOMPARE(templates->entries.at(0).synonym, QStringLiteral("Макет 1"));
    QCOMPARE(templates->entries.at(0).guid, kTemplateGuid);

    const MetadataSection *commands = sectionByKind(
        *catalog, MetadataSectionKind::Commands);
    QVERIFY(commands);
    QVERIFY(commands->isEmpty());

    const MetadataSection *tabular = sectionByKind(
        *catalog, MetadataSectionKind::TabularSections);
    QVERIFY(tabular);
    QCOMPARE(tabular->entries.size(), 1);
    const MetadataEntry &rows = tabular->entries.at(0);
    QCOMPARE(rows.name, QStringLiteral("ТабличнаяЧасть1"));
    QCOMPARE(rows.synonym, QStringLiteral("Строки"));
    QCOMPARE(rows.children.size(), 1);
    QCOMPARE(rows.children.at(0).name, QStringLiteral("РеквизитТЧ"));
    QCOMPARE(rows.children.at(0).synonym, QStringLiteral("Реквизит ТЧ"));

    const MetadataSection *attributes = sectionByKind(
        *catalog, MetadataSectionKind::Attributes);
    QVERIFY(attributes);
    QCOMPARE(attributes->entries.size(), 1);
    QCOMPARE(attributes->entries.at(0).name, QStringLiteral("Реквизит1"));
    QCOMPARE(attributes->entries.at(0).comment, QStringLiteral("Описание реквизита"));

    const MetadataSection *forms = sectionByKind(
        *catalog, MetadataSectionKind::Forms);
    QVERIFY(forms);
    QCOMPARE(forms->entries.size(), 1);
    QCOMPARE(forms->entries.at(0).name, QStringLiteral("Форма1"));
    QCOMPARE(forms->entries.at(0).guid, kFormGuid);
}

void TestMetadata::findsModules()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString error;
    const QString path = temp.filePath(QStringLiteral("synthetic.cf"));
    QVERIFY2(writeFile(path, buildContainer(16, &error), &error), qPrintable(error));

    ConfigurationReader reader;
    QVERIFY2(reader.open(path, &error), qPrintable(error));

    const MetadataObject *catalog = reader.configuration().object(kCatalogGuid);
    QVERIFY(catalog);

    ModuleExtractor extractor(reader.container());
    const QVector<ModuleInfo> modules = extractor.extractForObject(*catalog);
    QCOMPARE(modules.size(), 1);
    QCOMPARE(modules.at(0).name, QStringLiteral("Модуль объекта"));
    QCOMPARE(modules.at(0).elementName, kCatalogGuid + QStringLiteral(".0"));
    QVERIFY(modules.at(0).text.contains(
        QStringLiteral("Процедура ПриСозданииОбъекта")));
    QVERIFY(!modules.at(0).isEmpty());

    QVERIFY(ModuleExtractor::looksLikeModule(kModuleText));
    QVERIFY(!ModuleExtractor::looksLikeModule(QStringLiteral("{3,{1,0}}")));
    QCOMPARE(ModuleExtractor::moduleLabelForSuffix(1),
             QStringLiteral("Модуль менеджера"));
    QVERIFY(ModuleExtractor::moduleLabelForSuffix(-1).isEmpty());
}

void TestMetadata::extractsObject()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString error;
    const QString path = temp.filePath(QStringLiteral("synthetic.cf"));
    QVERIFY2(writeFile(path, buildContainer(16, &error), &error), qPrintable(error));

    ConfigurationReader reader;
    QVERIFY2(reader.open(path, &error), qPrintable(error));

    MetadataObject *catalog = reader.configuration().object(kCatalogGuid);
    QVERIFY(catalog);

    const QString outputRoot = temp.filePath(QStringLiteral("out"));
    Extractor extractor(reader);
    ExtractionReport report;
    QString extractError;
    QVERIFY2(extractor.extractObject(*catalog, outputRoot, ExtractionOptions(),
                                     &report, &extractError),
             qPrintable(extractError));

    QCOMPARE(report.moduleCount, 1);
    QCOMPARE(report.formCount, 1);
    QCOMPARE(report.templateCount, 1);
    QVERIFY(!report.isEmpty());

    const QString directory = QDir(outputRoot).filePath(
        QStringLiteral("Справочники/Справочник1"));
    const auto exists = [&](const QString &relative) {
        return QFileInfo::exists(QDir(directory).filePath(relative));
    };

    QVERIFY(exists(QStringLiteral("info.txt")));
    QVERIFY(exists(QStringLiteral("metadata.txt")));
    QVERIFY(exists(QStringLiteral("composition.txt")));
    QVERIFY(exists(QStringLiteral("modules/01_Модуль объекта.bsl")));
    QVERIFY(exists(QStringLiteral("forms/Форма1_%1.meta.txt").arg(kFormGuid)));
    QVERIFY(exists(QStringLiteral("forms/Форма1_%1.txt").arg(kFormGuid)));
    QVERIFY(exists(QStringLiteral("forms/Форма1_%1.decoded.xml").arg(kFormGuid)));
    QVERIFY(exists(QStringLiteral("templates/Макет1_%1.meta.txt").arg(kTemplateGuid)));
    QVERIFY(exists(QStringLiteral("templates/Макет1_%1.txt").arg(kTemplateGuid)));

    QFile infoFile(QDir(directory).filePath(QStringLiteral("info.txt")));
    QVERIFY(infoFile.open(QIODevice::ReadOnly));
    const QString info = QString::fromUtf8(infoFile.readAll());
    QVERIFY(info.contains(QStringLiteral("Справочник1")));
    QVERIFY(info.contains(QStringLiteral("Таблиц состава: 5")));

    QFile compositionFile(QDir(directory).filePath(QStringLiteral("composition.txt")));
    QVERIFY(compositionFile.open(QIODevice::ReadOnly));
    const QString composition = QString::fromUtf8(compositionFile.readAll());
    QVERIFY(composition.contains(QStringLiteral("Реквизит1")));
    QVERIFY(composition.contains(QStringLiteral("Реквизит ТЧ")));
    QVERIFY(composition.contains(QStringLiteral("Форма1")));
    QVERIFY(composition.contains(QStringLiteral("Макет1")));

    QFile decodedFile(QDir(directory).filePath(
        QStringLiteral("forms/Форма1_%1.decoded.xml").arg(kFormGuid)));
    QVERIFY(decodedFile.open(QIODevice::ReadOnly));
    QCOMPARE(decodedFile.readAll(), kFormXml);

    QFile moduleFile(QDir(directory).filePath(
        QStringLiteral("modules/01_Модуль объекта.bsl")));
    QVERIFY(moduleFile.open(QIODevice::ReadOnly));
    QVERIFY(moduleFile.readAll().contains(
        QStringLiteral("ПриСозданииОбъекта").toUtf8()));

    bool modulePathFound = false;
    for (const ExtractionFile &file : report.files) {
        if (nativePath(file.path).endsWith(
                QStringLiteral("/modules/01_Модуль объекта.bsl"))) {
            modulePathFound = true;
        }
    }
    QVERIFY(modulePathFound);
}

void TestMetadata::extractsAll()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString error;
    const QString path = temp.filePath(QStringLiteral("synthetic.cf"));
    QVERIFY2(writeFile(path, buildContainer(16, &error), &error), qPrintable(error));

    ConfigurationReader reader;
    QVERIFY2(reader.open(path, &error), qPrintable(error));

    const QString outputRoot = temp.filePath(QStringLiteral("all"));
    Extractor extractor(reader);
    ExtractionReport report;
    QString extractError;
    QVERIFY2(extractor.extractAll(outputRoot, ExtractionOptions(), &report,
                                  &extractError),
             qPrintable(extractError));

    QCOMPARE(report.moduleCount, 1);
    QCOMPARE(report.formCount, 1);
    QCOMPARE(report.templateCount, 1);
    QVERIFY(!report.warnings.isEmpty());
    QVERIFY(report.warnings.join(QStringLiteral("; ")).contains(kMissingGuid));

    QVERIFY(QFileInfo::exists(QDir(outputRoot).filePath(
        QStringLiteral("Справочники/Справочник1/info.txt"))));
    QVERIFY(QFileInfo::exists(QDir(outputRoot).filePath(
        QStringLiteral("Справочники/%1/info.txt").arg(kMissingGuid))));
    QVERIFY(QFileInfo::exists(QDir(outputRoot).filePath(
        QStringLiteral("Языки/Русский/info.txt"))));
    QVERIFY(QFileInfo::exists(QDir(outputRoot).filePath(
        QStringLiteral("Языки/Русский/metadata.txt"))));
}

void TestMetadata::extensionFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString error;
    const QString path = temp.filePath(QStringLiteral("synthetic.cfe"));
    QVERIFY2(writeFile(path, buildExtensionContainer(&error), &error), qPrintable(error));

    ConfigurationReader reader;
    QVERIFY2(reader.open(path, &error), qPrintable(error));

    const Configuration &configuration = reader.configuration();
    QCOMPARE(configuration.kind, ConfigurationKind::Extension);
    QCOMPARE(configuration.name, QStringLiteral("РасширениеТест"));
    QCOMPARE(configuration.synonym, QStringLiteral("Расширение тестовое"));
    QCOMPARE(configuration.configGuid,
             QStringLiteral("abababab-abab-abab-abab-abababababab"));
    QCOMPARE(configuration.objectCount(), 1);
    QCOMPARE(configuration.categories.size(), 1);
    QCOMPARE(configuration.categories.at(0).objects.at(0).name,
             QStringLiteral("Русский"));
    QVERIFY2(configuration.warnings.isEmpty(),
             qPrintable(configuration.warnings.join(QStringLiteral("; "))));
}

void TestMetadata::externalProcessingFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString error;
    const QString path = temp.filePath(QStringLiteral("synthetic.epf"));
    QVERIFY2(writeFile(path, buildExternalContainer(&error), &error), qPrintable(error));

    ConfigurationReader reader;
    QVERIFY2(reader.open(path, &error), qPrintable(error));

    const Configuration &configuration = reader.configuration();
    QCOMPARE(configuration.kind, ConfigurationKind::ExternalProcessing);
    QCOMPARE(configuration.kindTitle, QStringLiteral("Внешняя обработка"));
    QCOMPARE(configuration.objectCount(), 1);

    const QString objectGuid = QStringLiteral("66666666-7777-8888-9999-000000000000");
    MetadataObject *object = reader.configuration().object(objectGuid);
    QVERIFY(object);
    QCOMPARE(object->name, QStringLiteral("Обработка1"));
    QCOMPARE(object->synonym, QStringLiteral("Обработка тестовая"));
    QCOMPARE(object->guid, objectGuid);
    QVERIFY(object->rootObject);

    const MetadataSection *templates = sectionByKind(*object, MetadataSectionKind::Templates);
    QVERIFY(templates);
    QCOMPARE(templates->entries.size(), 1);
    QCOMPARE(templates->entries.at(0).name, QStringLiteral("Макет1"));

    const MetadataSection *forms = sectionByKind(*object, MetadataSectionKind::Forms);
    QVERIFY(forms);
    QCOMPARE(forms->entries.size(), 1);
    QCOMPARE(forms->entries.at(0).name, QStringLiteral("Форма1"));

    const MetadataSection *attributes = sectionByKind(*object, MetadataSectionKind::Attributes);
    QVERIFY(attributes);
    QCOMPARE(attributes->entries.size(), 1);
    QCOMPARE(attributes->entries.at(0).name, QStringLiteral("Реквизит1"));

    const MetadataSection *tabular = sectionByKind(*object, MetadataSectionKind::TabularSections);
    QVERIFY(tabular);
    QVERIFY(tabular->isEmpty());

    ModuleExtractor extractor(reader.container());
    const QVector<ModuleInfo> modules = extractor.extractForObject(*object);
    QCOMPARE(modules.size(), 1);
    QVERIFY(modules.at(0).text.contains(QStringLiteral("ПриСозданииОбъекта")));
}

void TestMetadata::legacyFormatNames()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString catalog15Guid = QStringLiteral("aaaa1111-bbbb-2222-cccc-3333dddd4444");
    const QString config = QStringLiteral(
        "{1,{0,0,%1},\"Конфигурация15\",{1,\"ru\",\"Конфигурация 1.5\"},\"\","
        "{cf4abea6-37b2-11d4-940f-008048da11f9,1,%2}}")
                               .arg(kConfigGuid, catalog15Guid);
    const QString catalog = QStringLiteral(
        "{1,{0,{0,{0,0,%1},\"Справочник15\",{1,\"ru\",\"Справочник 1.5\"},\"\"}}}")
                                .arg(catalog15Guid);

    const QVector<TestElement> elements = {
        {QStringLiteral("root"), QStringLiteral("{2,%1,}").arg(kConfigGuid).toUtf8(), true},
        {QStringLiteral("version"), QStringLiteral("{216,0,{80214,0}}").toUtf8(), false},
        {kConfigGuid, config.toUtf8(), true},
        {catalog15Guid, catalog.toUtf8(), true},
    };

    QString error;
    const QByteArray data = TestContainerBuilder::build(15, elements, &error);
    const QString path = temp.filePath(QStringLiteral("legacy.cf"));
    QVERIFY2(writeFile(path, data, &error), qPrintable(error));

    ConfigurationReader reader;
    QVERIFY2(reader.open(path, &error), qPrintable(error));

    const Configuration &configuration = reader.configuration();
    QCOMPARE(configuration.name, QStringLiteral("Конфигурация15"));
    QCOMPARE(configuration.synonym, QStringLiteral("Конфигурация 1.5"));

    const MetadataObject *catalog15 = configuration.object(catalog15Guid);
    QVERIFY(catalog15);
    QCOMPARE(catalog15->name, QStringLiteral("Справочник15"));
    QCOMPARE(catalog15->synonym, QStringLiteral("Справочник 1.5"));
}

void TestMetadata::lazySections()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QString error;
    const QString path = temp.filePath(QStringLiteral("synthetic.cf"));
    QVERIFY2(writeFile(path, buildContainer(16, &error), &error), qPrintable(error));

    ConfigurationReader reader;
    QVERIFY2(reader.open(path, &error), qPrintable(error));

    MetadataObject *catalog = reader.configuration().object(kCatalogGuid);
    QVERIFY(catalog);
    QVERIFY(!catalog->sectionsLoaded);
    QVERIFY(catalog->sections.isEmpty());

    QVERIFY(reader.loadSections(*catalog));
    QVERIFY(catalog->sectionsLoaded);
    QCOMPARE(catalog->sections.size(), 5);

    QVERIFY(reader.loadSections(*catalog));
    QCOMPARE(catalog->sections.size(), 5);
}

void TestMetadata::missingRootWarning()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    TestElement version;
    version.name = QStringLiteral("version");
    version.payload = QStringLiteral("{216,0,{80327,0}}").toUtf8();
    version.compress = false;

    QString error;
    const QByteArray data = TestContainerBuilder::build16({version}, &error);
    QVERIFY2(!data.isEmpty(), qPrintable(error));

    const QString path = temp.filePath(QStringLiteral("noroot.cf"));
    QVERIFY2(writeFile(path, data, &error), qPrintable(error));

    ConfigurationReader reader;
    QVERIFY2(reader.open(path, &error), qPrintable(error));

    const Configuration &configuration = reader.configuration();
    QVERIFY(configuration.name.isEmpty());
    QVERIFY(configuration.categories.isEmpty());
    QCOMPARE(configuration.formatVersion, 16);
    QVERIFY(!configuration.warnings.isEmpty());
    QVERIFY(configuration.warnings.join(QStringLiteral("; "))
                .contains(QStringLiteral("root")));
}

void TestMetadata::missingConfigurationObject()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    TestElement root;
    root.name = QStringLiteral("root");
    root.payload = QStringLiteral("{2,%1,{1,0,0,0}}").arg(kConfigGuid).toUtf8();

    QString error;
    const QByteArray data = TestContainerBuilder::build15({root}, &error);
    QVERIFY2(!data.isEmpty(), qPrintable(error));

    const QString path = temp.filePath(QStringLiteral("noconfig.cf"));
    QVERIFY2(writeFile(path, data, &error), qPrintable(error));

    ConfigurationReader reader;
    QVERIFY2(reader.open(path, &error), qPrintable(error));

    const Configuration &configuration = reader.configuration();
    QCOMPARE(configuration.configGuid, kConfigGuid);
    QVERIFY(configuration.categories.isEmpty());
    QVERIFY(configuration.warnings.join(QStringLiteral("; "))
                .contains(kConfigGuid));
}

void TestMetadata::realSampleMetadata()
{
    const QString sample = QStringLiteral(
        "C:/work/cpp/Projects/_e8unpack_/release_package/1Cv8_test1.cf");
    if (!QFileInfo::exists(sample))
        QSKIP("Реальный образец 1Cv8_test1.cf не найден");

    ConfigurationReader reader;
    QString error;
    QVERIFY2(reader.open(sample, &error), qPrintable(error));

    const Configuration &configuration = reader.configuration();
    QCOMPARE(configuration.name, QStringLiteral("Конфигурация"));
    QCOMPARE(configuration.version, QStringLiteral("80327"));
    QCOMPARE(configuration.configGuid,
             QStringLiteral("58776e78-c74e-4bf2-8451-ad73247103ce"));
    QCOMPARE(configuration.categories.size(), 2);
    QCOMPARE(configuration.objectCount(), 5);

    const MetadataObject *catalog = nullptr;
    for (const MetadataCategory &category : configuration.categories) {
        for (const MetadataObject &object : category.objects) {
            if (object.name == QStringLiteral("Справочник1_Тестовый"))
                catalog = &object;
        }
    }
    QVERIFY(catalog);
    QVERIFY(catalog->elementFound);
    QVERIFY(catalog->descriptorFound);
    QCOMPARE(catalog->categoryTitle, QStringLiteral("Справочники"));
}

void TestMetadata::realBpDocument()
{
    const QString sample = QStringLiteral("C:/work/cpp/utils/1Cv8_bp.cf");
    if (!QFileInfo::exists(sample))
        QSKIP("Большая конфигурация 1Cv8_bp.cf не найдена");

    ConfigurationReader reader;
    QString error;
    QVERIFY2(reader.open(sample, &error), qPrintable(error));

    const Configuration &configuration = reader.configuration();
    QCOMPARE(configuration.kind, ConfigurationKind::Configuration);
    QVERIFY(configuration.objectCount() > 10000);

    MetadataObject *document =
        reader.configuration().object(QStringLiteral("fcaee2cb-0d08-4617-98c7-044bf5d3c3bd"));
    QVERIFY(document);
    QCOMPARE(document->name, QStringLiteral("АвансовыйОтчет"));
    QCOMPARE(document->typeTitle, QStringLiteral("Документ"));

    QVERIFY(reader.loadSections(*document));
    const MetadataSection *attributes = sectionByKind(
        *document, MetadataSectionKind::Attributes);
    QVERIFY(attributes);
    QVERIFY(attributes->entries.size() >= 20);
    QVERIFY(!attributes->entries.at(0).name.isEmpty());
    QVERIFY(attributes->entries.at(0).guid != attributes->entries.at(0).name);

    const MetadataSection *tabular = sectionByKind(
        *document, MetadataSectionKind::TabularSections);
    QVERIFY(tabular);
    QVERIFY(tabular->entries.size() >= 3);
    QVERIFY(!tabular->entries.at(0).children.isEmpty());

    const MetadataSection *forms = sectionByKind(*document, MetadataSectionKind::Forms);
    QVERIFY(forms);
    QVERIFY(forms->entries.size() >= 5);
    QVERIFY(forms->entries.at(0).name.contains(QStringLiteral("Форма"),
                                               Qt::CaseInsensitive));

    const MetadataSection *templates = sectionByKind(
        *document, MetadataSectionKind::Templates);
    QVERIFY(templates);
    QVERIFY(templates->entries.size() >= 1);

    ModuleExtractor extractor(reader.container());
    const QVector<ModuleInfo> modules = extractor.extractForObject(*document);
    QVERIFY(modules.size() >= 2);
    QVERIFY(modules.at(0).name.contains(QStringLiteral("Модуль объекта")));
    QVERIFY(modules.at(1).name.contains(QStringLiteral("Модуль менеджера")));
    QVERIFY(modules.at(0).text.contains(QStringLiteral("КонецПроцедуры")));
}

void TestMetadata::realExternalProcessing()
{
    const QString sample = QStringLiteral("C:/work/1с/Обработки/UI_Portable.epf");
    if (!QFileInfo::exists(sample))
        QSKIP("Внешняя обработка UI_Portable.epf не найдена");

    ConfigurationReader reader;
    QString error;
    QVERIFY2(reader.open(sample, &error), qPrintable(error));

    const Configuration &configuration = reader.configuration();
    QCOMPARE(configuration.kind, ConfigurationKind::ExternalProcessing);
    QCOMPARE(configuration.objectCount(), 1);
    QCOMPARE(configuration.name,
             QStringLiteral("УИ_ПортативныеУниверсальныеИнструменты"));

    MetadataObject *object = reader.configuration().object(
        QStringLiteral("72a612b7-6b1c-43ed-8f1a-7ff5ae4c19b8"));
    QVERIFY(object);
    QVERIFY(object->rootObject);

    QVERIFY(reader.loadSections(*object));
    const MetadataSection *templates = sectionByKind(
        *object, MetadataSectionKind::Templates);
    QVERIFY(templates);
    QVERIFY(templates->entries.size() >= 100);

    const MetadataSection *forms = sectionByKind(*object, MetadataSectionKind::Forms);
    QVERIFY(forms);
    QVERIFY(forms->entries.size() >= 30);

    ModuleExtractor extractor(reader.container());
    const QVector<ModuleInfo> modules = extractor.extractForObject(*object);
    QVERIFY(!modules.isEmpty());
}

void TestMetadata::realExtension()
{
    const QString sample = QStringLiteral(
        "C:/work/repo_github/yellow_watcher/1c-extention/YellowWatcher.cfe");
    if (!QFileInfo::exists(sample))
        QSKIP("Расширение YellowWatcher.cfe не найдено");

    ConfigurationReader reader;
    QString error;
    QVERIFY2(reader.open(sample, &error), qPrintable(error));

    const Configuration &configuration = reader.configuration();
    QCOMPARE(configuration.kind, ConfigurationKind::Extension);
    QCOMPARE(configuration.name, QStringLiteral("YellowWatcher"));
    QCOMPARE(configuration.synonym, QStringLiteral("Yellow watcher"));
    QCOMPARE(configuration.configGuid,
             QStringLiteral("0486e422-ae70-44b4-8b58-e7952ce247b8"));
    QVERIFY(configuration.objectCount() >= 30);

    MetadataObject *catalog = nullptr;
    for (const MetadataCategory &category : configuration.categories) {
        for (const MetadataObject &object : category.objects) {
            if (object.name == QStringLiteral("YellowWatcher_БазыДанных")) {
                catalog = reader.configuration().object(object.guid);
                break;
            }
        }
        if (catalog)
            break;
    }
    QVERIFY(catalog);
    QVERIFY(catalog->descriptorFound);
    QVERIFY(reader.loadSections(*catalog));
    QVERIFY(!catalog->sections.isEmpty());
}

int runMetadataTests(int argc, char **argv)
{
    TestMetadata test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_metadata.moc"
