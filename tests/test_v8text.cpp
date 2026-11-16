#include "core/V8Text.h"

#include <QCoreApplication>
#include <QTest>

using namespace v8reader;

int runV8ContainerTests(int argc, char **argv);
int runMetadataTests(int argc, char **argv);

class TestV8Text : public QObject
{
    Q_OBJECT

private slots:
    void decodeUtf8();
    void decodeUtf16Le();
    void decodeUtf16Be();
    void decodeWindows1251();
    void parseSimpleTree();
    void parseQuotedStrings();
    void parseEmptyList();
    void parseErrors_data();
    void parseErrors();
    void serializeRoundTrip();
    void guidHelpers();
    void base64Value();
};

void TestV8Text::decodeUtf8()
{
    const QString source = QStringLiteral("Конфигурация 1С");
    QString encoding;
    QString error;
    const QString result = V8Text::decode(source.toUtf8(), &encoding, &error);
    QCOMPARE(result, source);
    QCOMPARE(encoding, QStringLiteral("UTF-8"));
    QVERIFY(error.isEmpty());
}

void TestV8Text::decodeUtf16Le()
{
    const QString source = QStringLiteral("Справочник");
    QByteArray data("\xff\xfe", 2);
    for (QChar ch : source) {
        data.append(static_cast<char>(ch.unicode() & 0xff));
        data.append(static_cast<char>((ch.unicode() >> 8) & 0xff));
    }

    QString encoding;
    QCOMPARE(V8Text::decode(data, &encoding, nullptr), source);
    QVERIFY(encoding.startsWith(QStringLiteral("UTF-16LE")));
}

void TestV8Text::decodeUtf16Be()
{
    const QString source = QStringLiteral("Документ");
    QByteArray data("\xfe\xff", 2);
    for (QChar ch : source) {
        data.append(static_cast<char>((ch.unicode() >> 8) & 0xff));
        data.append(static_cast<char>(ch.unicode() & 0xff));
    }

    QString encoding;
    QCOMPARE(V8Text::decode(data, &encoding, nullptr), source);
    QVERIFY(encoding.startsWith(QStringLiteral("UTF-16BE")));
}

void TestV8Text::decodeWindows1251()
{
    // "Привет" в windows-1251.
    const QByteArray data = QByteArray::fromHex("cff0e8e2e5f2");
    QString encoding;
    QString error;
    const QString result = V8Text::decode(data, &encoding, &error);
    QCOMPARE(result, QStringLiteral("Привет"));
    QCOMPARE(encoding, QStringLiteral("Windows-1251"));
    QVERIFY(error.isEmpty());
}

void TestV8Text::parseSimpleTree()
{
    const QString text = QStringLiteral(
        "{1,\"Справочник1\",{2,0,e51aa14c-a150-43cc-bfb2-f1dc2747e427}}");
    V8TextNode root;
    V8TextError error;
    QVERIFY2(V8Text::parse(text, root, &error), qPrintable(error.toString()));
    QVERIFY(root.isList());
    QCOMPARE(root.children.size(), 3);
    QCOMPARE(root.children.at(0).value, QStringLiteral("1"));
    QCOMPARE(root.children.at(1).type, V8TextNode::Type::String);
    QCOMPARE(root.children.at(1).value, QStringLiteral("Справочник1"));
    QVERIFY(root.children.at(2).isList());
    QCOMPARE(root.children.at(2).children.size(), 3);
    QVERIFY(root.child(2)->child(2)->isGuid());
    QCOMPARE(root.child(2)->child(2)->normalizedGuid(),
             QStringLiteral("e51aa14c-a150-43cc-bfb2-f1dc2747e427"));
}

void TestV8Text::parseQuotedStrings()
{
    const QString text = QStringLiteral("{\"Он сказал \"\"привет\"\"\",\"\"}");
    V8TextNode root;
    V8TextError error;
    QVERIFY2(V8Text::parse(text, root, &error), qPrintable(error.toString()));
    QCOMPARE(root.children.size(), 2);
    QCOMPARE(root.children.at(0).value, QStringLiteral("Он сказал \"привет\""));
    QCOMPARE(root.children.at(1).value, QString());
}

void TestV8Text::parseEmptyList()
{
    V8TextNode root;
    QVERIFY(V8Text::parse(QStringLiteral("{}"), root, nullptr));
    QVERIFY(root.isList());
    QVERIFY(root.isEmpty());
    QCOMPARE(root.children.size(), 0);
}

void TestV8Text::parseErrors_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("messagePart");

    QTest::newRow("missing brace") << QStringLiteral("{1,2") << QStringLiteral("закрыта");
    QTest::newRow("missing quote") << QStringLiteral("{\"abc}") << QStringLiteral("кавычка");
    QTest::newRow("extra data") << QStringLiteral("{} {}") << QStringLiteral("Лишние");
    QTest::newRow("unexpected end") << QStringLiteral("{1,") << QStringLiteral("закрыта");
}

void TestV8Text::parseErrors()
{
    QFETCH(QString, text);
    QFETCH(QString, messagePart);

    V8TextNode root;
    V8TextError error;
    QVERIFY(!V8Text::parse(text, root, &error));
    QVERIFY(error.isValid());
    QVERIFY2(error.message.contains(messagePart),
             qPrintable(QStringLiteral("message=%1").arg(error.message)));
    QVERIFY(error.line >= 1);
    QVERIFY(error.column >= 1);
}

void TestV8Text::serializeRoundTrip()
{
    const QString text = QStringLiteral(
        "{1,{2,0}, \"Строка с \"\"кавычками\"\"\",{0}}");
    V8TextNode root;
    QVERIFY(V8Text::parse(text, root, nullptr));

    const QString serialized = V8Text::serialize(root, true);
    V8TextNode second;
    V8TextError error;
    QVERIFY2(V8Text::parse(serialized, second, &error), qPrintable(error.toString()));
    QCOMPARE(second.children.size(), root.children.size());
    QCOMPARE(second.children.at(2).value, QStringLiteral("Строка с \"кавычками\""));
}

void TestV8Text::guidHelpers()
{
    QVERIFY(V8Text::isGuid(QStringLiteral("e51aa14c-a150-43cc-bfb2-f1dc2747e427")));
    QVERIFY(V8Text::isGuid(QStringLiteral("E51AA14C-A150-43CC-BFB2-F1DC2747E427")));
    QVERIFY(!V8Text::isGuid(QStringLiteral("e51aa14c-a150-43cc-bfb2-f1dc2747e42")));
    QVERIFY(!V8Text::isGuid(QStringLiteral("zzzzzzzz-a150-43cc-bfb2-f1dc2747e427")));

    QCOMPARE(V8Text::normalizeGuid(
                 QStringLiteral("  E51AA14C-A150-43CC-BFB2-F1DC2747E427  ")),
             QStringLiteral("e51aa14c-a150-43cc-bfb2-f1dc2747e427"));
    QVERIFY(V8Text::normalizeGuid(QStringLiteral("not a guid")).isEmpty());
}

void TestV8Text::base64Value()
{
    const QByteArray expected = QStringLiteral("Тестовые данные").toUtf8();
    const QString value = QStringLiteral("#base64:")
        + QString::fromLatin1(expected.toBase64())
        + QStringLiteral("\n   ");

    QByteArray decoded;
    QVERIFY(V8Text::decodeBase64Value(value, &decoded));
    QCOMPARE(decoded, expected);

    // Вставка переводов строк внутрь base64.
    QString wrapped = QStringLiteral("#base64:");
    const QString encoded = QString::fromLatin1(expected.toBase64());
    for (int i = 0; i < encoded.size(); i += 8) {
        wrapped += encoded.mid(i, 8);
        wrapped += QStringLiteral("\r\n");
    }
    QVERIFY(V8Text::decodeBase64Value(wrapped, &decoded));
    QCOMPARE(decoded, expected);

    QVERIFY(!V8Text::decodeBase64Value(QStringLiteral("обычный текст"), &decoded));
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qtv8reader-tests"));

    int status = 0;
    {
        TestV8Text test;
        status |= QTest::qExec(&test, argc, argv);
    }
    status |= runV8ContainerTests(argc, argv);
    status |= runMetadataTests(argc, argv);
    return status;
}

#include "test_v8text.moc"
