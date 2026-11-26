#include "TestContainerBuilder.h"
#include "cli/CliApplication.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

using namespace v8reader;
using namespace v8reader::test;

namespace {

const QString kConfigGuid = QStringLiteral("11111111-1111-1111-1111-111111111111");
const QString kCatalogGuid = QStringLiteral("22222222-2222-2222-2222-222222222222");

QString configText()
{
    return QStringLiteral(
               "{3,{1,0,%1},\"ТестКонфигурация\",{0,\"ru\",\"Тестовая конфигурация\"},"
               "\"Комментарий\","
               "{cf4abea6-37b2-11d4-940f-008048da11f9,1,%2}}")
        .arg(kConfigGuid, kCatalogGuid);
}

QString catalogText()
{
    return QStringLiteral(
               "{3,{1,0,%1},\"Справочник1\",{0,\"ru\",\"Справочник тестовый\"},"
               "\"Комментарий\","
               "{cf4abea7-37b2-11d4-940f-008048da11f9,1,"
               "{3,{1,0,aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa},"
               "\"Реквизит1\",{0,\"ru\",\"Реквизит 1\"},\"\"}}}")
        .arg(kCatalogGuid);
}

QString writeFixture(const QString &directory, QString *error)
{
    const QVector<TestElement> elements = {
        {QStringLiteral("root"),
         QStringLiteral("{2,%1,{1,0,0,0}}").arg(kConfigGuid).toUtf8(), true},
        {QStringLiteral("version"), QByteArray("{216,0,{80327,0}}"), false},
        {kConfigGuid, configText().toUtf8(), true},
        {kCatalogGuid, catalogText().toUtf8(), true},
    };
    const QByteArray data = TestContainerBuilder::build(16, elements, error);
    if (data.isEmpty())
        return QString();

    const QString path = QDir(directory).filePath(QStringLiteral("synthetic.cf"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return QString();
    }
    if (file.write(data) != data.size()) {
        if (error)
            *error = QStringLiteral("Не удалось записать тестовый контейнер");
        return QString();
    }
    return path;
}

struct CliResult
{
    int code = -1;
    QString out;
    QString err;
};

CliResult runOptions(const CliApplication::Options &options)
{
    CliResult result;
    QTextStream out(&result.out);
    QTextStream err(&result.err);
    out.setEncoding(QStringConverter::Utf8);
    err.setEncoding(QStringConverter::Utf8);

    CliApplication application;
    result.code = application.run(options, out, err);
    out.flush();
    err.flush();
    return result;
}

CliResult runCli(const QString &command, const QString &path,
                 const QString &argument = {}, const QString &output = {},
                 bool raw = false)
{
    CliApplication::Options options;
    options.command = command;
    options.path = path;
    options.argument = argument;
    options.output = output;
    options.raw = raw;
    return runOptions(options);
}

CliResult runView(const QString &path, const QString &object = {},
                  const QString &module = {}, bool json = true)
{
    CliApplication::Options options;
    options.command = QStringLiteral("view");
    options.path = path;
    options.argument = object;
    options.module = module;
    options.json = json;
    return runOptions(options);
}

} // namespace

class TestCli : public QObject
{
    Q_OBJECT

private slots:
    void info();
    void list();
    void metadata();
    void composition();
    void tree();
    void missingFile();
    void extractObject();
    void kindQualifiedObject();
    void viewJson();
    void viewObject();
    void viewText();
    void viewUnknownObject();
    void viewUnknownModule();
};

void TestCli::info()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    const QString path = writeFixture(temp.path(), &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));

    const CliResult result = runCli(QStringLiteral("info"), path);
    QCOMPARE(result.code, 0);
    QVERIFY(result.out.contains(QStringLiteral("Конфигурация: ТестКонфигурация")));
    QVERIFY(result.out.contains(QStringLiteral("Объектов: 1")));
    QVERIFY(result.err.isEmpty());
}

void TestCli::list()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    const QString path = writeFixture(temp.path(), &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));

    const CliResult result = runCli(QStringLiteral("list"), path);
    QCOMPARE(result.code, 0);
    QVERIFY(result.out.contains(QStringLiteral("[Справочники] 1 объектов")));
    QVERIFY(result.out.contains(QStringLiteral("Справочник1")));
}

void TestCli::metadata()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    const QString path = writeFixture(temp.path(), &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));

    const CliResult result = runCli(QStringLiteral("metadata"), path,
                                    QStringLiteral("Справочник1"));
    QCOMPARE(result.code, 0);
    QVERIFY(result.out.contains(QStringLiteral("Справочник1")));
}

void TestCli::composition()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    const QString path = writeFixture(temp.path(), &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));

    const CliResult result = runCli(QStringLiteral("composition"), path,
                                    QStringLiteral("Справочник1"));
    QCOMPARE(result.code, 0);
    QVERIFY(result.out.contains(QStringLiteral("Реквизит1")));
}

void TestCli::tree()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    const QString path = writeFixture(temp.path(), &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));

    const CliResult result = runCli(QStringLiteral("tree"), path);
    QCOMPARE(result.code, 0);
    QVERIFY(result.out.contains(QStringLiteral("root")));
    QVERIFY(result.out.contains(QStringLiteral("synthetic")) == false);
}

void TestCli::missingFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = QDir(temp.path()).filePath(QStringLiteral("missing.cf"));

    const CliResult result = runCli(QStringLiteral("info"), path);
    QCOMPARE(result.code, 1);
    QVERIFY(result.err.contains(QStringLiteral("не найден")));
}

void TestCli::extractObject()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    const QString path = writeFixture(temp.path(), &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));

    const QString output = QDir(temp.path()).filePath(QStringLiteral("out"));
    const CliResult result = runCli(QStringLiteral("extract"), path,
                                    QStringLiteral("Справочник1"), output);
    QCOMPARE(result.code, 0);
    QVERIFY(result.out.contains(QStringLiteral("сохранён")));
    const QDir outputDir(output);
    QVERIFY(!outputDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
}

// Уточнение вида в запросе: имена объектов в больших конфигурациях
// повторяются («ВидыКонтактнойИнформации» есть и у справочника, и у
// обработки), поэтому «Справочник.Имя» и «Справочники.Имя» должны находить
// объект, а чужой вид — честно сообщать, что объект не найден.
void TestCli::kindQualifiedObject()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    const QString path = writeFixture(temp.path(), &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));

    for (const QString &query : {QStringLiteral("Справочник.Справочник1"),
                                 QStringLiteral("Справочники.Справочник1")}) {
        const CliResult result = runCli(QStringLiteral("metadata"), path, query);
        QCOMPARE(result.code, 0);
        QVERIFY(result.out.contains(QStringLiteral("Справочник1")));
    }

    const CliResult wrongKind =
        runCli(QStringLiteral("metadata"), path, QStringLiteral("Документ.Справочник1"));
    QCOMPARE(wrongKind.code, 1);
    QVERIFY(wrongKind.err.contains(QStringLiteral("не найден")));

    // Неизвестный префикс не считается видом: запрос ищется как есть.
    const CliResult unknownPrefix =
        runCli(QStringLiteral("metadata"), path, QStringLiteral("Нечто.Справочник1"));
    QCOMPARE(unknownPrefix.code, 1);
}

// Снимок для панели чтения: схема, сведения о файле и дерево метаданных.
void TestCli::viewJson()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    const QString path = writeFixture(temp.path(), &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));

    const CliResult result = runView(path);
    QCOMPARE(result.code, 0);
    QVERIFY2(result.err.isEmpty(), qPrintable(result.err));

    QJsonParseError parseError{};
    const QJsonDocument document =
        QJsonDocument::fromJson(result.out.toUtf8(), &parseError);
    QVERIFY2(parseError.error == QJsonParseError::NoError, qPrintable(parseError.errorString()));
    QVERIFY(document.isObject());

    const QJsonObject root = document.object();
    QCOMPARE(root.value(QStringLiteral("schema")).toInt(), 1);
    QVERIFY(!root.value(QStringLiteral("appVersion")).toString().isEmpty());

    const QJsonObject file = root.value(QStringLiteral("file")).toObject();
    QCOMPARE(file.value(QStringLiteral("kind")).toString(), QStringLiteral("configuration"));
    QCOMPARE(file.value(QStringLiteral("name")).toString(), QStringLiteral("ТестКонфигурация"));
    QCOMPARE(file.value(QStringLiteral("objectCount")).toInt(), 1);
    QVERIFY(!file.value(QStringLiteral("path")).toString().isEmpty());
    QVERIFY(file.value(QStringLiteral("size")).toDouble() > 0);

    const QJsonArray categories = root.value(QStringLiteral("categories")).toArray();
    QCOMPARE(categories.size(), 1);
    const QJsonObject category = categories.at(0).toObject();
    QCOMPARE(category.value(QStringLiteral("title")).toString(), QStringLiteral("Справочники"));
    QCOMPARE(category.value(QStringLiteral("count")).toInt(), 1);
    const QJsonArray objects = category.value(QStringLiteral("objects")).toArray();
    QCOMPARE(objects.size(), 1);
    QCOMPARE(objects.at(0).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("Справочник1"));

    QVERIFY(root.value(QStringLiteral("object")).isNull());
    QVERIFY(root.value(QStringLiteral("content")).isNull());
}

// Состав объекта: разделы с реквизитами и список модулей.
void TestCli::viewObject()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    const QString path = writeFixture(temp.path(), &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));

    const CliResult result = runView(path, QStringLiteral("Справочник1"));
    QCOMPARE(result.code, 0);

    const QJsonObject root = QJsonDocument::fromJson(result.out.toUtf8()).object();
    const QJsonObject object = root.value(QStringLiteral("object")).toObject();
    QCOMPARE(object.value(QStringLiteral("name")).toString(), QStringLiteral("Справочник1"));
    QCOMPARE(object.value(QStringLiteral("typeTitle")).toString(), QStringLiteral("Справочник"));
    QVERIFY(object.contains(QStringLiteral("modules")));

    bool foundRequisite = false;
    for (const QJsonValue &value : object.value(QStringLiteral("sections")).toArray()) {
        const QJsonObject section = value.toObject();
        if (section.value(QStringLiteral("kind")).toString() != QStringLiteral("attributes"))
            continue;
        for (const QJsonValue &entry : section.value(QStringLiteral("entries")).toArray()) {
            if (entry.toObject().value(QStringLiteral("name")).toString()
                == QStringLiteral("Реквизит1")) {
                foundRequisite = true;
            }
        }
    }
    QVERIFY2(foundRequisite, "в разделе attributes нет реквизита Реквизит1");
}

// Без --json команда остаётся человекочитаемой.
void TestCli::viewText()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    const QString path = writeFixture(temp.path(), &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));

    const CliResult result = runView(path, {}, {}, false);
    QCOMPARE(result.code, 0);
    QVERIFY(result.out.contains(QStringLiteral("Конфигурация: ТестКонфигурация")));
    QVERIFY(result.out.contains(QStringLiteral("Объектов: 1")));
    QVERIFY(!result.out.contains(QLatin1Char('{')));
}

void TestCli::viewUnknownObject()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    const QString path = writeFixture(temp.path(), &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));

    const CliResult result = runView(path, QStringLiteral("НетТакого"));
    QCOMPARE(result.code, 1);
    QVERIFY(result.err.contains(QStringLiteral("не найден")));
    QVERIFY(result.out.isEmpty());
}

// У синтетического справочника модулей нет: запрос модуля — честная ошибка,
// а не пустой текст в content.
void TestCli::viewUnknownModule()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString error;
    const QString path = writeFixture(temp.path(), &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));

    const CliResult result =
        runView(path, QStringLiteral("Справочник1"), QStringLiteral("МодульОбъекта"));
    QCOMPARE(result.code, 1);
    QVERIFY(result.err.contains(QStringLiteral("не найден")));
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qtv8reader-cli-tests"));
    // Как и у настоящего v8reader_cli (main.cpp), версия приложения попадает в
    // машиночитаемый вывод команды view.
    QCoreApplication::setApplicationVersion(QStringLiteral("test-cli"));

    TestCli test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_cli.moc"
