#include "core/ConfigurationReader.h"
#include "core/MxlDocument.h"
#include "core/MxlReader.h"
#include "core/V8Container.h"
#include "TestPaths.h"

#include <QTest>

#include <QCoreApplication>
#include <QFileInfo>

using namespace v8reader;
using namespace v8reader::mxl;

namespace {

// Собирает минимальный корректный MXL8: заголовок + тело со строками,
// форматами, шрифтами, цветами, линиями и объединениями.
QByteArray makeSyntheticMxl()
{
    const QString body = QStringLiteral(
        "{8,1,12,"
        "{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},"
        // блок строк: 2 строки
        "2,"
        "0,0,2,"
        "0,{2,1,{\"S\",\"Привет\"}},"
        "1,{2,1,{\"S\",\"Мир\"}},"
        "1,0,1,"
        "0,{16,2,{1,1,{\"ru\",\"Итого\"}}},"
        // группа колонок: 2 колонки, ширины через форматы 1 и 2
        "{2,0,00000000-0000-0000-0000-000000000000,2,0,1,1,2},"
        "2,"          // число строк (дубль)
        "0,"          // доп. групп колонок
        "0,"          // карта строка-группа
        "0,0,"        // графические объекты: нет
        "{1,{0,0,1,0,0}}," // объединение A1:B1
        // именованные области: 2 области строк
        "{2,"
        "\"Шапка\",{\"1\",{\"1\",\"-1\",\"1\",\"-1\",\"5\","
        "00000000-0000-0000-0000-000000000000},\"0\"},"
        "\"Строка\",{\"1\",{\"1\",\"-1\",\"7\",\"-1\",\"7\","
        "00000000-0000-0000-0000-000000000000},\"0\"}},"
        // параметры печати: A4 альбомная, масштаб 80%, поля 10/12 мм
        "{{\"0\",\"7\",\"0\",{\"S\",\"A4\"},"
        "\"1\",{\"N\",\"2\"},"
        "\"2\",{\"N\",\"80\"},"
        "\"6\",{\"N\",\"1000\"},"
        "\"7\",{\"N\",\"1200\"},"
        "\"8\",{\"N\",\"1000\"},"
        "\"9\",{\"N\",\"1200\"}}},"
        // таблица форматов: 2 записи
        "2,"
        "{65,0,120}," // шрифт 0, высота 120 (1/288")
        "{65,1,0},"
        // таблица шрифтов: 2 записи
        "2,"
        "{7,0,575,80,0,0,0,400,0,0,0,204,0,0,0,34,\"Arial\",0,100},"
        "{7,0,575,140,0,0,0,700,0,0,0,204,0,0,0,34,\"Arial\",0,100},"
        // таблица цветов
        "2,"
        "{3,0,{16711680}},"
        "{3,3,{-3}},"
        // таблица линий: 2 записи
        "{2,(1,{4,0,{0},1,1,0,00000000-0000-0000-0000-000000000000,0},0),"
        "(1,{4,0,{0},2,1,0,00000000-0000-0000-0000-000000000000,0},0)}"
        "}");

    QByteArray data;
    data.append("MOXCEL", 6);
    data.append('\0');
    data.append('\x08');
    data.append('\0');
    data.append('\x01');
    data.append('\0');
    data.append('\x0C');
    data.append('\0');
    data.append('\xEF');
    data.append('\xBB');
    data.append('\xBF');
    data.append(body.toUtf8());
    return data;
}

} // namespace

class TestMxl : public QObject
{
    Q_OBJECT

private slots:
    void parseSynthetic();
    void rejectsSeventhFormat();
    void rejectsNonMxl();
    void parsesBodyWithoutHeader();
    void lookupHelpers();
    void realBpTemplate();
};

void TestMxl::parseSynthetic()
{
    const QByteArray data = makeSyntheticMxl();
    QVERIFY(MxlReader::looksLikeMxl(data));

    Document document;
    QString error;
    QVERIFY2(MxlReader::parseBytes(data, &document, &error), qPrintable(error));
    QVERIFY(document.valid);
    QCOMPARE(document.versionMajor, 8);
    QCOMPARE(document.versionMinor, 1);
    QCOMPARE(document.versionSub, 12);

    QCOMPARE(document.rowCount(), 2);
    QCOMPARE(document.rows.at(0).index, 0);
    QCOMPARE(document.rows.at(0).cells.size(), 2);
    QCOMPARE(document.rows.at(1).cells.size(), 1);

    const Cell *first = document.cellAt(0, 0);
    QVERIFY(first);
    QCOMPARE(first->text, QStringLiteral("Привет"));
    QCOMPARE(first->format, 1);

    const Cell *localized = document.cellAt(1, 0);
    QVERIFY(localized);
    QCOMPARE(localized->text, QStringLiteral("Итого"));
    QCOMPARE(localized->format, 2);

    QCOMPARE(document.formats.size(), 2);
    const CellFormat *format = document.formatAt(1);
    QVERIFY(format);
    QCOMPARE(format->font, 0);
    QCOMPARE(format->height, 120);

    QCOMPARE(document.fonts.size(), 2);
    QCOMPARE(document.fonts.at(1).family, QStringLiteral("Arial"));
    QCOMPARE(document.fonts.at(1).sizeTenthPt, 140);
    QVERIFY(document.fonts.at(1).bold);

    QCOMPARE(document.colors.size(), 2);
    quint32 rgb = 0;
    QVERIFY(document.colorAt(0, &rgb));
    QCOMPARE(rgb, 0x00FF0000u); // COLORREF: B<<16 | G<<8 | R
    QVERIFY(!document.colorAt(1, &rgb)); // авто-цвет

    QCOMPARE(document.lines.size(), 2);
    QCOMPARE(document.lines.at(0).kind, 1);
    QCOMPARE(document.lines.at(1).kind, 2);
    QCOMPARE(document.lines.at(1).thickness, 1);

    QCOMPARE(document.merges.size(), 1);
    QCOMPARE(document.merges.at(0).left, 0);
    QCOMPARE(document.merges.at(0).top, 0);
    QCOMPARE(document.merges.at(0).right, 1);
    QCOMPARE(document.defaultGroup.columns.size(), 2);
    QCOMPARE(document.defaultGroup.columns.at(1).first, 1);
    QCOMPARE(document.defaultGroup.columns.at(1).second, 2);

    QVERIFY(document.mergeAt(0, 1) != nullptr);
    QVERIFY(document.mergeAt(1, 0) == nullptr);
    QCOMPARE(document.maxColumn(), 1);
    QCOMPARE(document.maxRowIndex(), 1);

    QCOMPARE(document.namedAreas.size(), 2);
    QCOMPARE(document.namedAreas.at(0).name, QStringLiteral("Шапка"));
    QCOMPARE(document.namedAreas.at(0).type, NamedArea::Type::Rows);
    QCOMPARE(document.namedAreas.at(0).top, 1);
    QCOMPARE(document.namedAreas.at(0).bottom, 5);
    QVERIFY(!document.namedAreas.at(0).hasColumns());
    QCOMPARE(document.namedAreas.at(1).name, QStringLiteral("Строка"));
    QCOMPARE(document.namedAreas.at(1).top, 7);
    QCOMPARE(document.namedAreas.at(1).bottom, 7);

    QVERIFY(document.printSettings.valid);
    QCOMPARE(document.printSettings.pageSize, QStringLiteral("A4"));
    QCOMPARE(document.printSettings.orientation, 2);
    QCOMPARE(document.printSettings.scale, 80);
    QCOMPARE(document.printSettings.topMarginMm, 10.0);
    QCOMPARE(document.printSettings.bottomMarginMm, 12.0);
    QCOMPARE(document.printSettings.leftMarginMm, 10.0);
    QCOMPARE(document.printSettings.rightMarginMm, 12.0);
}

void TestMxl::rejectsSeventhFormat()
{
    QByteArray data;
    data.append("MOXCEL", 6);
    data.append(QByteArray(5, '\0'));
    data.append('\x07');
    data.append(QByteArray(16, '\0'));

    Document document;
    QString error;
    QVERIFY(!MxlReader::parseBytes(data, &document, &error));
    QVERIFY(error.contains(QStringLiteral("7.7")));
}

void TestMxl::rejectsNonMxl()
{
    Document document;
    QString error;
    QVERIFY(!MxlReader::parseBytes(QByteArray("not a spreadsheet"), &document, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!MxlReader::looksLikeMxl(QByteArray("not a spreadsheet")));
}

void TestMxl::parsesBodyWithoutHeader()
{
    // Тело без заголовка допускается (полезно для тестов и внешних вызовов).
    const QByteArray full = makeSyntheticMxl();
    Document document;
    QString error;
    QVERIFY2(MxlReader::parseBytes(full.mid(16), &document, &error), qPrintable(error));
    QCOMPARE(document.rowCount(), 2);
    QCOMPARE(document.cellAt(0, 0)->text, QStringLiteral("Привет"));
}

void TestMxl::lookupHelpers()
{
    const QByteArray data = makeSyntheticMxl();
    Document document;
    QVERIFY(MxlReader::parseBytes(data, &document, nullptr));

    QVERIFY(document.formatAt(0) == nullptr);
    QVERIFY(document.formatAt(3) == nullptr);
    QCOMPARE(document.groupForRow(0).columns.size(), 2);
    QVERIFY(document.cellAt(5, 5) == nullptr);
}

void TestMxl::realBpTemplate()
{
    const QString sample = test::corpusFile(QStringLiteral("1Cv8_bp.cf"));
    if (sample.isEmpty())
        QSKIP("Большая конфигурация 1Cv8_bp.cf не найдена");

    ConfigurationReader reader;
    QString error;
    QVERIFY2(reader.open(sample, &error), qPrintable(error));

    MetadataObject *object = nullptr;
    for (MetadataCategory &category : reader.configuration().categories) {
        for (MetadataObject &candidate : category.objects) {
            if (candidate.name.compare(QStringLiteral("АвансовыйОтчет"),
                                       Qt::CaseInsensitive) == 0) {
                object = &candidate;
                break;
            }
        }
        if (object)
            break;
    }
    QVERIFY(object);

    reader.loadSections(*object);
    const MetadataSection *templates =
        object->section(MetadataSectionKind::Templates);
    QVERIFY(templates);
    QVERIFY(!templates->entries.isEmpty());

    const MetadataEntry &entry = templates->entries.first();
    const V8Element *element = reader.container().find(entry.guid + QStringLiteral(".0"));
    QVERIFY(element);

    QByteArray payload;
    QVERIFY2(reader.container().elementPayload(*element, &payload, nullptr, &error),
             qPrintable(error));
    QVERIFY(MxlReader::looksLikeMxl(payload));

    Document document;
    QVERIFY2(MxlReader::parseBytes(payload, &document, &error), qPrintable(error));
    QCOMPARE(document.rowCount(), 74);
    QCOMPARE(document.maxColumn() + 1, 25);
    QVERIFY(document.formats.size() > 100);
    QCOMPARE(document.fonts.size(), 5);
    QVERIFY(document.merges.size() > 50);
    QVERIFY(document.colors.size() >= 3);
    QVERIFY(document.lines.size() >= 2);
    QCOMPARE(document.namedAreas.size(), 4);
    QCOMPARE(document.namedAreas.at(0).name, QStringLiteral("Подвал"));
    QVERIFY(document.namedAreas.at(0).hasRows());
    QVERIFY(document.printSettings.valid);
    QCOMPARE(document.printSettings.topMarginMm, 10.0);
    QCOMPARE(document.printSettings.bottomMarginMm, 10.0);
}
int runMxlTests(int argc, char **argv)
{
    TestMxl test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_mxl.moc"
