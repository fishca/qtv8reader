#include "core/ConfigurationReader.h"
#include "core/FormBindings.h"
#include "core/FormDocument.h"
#include "core/FormOwner.h"
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

// Узел модели командной панели по имени на любой глубине вложенности.
QJsonObject findModelNode(const QJsonArray &items, const QString &name)
{
    for (const QJsonValue &value : items) {
        const QJsonObject node = value.toObject();
        if (node.value(QStringLiteral("name")).toString() == name)
            return node;
        const QJsonObject nested =
            findModelNode(node.value(QStringLiteral("childItems")).toArray(), name);
        if (!nested.isEmpty())
            return nested;
    }
    return QJsonObject();
}

int modelNodeIndex(const QJsonArray &items, const QString &name)
{
    for (int i = 0; i < items.size(); ++i) {
        if (items.at(i).toObject().value(QStringLiteral("name")).toString() == name)
            return i;
    }
    return -1;
}

class TestForm : public QObject
{
    Q_OBJECT

private slots:
    void parseSynthetic();
    void parsedGroupAndTitleModes();
    void webModel();
    void commandBarStandardCommands();
    void notAForm();
    void realBpForm();
    void realBpListFormGeometry();
    void resolvedFieldTypes();
    void ownerMetadataAndTableColumns();
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

// Командная панель формы документа: платформенные команды выводятся одними
// пиктограммами, а «Создать на основании» стоит после группы важных команд.
void TestForm::commandBarStandardCommands()
{
    FormDocument document;
    document.runtimeDocumentCommands = true;
    document.ownerTitle = QStringLiteral("Документ.АвансовыйОтчет");

    FormItem bar;
    bar.type = FormItem::Type::Group;
    bar.groupKind = 9;  // командная панель
    bar.name = QStringLiteral("ФормаКоманднаяПанель");

    FormItem panel;
    panel.type = FormItem::Type::Group;
    panel.groupKind = 6;  // группа кнопок
    panel.name = QStringLiteral("ГруппаКнопкиКоманднойПанели");

    FormItem important;
    important.type = FormItem::Type::Group;
    important.groupKind = 6;
    important.name = QStringLiteral("ГруппаВажныеКоманды");
    important.synonym = QStringLiteral("Важные команды");

    FormItem motions;
    motions.type = FormItem::Type::Button;
    motions.name = QStringLiteral("ФормаОбщаяКомандаПоказатьДвиженияДокумента");
    important.children.append(motions);

    FormItem global;
    global.type = FormItem::Type::Group;
    global.groupKind = 6;
    global.name = QStringLiteral("ГруппаГлобальныеКоманды");

    FormItem structure;
    structure.type = FormItem::Type::Button;
    structure.name = QStringLiteral("ОбщаяКомандаСтруктураПодчиненности");
    FormItem history;
    history.type = FormItem::Type::Button;
    history.name = QStringLiteral("ОбщаяКомандаИсторияИзменений");
    global.children.append(structure);
    global.children.append(history);

    panel.children.append(important);
    panel.children.append(global);
    bar.children.append(panel);
    document.items.append(bar);

    const QJsonObject model =
        QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonArray barItems = model.value(QStringLiteral("autoCommandBar")).toObject()
                                    .value(QStringLiteral("childItems")).toArray();
    QCOMPARE(barItems.first().toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("_stdPostAndClose"));
    QCOMPARE(modelNodeIndex(barItems, QStringLiteral("_global_create_based_on")), -1);

    const QJsonObject panelNode =
        findModelNode(barItems, QStringLiteral("ГруппаКнопкиКоманднойПанели"));
    QVERIFY(!panelNode.isEmpty());
    const QJsonArray panelItems = panelNode.value(QStringLiteral("childItems")).toArray();
    const int importantIndex =
        modelNodeIndex(panelItems, QStringLiteral("ГруппаВажныеКоманды"));
    const int basedOnIndex =
        modelNodeIndex(panelItems, QStringLiteral("_global_create_based_on"));
    const int globalIndex =
        modelNodeIndex(panelItems, QStringLiteral("ГруппаГлобальныеКоманды"));
    QVERIFY(importantIndex >= 0);
    QVERIFY(basedOnIndex > importantIndex);
    QVERIFY(basedOnIndex < globalIndex);

    const QJsonObject motionsProperties =
        findModelNode(barItems, QStringLiteral("ФормаОбщаяКомандаПоказатьДвиженияДокумента"))
            .value(QStringLiteral("properties")).toObject();
    QCOMPARE(motionsProperties.value(QStringLiteral("Representation")).toString(),
             QStringLiteral("Picture"));
    QCOMPARE(motionsProperties.value(QStringLiteral("Picture")).toString(),
             QStringLiteral("StdPicture.ДебетКредит"));
    QCOMPARE(motionsProperties.value(QStringLiteral("Title")).toString(),
             QStringLiteral("Движения документа"));

    const QJsonObject historyProperties =
        findModelNode(barItems, QStringLiteral("ОбщаяКомандаИсторияИзменений"))
            .value(QStringLiteral("properties")).toObject();
    QCOMPARE(historyProperties.value(QStringLiteral("Representation")).toString(),
             QStringLiteral("Picture"));
    QCOMPARE(historyProperties.value(QStringLiteral("Picture")).toString(),
             QStringLiteral("StdPicture.DataHistory"));
    QCOMPARE(historyProperties.value(QStringLiteral("Title")).toString(),
             QStringLiteral("История изменений"));
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

// Метаданные владельца для рендерера: типы реквизитов с квалификаторами,
// полные пути колонок табличных частей и группы колонок.
void TestForm::ownerMetadataAndTableColumns()
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
    FormDocument document;
    QVERIFY2(FormReader::parseBytes(payload, &document, &error, &resolver),
             qPrintable(error));
    applyOwnerMetadata(*object, &document);

    QVERIFY(document.ownerMeta.valid);
    QCOMPARE(document.ownerMeta.kindTag, QStringLiteral("Document"));
    QCOMPARE(document.ownerMeta.types.value(QStringLiteral("Организация")),
             QStringLiteral("cfg:CatalogRef.Организации"));
    QVERIFY(document.ownerMeta.stringLen.contains(QStringLiteral("Комментарий")));
    QVERIFY(document.ownerMeta.numberQ.contains(QStringLiteral("СуммаДокумента")));

    // Путь колонки табличной части разобран до конца: имя табличной части
    // встало на место неопознанного сегмента.
    const FormItem *advance =
        findItemByName(document.items, QStringLiteral("ВыданныеАвансыДокументАванса"));
    QVERIFY(advance);
    QCOMPARE(advance->dataPathText,
             QStringLiteral("Объект.ВыданныеАвансы.ДокументАванса"));
    // Тип колонки — составной (три вида документов-оснований).
    QCOMPARE(advance->valueKind, FormValueKind::Composite);
    QVERIFY2(advance->typeText.contains(QStringLiteral("ДокументСсылка")),
             qPrintable(advance->typeText));

    // Стандартная колонка «Номер строки» получает платформенное имя LineNumber:
    // по нему рендерер рисует заголовок «N», а не имя элемента.
    const FormItem *lineNumber =
        findItemByName(document.items, QStringLiteral("ВыданныеАвансыНомерСтроки"));
    QVERIFY(lineNumber);
    QCOMPARE(lineNumber->dataPathText,
             QStringLiteral("Объект.ВыданныеАвансы.LineNumber"));

    // Группа колонок таблицы (двухуровневый заголовок).
    const FormItem *columnGroup =
        findItemByName(document.items, QStringLiteral("ГруппаТоварыКодАртикул"));
    QVERIFY(columnGroup);
    QCOMPARE(columnGroup->type, FormItem::Type::Group);
    QCOMPARE(columnGroup->groupKind, 2);

    const QJsonObject model =
        QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonObject objectMeta = model.value(QStringLiteral("objectMeta")).toObject();
    QVERIFY(objectMeta.value(QStringLiteral("types")).toObject()
                .contains(QStringLiteral("Организация")));
    QVERIFY(objectMeta.value(QStringLiteral("numberQ")).toObject()
                .contains(QStringLiteral("СуммаДокумента")));
    // Главный реквизит формы не дублируется.
    const QJsonArray attributes = model.value(QStringLiteral("attributes")).toArray();
    int mainAttributes = 0;
    for (const QJsonValue &value : attributes) {
        const QJsonObject attribute = value.toObject();
        if (attribute.value(QStringLiteral("name")).toString()
            == QStringLiteral("Объект")) {
            ++mainAttributes;
        }
    }
    QCOMPARE(mainAttributes, 1);

    // В модели таблицы есть группы колонок.
    bool sawColumnGroup = false;
    std::function<void(const QJsonArray &)> scan = [&](const QJsonArray &items) {
        for (const QJsonValue &value : items) {
            const QJsonObject item = value.toObject();
            if (item.value(QStringLiteral("tag")).toString()
                == QStringLiteral("ColumnGroup")) {
                sawColumnGroup = true;
            }
            scan(item.value(QStringLiteral("childItems")).toArray());
        }
    };
    scan(model.value(QStringLiteral("childItemsRoot")).toArray());
    QVERIFY(sawColumnGroup);
}

} // namespace

int runFormTests(int argc, char **argv){
    TestForm test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_form.moc"
