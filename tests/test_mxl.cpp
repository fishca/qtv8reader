#include "core/ConfigurationReader.h"
#include "core/MxlDocument.h"
#include "core/MxlHtmlExporter.h"
#include "core/MxlReader.h"
#include "core/V8Container.h"
#include "SyntheticMxl.h"
#include "TestPaths.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include <QCoreApplication>
#include <QFileInfo>

using namespace v8reader;
using namespace v8reader::mxl;

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
    void exporterHtml();
    void exporterEscaping();
    void exporterWithoutFormats();
    void exporterEmpty();
    void exporterSummary();
};

void TestMxl::parseSynthetic()
{
    const QByteArray data = test::syntheticMxl();
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
    const QByteArray full = test::syntheticMxl();
    Document document;
    QString error;
    QVERIFY2(MxlReader::parseBytes(full.mid(16), &document, &error), qPrintable(error));
    QCOMPARE(document.rowCount(), 2);
    QCOMPARE(document.cellAt(0, 0)->text, QStringLiteral("Привет"));
}

void TestMxl::lookupHelpers()
{
    const QByteArray data = test::syntheticMxl();
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

// HTML-экспорт синтетического макета: объединение, параметры печати,
// именованные области и масштаб страницы.
void TestMxl::exporterHtml()
{
    Document document;
    QString error;
    QVERIFY2(MxlReader::parseBytes(test::syntheticMxl(), &document, &error),
             qPrintable(error));

    const QString html = MxlHtmlExporter::toHtml(document);
    QVERIFY(html.startsWith(QStringLiteral("<!DOCTYPE html>")));
    QVERIFY(html.contains(QStringLiteral("<table class=\"mxl\"")));
    QVERIFY(html.contains(QStringLiteral("colspan=\"2\""))); // A1:B1
    QVERIFY(!html.contains(QStringLiteral("rowspan=\"2\"")));
    QVERIFY(html.contains(QStringLiteral("Привет")));
    QVERIFY(html.contains(QStringLiteral("Итого")));
    QVERIFY(html.contains(QStringLiteral(
        "@page{size:A4 landscape;margin:10mm 12mm 12mm 10mm}")));
    QVERIFY(html.contains(QStringLiteral("zoom:80%"))); // масштаб печати
    QVERIFY(html.contains(QStringLiteral("@media print")));
    QVERIFY(html.contains(QStringLiteral("class=\"area\"")));
    QVERIFY(html.contains(QStringLiteral("Шапка")));
    // Область строк 7..7 за пределами двух строк не рисуется.
    QVERIFY(!html.contains(QStringLiteral("Строка")));
    QVERIFY(!html.contains(QStringLiteral(".sheet{zoom:")));

    HtmlExportOptions options;
    options.title = QStringLiteral("Макет1");
    options.zoomPercent = 150;
    const QString zoomed = MxlHtmlExporter::toHtml(document, options);
    QVERIFY(zoomed.contains(QStringLiteral("<title>Макет1</title>")));
    QVERIFY(zoomed.contains(QStringLiteral(".sheet{zoom:150%}")));

    HtmlExportOptions noAreas;
    noAreas.namedAreas = false;
    QVERIFY(!MxlHtmlExporter::toHtml(document, noAreas)
                 .contains(QStringLiteral("class=\"area\"")));
}

// Экранирование текста, шрифты, цвета, рамки и выравнивание ячейки.
void TestMxl::exporterEscaping()
{
    Document document;
    document.valid = true;

    Row row;
    row.index = 0;
    Cell cell;
    cell.format = 1;
    cell.text = QStringLiteral("<a&\"b'>\nстрока");
    row.cells.append({0, cell});
    document.rows.append(row);

    CellFormat format;
    format.font = 0;
    format.textColor = 0;
    format.bgColor = 1;
    format.pattern = 0;
    format.patternColor = 1;
    format.borderLeft = 0; // 0-базный индекс в таблице линий
    format.bordersColor = 0;
    format.hAlign = 6;
    format.vAlign = 24;
    format.textControl = 3;
    document.formats.append(format);

    FontSpec font;
    font.family = QStringLiteral("Arial");
    font.sizeTenthPt = 140;
    font.bold = true;
    font.italic = true;
    font.underline = true;
    document.fonts.append(font);

    Color red;
    red.kind = Color::Kind::Rgb;
    red.rgb = 0x0000FF; // COLORREF: R в младшем байте
    Color green;
    green.kind = Color::Kind::Rgb;
    green.rgb = 0x00FF00;
    document.colors.append(red);
    document.colors.append(green);

    LineStyle line;
    line.kind = 1;
    line.thickness = 2;
    document.lines.append(line);

    const QString html = MxlHtmlExporter::toHtml(document, HtmlExportOptions{});
    QVERIFY(html.contains(QStringLiteral("&lt;a&amp;&quot;b&#39;&gt;<br>строка")));
    QVERIFY(html.contains(QStringLiteral("font-family:'Arial'")));
    QVERIFY(html.contains(QStringLiteral("font-size:19px")));
    QVERIFY(html.contains(QStringLiteral("font-weight:bold")));
    QVERIFY(html.contains(QStringLiteral("font-style:italic")));
    QVERIFY(html.contains(QStringLiteral("text-decoration:underline")));
    QVERIFY(html.contains(QStringLiteral("color:#ff0000")));
    QVERIFY(html.contains(QStringLiteral("background:#00ff00")));
    QVERIFY(html.contains(QStringLiteral("border-left:2px solid #ff0000")));
    QVERIFY(html.contains(QStringLiteral("text-align:center")));
    QVERIFY(html.contains(QStringLiteral("vertical-align:middle")));
    QVERIFY(html.contains(QStringLiteral("white-space:pre-wrap")));
}

// Ячейка с format=0 и пустые таблицы форматов не должны ронять экспорт.
void TestMxl::exporterWithoutFormats()
{
    Document document;
    document.valid = true;

    Row row;
    row.index = 0;
    Cell cell;
    cell.format = 0;
    cell.text = QStringLiteral("Без формата");
    row.cells.append({0, cell});
    document.rows.append(row);

    const QString html = MxlHtmlExporter::toHtml(document);
    QVERIFY(html.contains(QStringLiteral("Без формата")));
    QVERIFY(html.contains(QStringLiteral("font-family:Arial, Helvetica, sans-serif")));
    QVERIFY(html.contains(QStringLiteral("font-size:13px")));
    QVERIFY(!html.contains(QStringLiteral("@page")));
}

void TestMxl::exporterEmpty()
{
    Document document;
    const QString html = MxlHtmlExporter::toHtml(document);
    QVERIFY(html.startsWith(QStringLiteral("<!DOCTYPE html>")));
    QVERIFY(html.contains(QStringLiteral("Макет не содержит данных")));
    QVERIFY(!html.contains(QStringLiteral("<table")));
    QVERIFY(html.endsWith(QStringLiteral("</html>\n")));
}

// Сводка для mxl --json: числа, области и параметры печати.
void TestMxl::exporterSummary()
{
    Document document;
    QString error;
    QVERIFY2(MxlReader::parseBytes(test::syntheticMxl(), &document, &error),
             qPrintable(error));

    const QJsonObject summary = MxlHtmlExporter::summarize(document);
    QCOMPARE(summary.value(QStringLiteral("schema")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("rows")).toInt(), 2);
    QCOMPARE(summary.value(QStringLiteral("columns")).toInt(), 2);
    QCOMPARE(summary.value(QStringLiteral("formats")).toInt(), 2);
    QCOMPARE(summary.value(QStringLiteral("fonts")).toInt(), 2);
    QCOMPARE(summary.value(QStringLiteral("merges")).toInt(), 1);
    QCOMPARE(summary.value(QStringLiteral("namedAreas")).toArray().size(), 2);

    const QJsonObject area =
        summary.value(QStringLiteral("namedAreas")).toArray().at(0).toObject();
    QCOMPARE(area.value(QStringLiteral("name")).toString(), QStringLiteral("Шапка"));
    QCOMPARE(area.value(QStringLiteral("type")).toString(), QStringLiteral("rows"));
    QCOMPARE(area.value(QStringLiteral("top")).toInt(), 1);
    QCOMPARE(area.value(QStringLiteral("bottom")).toInt(), 5);
    QVERIFY(!area.contains(QStringLiteral("left")));

    const QJsonObject print = summary.value(QStringLiteral("print")).toObject();
    QVERIFY(print.value(QStringLiteral("valid")).toBool());
    QCOMPARE(print.value(QStringLiteral("pageSize")).toString(), QStringLiteral("A4"));
    QCOMPARE(print.value(QStringLiteral("orientation")).toString(),
             QStringLiteral("landscape"));
    QCOMPARE(print.value(QStringLiteral("scale")).toInt(), 80);
    const QJsonObject margins = print.value(QStringLiteral("marginsMm")).toObject();
    QCOMPARE(margins.value(QStringLiteral("top")).toDouble(), 10.0);
    QCOMPARE(margins.value(QStringLiteral("right")).toDouble(), 12.0);
    QCOMPARE(margins.value(QStringLiteral("bottom")).toDouble(), 12.0);
    QCOMPARE(margins.value(QStringLiteral("left")).toDouble(), 10.0);

    const QJsonObject empty = MxlHtmlExporter::summarize(Document{});
    QCOMPARE(empty.value(QStringLiteral("rows")).toInt(), 0);
    QVERIFY(!empty.value(QStringLiteral("print")).toObject()
                 .value(QStringLiteral("valid")).toBool());
}

int runMxlTests(int argc, char **argv)
{
    TestMxl test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_mxl.moc"
