#include "core/ConfigurationReader.h"
#include "core/FormBindings.h"
#include "core/FormDocument.h"
#include "core/FormReader.h"
#include "core/FormWebModel.h"
#include "core/V8Text.h"
#include "core/V8Container.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
        "\"ГруппаШапка\",{1,1,{\"ru\",\"Шапка\"}},"
        // {29}: ориентация 1, выделение 0 (нет), заголовок 1, раскрываемая.
        "{29,\"1\",\"0\",\"0\",\"1\",{\"0\"},{\"1\",\"0\"},{\"Pattern\"},\"\","
        "{\"3\",\"4\",{\"0\"}},\"1\",\"1\",\"1\",\"0\"},"
        "{22,{2,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"5\","
        "\"ГруппаЛевая\",{1,1,{\"ru\",\"Левая часть\"}},{29,\"0\",\"0\",\"0\",\"0\"},"
        "{37,{3,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"0\","
        "\"Поле1\",{1,1,{\"ru\",\"Поле 1\"}}},"
        "{37,{5,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"0\","
        "\"Поле2\",\"3\",{1,1,{\"ru\",\"Поле 2\"}}},"
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
    void parsedGroupAndTitleModes();
    void webModel();
    void notAForm();
    void realBpForm();
    void realBpListFormGeometry();
    void resolvedFieldTypes();
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
    QCOMPARE(document.fieldCount(), 2);
    QVERIFY(document.itemCount() >= 4);
}

// TitleLocation и режимы групп разобраны из узла макета {29,...}:
// раскрываемая группа, положение заголовка «сверху» и «авто».
void TestForm::parsedGroupAndTitleModes()
{
    FormDocument document;
    QString error;
    QVERIFY2(FormReader::parseBytes(makeSyntheticForm(), &document, &error),
             qPrintable(error));

    const FormItem *group = findItemByName(document.items,
                                           QStringLiteral("ГруппаШапка"));
    QVERIFY(group);
    QVERIFY(group->showTitle);
    QCOMPARE(group->behavior, FormGroupBehavior::Collapsible);
    QCOMPARE(group->representation, FormGroupRepresentation::None);

    const FormItem *top = findItemByName(document.items, QStringLiteral("Поле2"));
    QVERIFY(top);
    QCOMPARE(top->titleLocation, FormTitleLocation::Top);
    QVERIFY(top->showTitle);

    const FormItem *autoField = findItemByName(document.items,
                                               QStringLiteral("Поле1"));
    QVERIFY(autoField);
    QCOMPARE(autoField->titleLocation, FormTitleLocation::Auto);
}

// Модель формы для точного вида: структура JSON, командная панель и метаданные
// владельца, которые ожидает браузерный рендерер.
void TestForm::webModel()
{
    FormDocument document;
    QString error;
    QVERIFY2(FormReader::parseBytes(makeSyntheticForm(), &document, &error),
             qPrintable(error));
    document.name = QStringLiteral("ФормаДокумента");
    document.synonym = QStringLiteral("Форма документа");
    document.ownerTitle = QStringLiteral("Документ.АвансовыйОтчет");
    document.ownerSynonym = QStringLiteral("Авансовый отчет");
    document.runtimeDocumentCommands = true;
    document.titles.insert(QStringLiteral("Поле1"), QStringLiteral("Поле 1"));

    const QJsonObject model =
        QJsonDocument::fromJson(formWebModelJson(document)).object();
    QCOMPARE(model.value(QStringLiteral("tag")).toString(), QStringLiteral("Form"));
    QCOMPARE(model.value(QStringLiteral("name")).toString(),
             QStringLiteral("ФормаДокумента"));

    const QJsonObject properties = model.value(QStringLiteral("properties")).toObject();
    QCOMPARE(properties.value(QStringLiteral("Title")).toString(),
             QStringLiteral("Авансовый отчет"));
    QCOMPARE(properties.value(QStringLiteral("UsePostingMode")).toString(),
             QStringLiteral("Auto"));

    const QJsonArray roots = model.value(QStringLiteral("childItemsRoot")).toArray();
    QVERIFY(!roots.isEmpty());
    const QJsonObject group = roots.first().toObject();
    QCOMPARE(group.value(QStringLiteral("tag")).toString(),
             QStringLiteral("UsualGroup"));
    QCOMPARE(group.value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("Group")).toString(),
             QStringLiteral("Horizontal"));

    const QJsonObject bar = model.value(QStringLiteral("autoCommandBar")).toObject();
    QCOMPARE(bar.value(QStringLiteral("tag")).toString(),
             QStringLiteral("AutoCommandBar"));
    const QJsonArray barItems = bar.value(QStringLiteral("childItems")).toArray();
    QVERIFY(barItems.size() >= 2);
    // Порядок как в клиенте 1С: «Провести и закрыть» — первая и по умолчанию.
    const QJsonObject firstButton = barItems.first().toObject();
    QCOMPARE(firstButton.value(QStringLiteral("name")).toString(),
             QStringLiteral("_stdPostAndClose"));
    QCOMPARE(firstButton.value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("DefaultButton")).toString(),
             QStringLiteral("true"));
    bool hasWrite = false;
    for (const QJsonValue &value : barItems) {
        const QJsonObject button = value.toObject();
        if (button.value(QStringLiteral("name")).toString() == QStringLiteral("_stdWrite"))
            hasWrite = true;
    }
    QVERIFY(hasWrite);

    // Атрибуты: «Объект» (основной реквизит) плюс два из формы.
    QCOMPARE(model.value(QStringLiteral("attributes")).toArray().size(), 3);
    QCOMPARE(model.value(QStringLiteral("commands")).toArray().size(), 1);

    const QJsonObject objectMeta = model.value(QStringLiteral("objectMeta")).toObject();
    QCOMPARE(objectMeta.value(QStringLiteral("kindTag")).toString(),
             QStringLiteral("Document"));
    QCOMPARE(objectMeta.value(QStringLiteral("name")).toString(),
             QStringLiteral("АвансовыйОтчет"));
    QCOMPARE(objectMeta.value(QStringLiteral("captions")).toObject()
                 .value(QStringLiteral("Поле1")).toString(),
             QStringLiteral("Поле 1"));
}

void TestForm::notAForm()
{
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
    QCOMPARE(noTitle->titleLocation, FormTitleLocation::None);

    // TitleLocation различает не только «показывать/нет»: у полей формы
    // встречаются Left и Right (id 7 и 35 в Form.xml).
    const FormItem *left = findItemById(document.items, 7);
    QVERIFY(left);
    QCOMPARE(left->titleLocation, FormTitleLocation::Left);
    const FormItem *right = findItemById(document.items, 35);
    QVERIFY(right);
    QCOMPARE(right->titleLocation, FormTitleLocation::Right);

    // Группа шапки — обычная, без выделения и без заголовка.
    const FormItem *header = findItemById(document.items, 233);
    QVERIFY(header);
    QCOMPARE(header->representation, FormGroupRepresentation::None);
    QCOMPARE(header->behavior, FormGroupBehavior::Usual);
    QVERIFY(!header->showTitle);

    // Кнопка «Показать контекстные новости» в Form.xml живёт только
    // в подменю (LocationInCommandBar=InAdditionalSubmenu) — на панель
    // её не выводим.
    const FormItem *submenuButton = findItemByName(
        document.items, QStringLiteral("ФормаПоказатьКонтекстныеНовости"));
    QVERIFY(submenuButton);
    QVERIFY(!submenuButton->onCommandBar);
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

    // Растяжение групп (по Form.xml): 146 — H=false, 160 — V=true,
    // 110 — H=true/V=false.
    const FormItem *group146 = findItemById(document.items, 146);
    QVERIFY(group146);
    QCOMPARE(group146->horizontalStretch, 0);
    const FormItem *group160 = findItemById(document.items, 160);
    QVERIFY(group160);
    QCOMPARE(group160->verticalStretch, 1);
    const FormItem *group110 = findItemById(document.items, 110);
    QVERIFY(group110);
    QCOMPARE(group110->horizontalStretch, 1);
    QCOMPARE(group110->verticalStretch, 0);
}

// Путь к данным в бинарной форме разобран и расшифрован по метаданным:
// у поля появляются заголовок реквизита, тип и ширина в символах.
void TestForm::resolvedFieldTypes()
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
            if (candidate.name == QStringLiteral("АвансовыйОтчет")) {
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

    FormBindingResolver resolver;
    resolver.addObject(*object);
    QVERIFY(!resolver.isEmpty());

    FormDocument document;
    QVERIFY2(FormReader::parseBytes(payload, &document, &error, &resolver),
             qPrintable(error));

    // Поле «Организация» — путь «Объект.Организация», тип-ссылка.
    const FormItem *organization = findItemByName(document.items,
                                                  QStringLiteral("Организация"));
    QVERIFY(organization);
    QVERIFY(!organization->dataPath.isEmpty());
    QCOMPARE(organization->dataPathText, QStringLiteral("Объект.Организация"));
    QCOMPARE(organization->valueKind, FormValueKind::Reference);
    QVERIFY2(organization->typeText.contains(QStringLiteral("СправочникСсылка")),
             qPrintable(organization->typeText));
    QVERIFY(!organization->dataTitle.isEmpty());
    QVERIFY(organization->valueChars > 0);

    // Дата — путь на стандартный реквизит (отрицательный код сегмента).
    const FormItem *date = findItemById(document.items, 238);
    QVERIFY(date);
    QCOMPARE(date->valueKind, FormValueKind::Date);
    QVERIFY(date->valueChars > 0);

    // Строковое поле номера документа.
    const FormItem *number = findItemById(document.items, 236);
    QVERIFY(number);
    QCOMPARE(number->valueKind, FormValueKind::String);

    // Колонка табличной части: заголовок из метаданных («Сумма»).
    const FormItem *sum = findItemByName(document.items, QStringLiteral("БилетыСумма"));
    QVERIFY(sum);
    QCOMPARE(sum->valueKind, FormValueKind::Number);
    QCOMPARE(sum->dataTitle, QStringLiteral("Сумма"));
}

} // namespace

int runFormTests(int argc, char **argv){
    TestForm test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_form.moc"
