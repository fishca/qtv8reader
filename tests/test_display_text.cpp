#include "core/BinaryText.h"
#include "core/DisplayText.h"

#include <QTest>

using namespace v8reader;

class TestDisplayText : public QObject
{
    Q_OBJECT

private slots:
    void textHeuristics_data();
    void textHeuristics();
    void eventNames();
    void compositionLabel();
    void dateTimeFormat();
};

void TestDisplayText::textHeuristics_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<bool>("expected");

    QTest::newRow("empty") << QByteArray() << false;
    QTest::newRow("utf8") << QByteArray("Привет, 1С") << true;
    QTest::newRow("utf8-bom") << QByteArray("\xef\xbb\xbftext") << true;
    QTest::newRow("utf16le") << QByteArray("A\0B\0C\0D\0", 8) << false;
    QByteArray binaryZeros;
    binaryZeros.append(QByteArray(8, '\0'));
    binaryZeros.append(QByteArray(8, '\xff'));
    QTest::newRow("binary-zeros") << binaryZeros << false;
    QTest::newRow("binary-controls")
        << QByteArray("\x01\x02\x03\x04\x05\x06\x07\x08", 8) << false;
}

void TestDisplayText::textHeuristics()
{
    QFETCH(QByteArray, data);
    QFETCH(bool, expected);
    QCOMPARE(isProbablyText(data), expected);
    QVERIFY(isProbablyText(QByteArray("A\0B\0C\0D\0", 8), true));
}

void TestDisplayText::eventNames()
{
    QCOMPARE(eventDisplayName(QStringLiteral("Posting_ОбработкаПроведения")),
             QStringLiteral("ОбработкаПроведения"));
    QCOMPARE(eventDisplayName(QStringLiteral("ОбработкаПроведения")),
             QStringLiteral("ОбработкаПроведения"));
    QCOMPARE(eventDisplayName(QStringLiteral("1_Обработка")),
             QStringLiteral("1_Обработка"));
}

void TestDisplayText::compositionLabel()
{
    MetadataEntry entry;
    entry.typeTitle = QStringLiteral("Реквизит");
    entry.typeText = QStringLiteral("Строка(50)");
    QCOMPARE(compositionTypeLabel(entry), QStringLiteral("Реквизит · Строка(50)"));

    entry.typeText.clear();
    QCOMPARE(compositionTypeLabel(entry), QStringLiteral("Реквизит"));
}

void TestDisplayText::dateTimeFormat()
{
    QCOMPARE(dateTimeText(QDateTime()), QStringLiteral("—"));
    const QDateTime value(QDate(2024, 1, 2), QTime(3, 4, 5));
    QCOMPARE(dateTimeText(value), QStringLiteral("02.01.2024 03:04:05"));
}

int runDisplayTextTests(int argc, char **argv)
{
    TestDisplayText test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_display_text.moc"
