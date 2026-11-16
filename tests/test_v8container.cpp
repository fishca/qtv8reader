#include "TestContainerBuilder.h"
#include "core/V8Container.h"

#include <QFile>
#include <QFileInfo>
#include <QTest>

using namespace v8reader;
using namespace v8reader::test;

namespace {

QVector<TestElement> makeElements()
{
    return {
        {QStringLiteral("root"),
         QStringLiteral("{2,58776e78-c74e-4bf2-8451-ad73247103ce,AAAA}").toUtf8(),
         true},
        {QStringLiteral("version"),
         QStringLiteral("{216,0,{80327,0}}").toUtf8(),
         false},
        {QStringLiteral("binary"), QByteArray::fromHex("00010203ff10"), true},
        {QStringLiteral("empty"), QByteArray(), true},
    };
}

} // namespace

class TestV8Container : public QObject
{
    Q_OBJECT

private slots:
    void builder15();
    void builder16();
    void chainedBlocks();
    void compressedChainedBlocks();
    void uncompressedPayload();
    void emptyElement();
    void nestedContainer();
    void looksLikeContainer();
    void corruptHeader();
    void realSamples();
};

void TestV8Container::builder15()
{
    const QVector<TestElement> elements = makeElements();
    QString error;
    const QByteArray data = TestContainerBuilder::build15(elements, &error);
    QVERIFY2(!data.isEmpty(), qPrintable(error));

    V8Container container;
    QVERIFY2(container.openData(data, &error), qPrintable(error));
    QCOMPARE(container.format().version, 15);
    QCOMPARE(container.format().baseOffset, qint64(0));
    QCOMPARE(container.count(), 4);

    const V8Element *root = container.find(QStringLiteral("root"));
    QVERIFY(root);
    QVERIFY(root->hasData());
    QVERIFY(root->creationDate.isValid());
    QCOMPARE(root->creationDate.date(), QDate(2024, 1, 2));

    QByteArray payload;
    bool compressed = false;
    QVERIFY(container.elementPayload(*root, &payload, &compressed, &error));
    QVERIFY(compressed);
    QCOMPARE(payload, elements.at(0).payload);

    const V8Element *version = container.find(QStringLiteral("version"));
    QVERIFY(version);
    QVERIFY(container.elementPayload(*version, &payload, &compressed, &error));
    QVERIFY(!compressed);
    QCOMPARE(payload, elements.at(1).payload);

    const V8Element *binary = container.find(QStringLiteral("binary"));
    QVERIFY(binary);
    QVERIFY(container.elementPayload(*binary, &payload, nullptr, &error));
    QCOMPARE(payload, QByteArray::fromHex("00010203ff10"));
}

void TestV8Container::builder16()
{
    const QVector<TestElement> elements = makeElements();
    QString error;
    const QByteArray data = TestContainerBuilder::build16(elements, &error);
    QVERIFY2(!data.isEmpty(), qPrintable(error));
    QVERIFY(data.size() > 0x1359);

    V8Container container;
    QVERIFY2(container.openData(data, &error), qPrintable(error));
    QCOMPARE(container.format().version, 16);
    QCOMPARE(container.format().baseOffset, qint64(0x1359));
    QCOMPARE(container.format().blockHeaderSize, 55);
    QCOMPARE(container.count(), 4);

    const V8Element *root = container.find(QStringLiteral("root"));
    QVERIFY(root);
    QByteArray payload;
    QVERIFY(container.elementPayload(*root, &payload, nullptr, &error));
    QCOMPARE(payload, elements.at(0).payload);

    const V8Element *empty = container.find(QStringLiteral("empty"));
    QVERIFY(empty);
    QVERIFY(!empty->hasData());
    QVERIFY(container.elementData(*empty, &payload, &error));
    QVERIFY(payload.isEmpty());
}

void TestV8Container::chainedBlocks()
{
    QByteArray payload;
    for (int i = 0; i < 200; ++i)
        payload += QStringLiteral("0123456789").toLatin1();

    TestElement element;
    element.name = QStringLiteral("chained");
    element.payload = payload;
    element.compress = false;
    element.pageSize = 128;

    QString error;
    const QByteArray data = TestContainerBuilder::build16({element}, &error);
    QVERIFY2(!data.isEmpty(), qPrintable(error));

    V8Container container;
    QVERIFY2(container.openData(data, &error), qPrintable(error));
    const V8Element *found = container.find(element.name);
    QVERIFY(found);

    QByteArray result;
    QVERIFY2(container.elementPayload(*found, &result, nullptr, &error), qPrintable(error));
    QCOMPARE(result, payload);
}

void TestV8Container::compressedChainedBlocks()
{
    QByteArray payload;
    for (int i = 0; i < 500; ++i)
        payload += QStringLiteral("Строка %1;").arg(i).toUtf8();

    TestElement element;
    element.name = QStringLiteral("compressed-chained");
    element.payload = payload;
    element.compress = true;
    element.pageSize = 64;

    QString error;
    const QByteArray data = TestContainerBuilder::build15({element}, &error);
    QVERIFY2(!data.isEmpty(), qPrintable(error));

    V8Container container;
    QVERIFY2(container.openData(data, &error), qPrintable(error));
    const V8Element *found = container.find(element.name);
    QVERIFY(found);

    QByteArray result;
    bool compressed = false;
    QVERIFY(container.elementPayload(*found, &result, &compressed, &error));
    QVERIFY(compressed);
    QCOMPARE(result, payload);
}

void TestV8Container::uncompressedPayload()
{
    TestElement element;
    element.name = QStringLiteral("plain");
    element.payload = QStringLiteral("{1,\"текст\"}").toUtf8();
    element.compress = false;

    QString error;
    V8Container container;
    QVERIFY(container.openData(
        TestContainerBuilder::build15({element}, &error), &error));

    const V8Element *found = container.find(element.name);
    QVERIFY(found);
    QByteArray payload;
    bool compressed = true;
    QVERIFY(container.elementPayload(*found, &payload, &compressed, &error));
    QVERIFY(!compressed);
    QCOMPARE(payload, element.payload);
}

void TestV8Container::emptyElement()
{
    TestElement element;
    element.name = QStringLiteral("empty");
    element.payload.clear();

    QString error;
    V8Container container;
    QVERIFY(container.openData(
        TestContainerBuilder::build15({element}, &error), &error));

    const V8Element *found = container.find(element.name);
    QVERIFY(found);
    QVERIFY(!found->hasData());
    QByteArray payload = "not empty";
    QVERIFY(container.elementPayload(*found, &payload, nullptr, &error));
    QVERIFY(payload.isEmpty());
}

void TestV8Container::nestedContainer()
{
    TestElement innerElement;
    innerElement.name = QStringLiteral("root");
    innerElement.payload = QStringLiteral("{2,00000000-0000-0000-0000-000000000001}").toUtf8();

    QString error;
    const QByteArray innerData = TestContainerBuilder::build15({innerElement}, &error);
    QVERIFY2(!innerData.isEmpty(), qPrintable(error));
    QVERIFY(V8Container::looksLikeContainer(innerData));

    TestElement outerElement;
    outerElement.name = QStringLiteral("nested");
    outerElement.payload = innerData;
    outerElement.compress = true;

    V8Container outer;
    QVERIFY2(outer.openData(TestContainerBuilder::build16({outerElement}, &error), &error),
             qPrintable(error));

    const V8Element *element = outer.find(QStringLiteral("nested"));
    QVERIFY(element);

    V8Container nested;
    QVERIFY2(outer.openNested(*element, &nested, &error), qPrintable(error));
    QCOMPARE(nested.format().version, 15);
    QCOMPARE(nested.count(), 1);
    QCOMPARE(nested.at(0)->name, QStringLiteral("root"));
}

void TestV8Container::looksLikeContainer()
{
    const QVector<TestElement> elements = makeElements();
    QVERIFY(V8Container::looksLikeContainer(TestContainerBuilder::build15(elements)));
    QVERIFY(V8Container::looksLikeContainer(TestContainerBuilder::build16(elements)));
    QVERIFY(!V8Container::looksLikeContainer(QByteArray("обычный текст")));
    QVERIFY(!V8Container::looksLikeContainer(QByteArray()));
}

void TestV8Container::corruptHeader()
{
    const QVector<TestElement> elements = makeElements();
    QByteArray data = TestContainerBuilder::build15(elements);
    QVERIFY(!data.isEmpty());

    V8Container container;
    QString error;
    data[16] = 'X';
    QVERIFY(!container.openData(data, &error));
    QVERIFY(!error.isEmpty());

    QByteArray shortData("1234567890");
    QVERIFY(!container.openData(shortData, &error));
}

void TestV8Container::realSamples()
{
    const QString test1 = QStringLiteral(
        "C:/work/cpp/Projects/_e8unpack_/release_package/1Cv8_test1.cf");
    const QString minTest = QStringLiteral(
        "C:/work/cpp/Projects/w8reader/bin/Debug/1Cv8_minTest.cf");

    if (!QFileInfo::exists(test1))
        QSKIP("Реальный образец 1Cv8_test1.cf не найден");

    V8Container container;
    QString error;
    QVERIFY2(container.open(test1, &error), qPrintable(error));
    QCOMPARE(container.format().version, 16);
    QCOMPARE(container.count(), 49);

    const V8Element *root = container.find(QStringLiteral("root"));
    QVERIFY(root);
    QByteArray payload;
    QVERIFY(container.elementPayload(*root, &payload, nullptr, &error));
    QVERIFY(payload.contains("58776e78-c74e-4bf2-8451-ad73247103ce"));

    if (QFileInfo::exists(minTest)) {
        V8Container small;
        QVERIFY2(small.open(minTest, &error), qPrintable(error));
        QCOMPARE(small.format().version, 16);
        QCOMPARE(small.count(), 6);
    }
}

int runV8ContainerTests(int argc, char **argv)
{
    TestV8Container test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_v8container.moc"
