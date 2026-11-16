#include "core/ConfigurationReader.h"
#include "core/FormDocument.h"
#include "core/FormReader.h"
#include "core/V8Text.h"
#include "core/V8Container.h"

#include <QTest>

#include <QFileInfo>

using namespace v8reader;

namespace {

QByteArray makeSyntheticForm()
{
    const QString text = QStringLiteral(
        "{4,"
        "{50,0,"
        // группа: горизонтальная, внутри вертикальная группа с полем и кнопка
        "{22,{1,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"5\","
        "\"ГруппаШапка\",{1,1,{\"ru\",\"Шапка\"}},{29,\"1\",\"0\",\"0\",\"0\"},"
        "{22,{2,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"5\","
        "\"ГруппаЛевая\",{1,1,{\"ru\",\"Левая часть\"}},{29,\"0\",\"0\",\"0\",\"0\"},"
        "{37,{3,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"0\","
        "\"Поле1\",{1,1,{\"ru\",\"Поле 1\"}}},"
        "},"
        "{31,{4,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\","
        "\"Кнопка1\",{1,1,{\"ru\",\"Кнопка\"}}}"
        "}"
        "},"
        "\"Процедура Тест()\\nКонецПроцедуры\","
        "{4,\"2\","
        "{9,{\"1\"},\"0\",\"Атрибут1\",{1,1,{\"ru\",\"Атрибут 1\"}},{\"Pattern\",{\"S\"}}},"
        "{9,{\"2\"},\"0\",\"Атрибут2\",{1,1,{\"ru\",\"Атрибут 2\"}},"
        "{\"Pattern\",{\"#\",\"43a53a27-d6d5-46dc-9adc-51ccb785e3e5\"}}}"
        "},"
        "{0,0},"
        "{0,\"1\",{9,{\"1\"},\"Команда1\",{1,1,{\"ru\",\"Команда\"}}}}"
        "}");
    return text.toUtf8();
}

const FormItem *findItemByName(const QVector<FormItem> &items, const QString &name)
{
    for (const FormItem &item : items) {
        if (item.name == name)
            return &item;
        if (const FormItem *nested = findItemByName(item.children, name))
            return nested;
    }
    return nullptr;
}

const FormItem *findItemById(const QVector<FormItem> &items, int id)
{
    for (const FormItem &item : items) {
        if (item.id == id)
            return &item;
        if (const FormItem *nested = findItemById(item.children, id))
            return nested;
    }
    return nullptr;
}

class TestForm : public QObject
{
    Q_OBJECT

private slots:
    void parseSynthetic();
    void notAForm();
    void realBpForm();
    void realBpListFormGeometry();
};

void TestForm::parseSynthetic()
{
    const QByteArray data = makeSyntheticForm();
    {
    }
    {
        V8TextNode root;
        V8TextError parseError;
        const QString text = V8Text::decode(data);
        const bool parsed = V8Text::parse(text, root, &parseError);
        QVERIFY2(parsed, qPrintable(parseError.toString()));
    }
    QVERIFY(FormReader::looksLikeForm(data));

    FormDocument document;
    QString error;
    QVERIFY2(FormReader::parseBytes(data, &document, &error), qPrintable(error));
    QVERIFY(document.valid);
    QVERIFY(document.moduleText.contains(QStringLiteral("Процедура Тест")));
    QCOMPARE(document.items.size(), 1);
    QCOMPARE(document.attributes.size(), 2);
    QCOMPARE(document.commands.size(), 1);
    QCOMPARE(document.commands.first().name, QStringLiteral("Команда1"));
    QCOMPARE(document.attributes.first().synonym, QStringLiteral("Атрибут 1"));
    QCOMPARE(document.attributes.first().typeHint, QStringLiteral("Строка"));
    QCOMPARE(document.attributes.at(1).typeHint, QStringLiteral("Ссылка"));

    const FormItem &group = document.items.first();
    QCOMPARE(group.type, FormItem::Type::Group);
    QCOMPARE(group.name, QStringLiteral("ГруппаШапка"));
    QCOMPARE(group.synonym, QStringLiteral("Шапка"));
    QCOMPARE(group.orientation, 1);
    QCOMPARE(group.children.size(), 2);
    QCOMPARE(group.children.first().orientation, 0);
    QVERIFY(findItemByName(document.items, QStringLiteral("Поле1")) != nullptr);
    QVERIFY(findItemByName(document.items, QStringLiteral("Кнопка1")) != nullptr);
    QCOMPARE(document.fieldCount(), 1);
    QVERIFY(document.itemCount() >= 4);
}

void TestForm::notAForm()
{
    QVERIFY(!FormReader::looksLikeForm(QByteArrayLiteral("MOXCEL")));
    QVERIFY(!FormReader::looksLikeForm(QByteArrayLiteral("{1,2,3}")));
    FormDocument document;
    QVERIFY(!FormReader::parseBytes(QByteArrayLiteral("{1,2,3}"), &document, nullptr));
}

void TestForm::realBpForm()
{
    const QString sample = QStringLiteral("C:/work/cpp/utils/1Cv8_bp.cf");
    if (!QFileInfo::exists(sample))
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
    const MetadataSection *forms = object->section(MetadataSectionKind::Forms);
    QVERIFY(forms);
    QVERIFY(!forms->entries.isEmpty());

    const MetadataEntry *formEntry = nullptr;
    for (const MetadataEntry &entry : forms->entries) {
        if (entry.name == QStringLiteral("ФормаДокумента")) {
            formEntry = &entry;
            break;
        }
    }
    QVERIFY(formEntry);

    const V8Element *element =
        reader.container().find(formEntry->guid + QStringLiteral(".0"));
    QVERIFY(element);

    QByteArray payload;
    QVERIFY2(reader.container().elementPayload(*element, &payload, nullptr, &error),
             qPrintable(error));
    QVERIFY(FormReader::looksLikeForm(payload));

    FormDocument document;
    QVERIFY2(FormReader::parseBytes(payload, &document, &error), qPrintable(error));
    QVERIFY(document.valid);
    QVERIFY(document.items.size() > 5);
    QVERIFY(document.moduleText.size() > 100000);
    QVERIFY(document.attributes.size() >= 40);
    QVERIFY(document.commands.size() >= 5);
    QVERIFY(findItemByName(document.items, QStringLiteral("ГруппаШапка")) != nullptr);
    QVERIFY(findItemByName(document.items, QStringLiteral("Номер")) != nullptr);
    QCOMPARE(findItemByName(document.items, QStringLiteral("ГруппаШапка"))->orientation, 1);

    // id 214 в Form.xml имеет Visible=false, id 321 — TitleLocation=None.
    const FormItem *hidden = findItemById(document.items, 214);
    QVERIFY(hidden);
    QVERIFY(!hidden->visible);
    const FormItem *noTitle = findItemById(document.items, 321);
    QVERIFY(noTitle);
    QVERIFY(!noTitle->showTitle);
}

void TestForm::realBpListFormGeometry()
{
    const QString sample = QStringLiteral("C:/work/cpp/utils/1Cv8_bp.cf");
    if (!QFileInfo::exists(sample))
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
    const MetadataSection *forms = object->section(MetadataSectionKind::Forms);
    QVERIFY(forms);

    const MetadataEntry *formEntry = nullptr;
    for (const MetadataEntry &entry : forms->entries) {
        if (entry.name == QStringLiteral("ФормаСписка")) {
            formEntry = &entry;
            break;
        }
    }
    QVERIFY(formEntry);

    const V8Element *element =
        reader.container().find(formEntry->guid + QStringLiteral(".0"));
    QVERIFY(element);
    QByteArray payload;
    QVERIFY2(reader.container().elementPayload(*element, &payload, nullptr, &error),
             qPrintable(error));

    FormDocument document;
    QVERIFY2(FormReader::parseBytes(payload, &document, &error), qPrintable(error));

    const FormItem *date = findItemByName(document.items, QStringLiteral("Дата"));
    QVERIFY(date);
    QCOMPARE(date->widthUnits, 7);
    QCOMPARE(date->horizontalStretch, 0);

    const FormItem *sum = findItemByName(document.items, QStringLiteral("СуммаДокумента"));
    QVERIFY(sum);
    QCOMPARE(sum->widthUnits, 10);

    const FormItem *bannerText = findItemByName(document.items, QStringLiteral("ТекстБаннера"));
    QVERIFY(bannerText);
    QCOMPARE(bannerText->heightUnits, 3);
    QCOMPARE(bannerText->horizontalStretch, 1);
    QVERIFY(!bannerText->showTitle);

    const FormItem *bannerGroup = findItemByName(document.items,
        QStringLiteral("ГруппаБаннерОжидание"));
    QVERIFY(bannerGroup);
    QCOMPARE(bannerGroup->widthUnits, 24);
}

} // namespace

int runFormTests(int argc, char **argv)
{
    TestForm test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_form.moc"
