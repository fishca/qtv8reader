#include "TestContainerBuilder.h"
#include "cli/CliApplication.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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

CliResult runCli(const QString &command, const QString &path,
                 const QString &argument = {}, const QString &output = {},
                 bool raw = false)
{
    CliResult result;
    QTextStream out(&result.out);
    QTextStream err(&result.err);
    out.setEncoding(QStringConverter::Utf8);
    err.setEncoding(QStringConverter::Utf8);

    CliApplication::Options options;
    options.command = command;
    options.path = path;
    options.argument = argument;
    options.output = output;
    options.raw = raw;

    CliApplication application;
    result.code = application.run(options, out, err);
    out.flush();
    err.flush();
    return result;
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

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qtv8reader-cli-tests"));

    TestCli test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_cli.moc"
