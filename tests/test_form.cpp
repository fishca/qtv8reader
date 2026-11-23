#include "core/ConfigurationReader.h"
#include "core/FormBindings.h"
#include "core/FormDocument.h"
#include "core/FormOwner.h"
#include "core/FormReader.h"
#include "core/FormStyles.h"
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
    void dynamicListColumns();
    void fieldKindsFromForm();
    void decorationKindsAndHyperlinks();
    void commandBarGroupKinds();
    void styleReferences();
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

    // Видимость групп, декораций и кнопок записана «хвостом» записи —
    // перед расширенной подсказкой. В Form.xml у баннера Visible=false,
    // а декорация-крестик внутри него видима.
    const FormItem *banner = findItemById(document.items, 782);
    QVERIFY(banner);
    QCOMPARE(banner->type, FormItem::Type::Group);
    QVERIFY(!banner->visible);
    const FormItem *bannerText = findItemById(document.items, 784);
    QVERIFY(bannerText);
    QCOMPARE(bannerText->type, FormItem::Type::Decoration);
    QVERIFY(bannerText->visible);
    // Второй баннер (id 774) в форме видим.
    const FormItem *visibleBanner = findItemById(document.items, 774);
    QVERIFY(visibleBanner);
    QVERIFY(visibleBanner->visible);
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

// Форма списка справочника: путь колонки динамического списка записан в
// бинарной форме номером поля («Список.#5»). Номер расшифровывается картой
// полей реквизита «Список» (FieldsMapItemIdN / FieldsMapItemNameN), и колонка
// получает настоящее имя поля — по нему рендерер берёт заголовок и тип.
void TestForm::dynamicListColumns()
{
    const QString sample = QStringLiteral("C:/work/cpp/utils/1Cv8_bp.cf");
    if (!QFileInfo::exists(sample))
        QSKIP("Большая конфигурация 1Cv8_bp.cf не найдена");

    ConfigurationReader reader;
    QString error;
    QVERIFY2(reader.open(sample, &error), qPrintable(error));

    MetadataObject *object = nullptr;
    for (MetadataCategory &category : reader.configuration().categories) {
        if (category.typeTitle != QStringLiteral("Справочник"))
            continue;
        for (MetadataObject &candidate : category.objects) {
            if (candidate.name == QStringLiteral("Валюты")) {
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
        if (entry.name.compare(QStringLiteral("ФормаСписка"), Qt::CaseInsensitive) == 0) {
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

    // Реквизит динамического списка и карта его полей.
    const FormAttribute *listAttribute = nullptr;
    for (const FormAttribute &attribute : document.attributes) {
        if (attribute.name == QStringLiteral("Список"))
            listAttribute = &attribute;
    }
    QVERIFY(listAttribute);
    QVERIFY(listAttribute->dynamicList);
    QCOMPARE(listAttribute->listFields.value(5), QStringLiteral("НаименованиеПолное"));
    QCOMPARE(listAttribute->listFields.value(6), QStringLiteral("Код"));
    QCOMPARE(listAttribute->listFields.value(14), QStringLiteral("Ссылка"));
    QVERIFY(listAttribute->listQuery.startsWith(QStringLiteral("ВЫБРАТЬ")));
    QVERIFY(listAttribute->listManualQuery);

    // Колонки таблицы получили имена полей вместо номеров.
    const FormItem *code = findItemByName(document.items, QStringLiteral("Код"));
    QVERIFY(code);
    QCOMPARE(code->dataPathText, QStringLiteral("Список.Код"));
    const FormItem *fullName =
        findItemByName(document.items, QStringLiteral("НаименованиеПолное"));
    QVERIFY(fullName);
    QCOMPARE(fullName->dataPathText, QStringLiteral("Список.НаименованиеПолное"));
    // Реквизит объекта: заголовок и тип берутся из метаданных.
    QCOMPARE(fullName->dataTitle, QStringLiteral("Наименование валюты"));
    const FormItem *rate = findItemByName(document.items, QStringLiteral("Курс"));
    QVERIFY(rate);
    QCOMPARE(rate->dataPathText, QStringLiteral("Список.Курс"));

    const QJsonObject model =
        QJsonDocument::fromJson(formWebModelJson(document)).object();
    // Реквизит списка — главный, вид — динамический список, настройки списка
    // (основная таблица и текст запроса) отданы рендереру.
    bool sawDynamicAttribute = false;
    const QJsonArray attributes = model.value(QStringLiteral("attributes")).toArray();
    for (const QJsonValue &value : attributes) {
        const QJsonObject attribute = value.toObject();
        if (attribute.value(QStringLiteral("name")).toString() != QStringLiteral("Список"))
            continue;
        sawDynamicAttribute = true;
        const QJsonObject properties =
            attribute.value(QStringLiteral("properties")).toObject();
        QCOMPARE(properties.value(QStringLiteral("MainAttribute")).toString(),
                 QStringLiteral("true"));
        QCOMPARE(properties.value(QStringLiteral("Type")).toString(),
                 QStringLiteral("cfg:DynamicList"));
        const QJsonObject settings = attribute.value(QStringLiteral("settings")).toObject();
        QCOMPARE(settings.value(QStringLiteral("type")).toString(),
                 QStringLiteral("DynamicList"));
        QCOMPARE(settings.value(QStringLiteral("mainTable")).toString(),
                 QStringLiteral("Catalog.Валюты"));
    }
    QVERIFY(sawDynamicAttribute);

    // У основной таблицы списка своей командной панели нет: команды живут
    // в панели формы.
    const QJsonObject table = findModelNode(
        model.value(QStringLiteral("childItemsRoot")).toArray(), QStringLiteral("Валюты"));
    QVERIFY(!table.isEmpty());
    QCOMPARE(table.value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("CommandBarLocation")).toString(),
             QStringLiteral("None"));
}

void TestForm::fieldKindsFromForm()
{
    const QString sample = QStringLiteral("C:/work/cpp/utils/1Cv8_bp.cf");
    if (!QFileInfo::exists(sample))
        QSKIP("Большая конфигурация 1Cv8_bp.cf не найдена");

    ConfigurationReader reader;
    QString error;
    QVERIFY2(reader.open(sample, &error), qPrintable(error));

    // Разбирает форму объекта так же, как это делает приложение: реквизиты
    // объекта известны до разбора, метаданные владельца применяются после.
    auto loadForm = [&reader, &error](const QString &typeTitle, const QString &objectName,
                                      const QString &formName, FormDocument *document) {
        MetadataObject *object = nullptr;
        for (MetadataCategory &category : reader.configuration().categories) {
            if (!category.typeTitle.startsWith(typeTitle))
                continue;
            for (MetadataObject &candidate : category.objects) {
                if (candidate.name == objectName) {
                    object = &candidate;
                    break;
                }
            }
            if (object)
                break;
        }
        if (!object)
            return false;
        reader.loadSections(*object);
        const MetadataSection *forms = object->section(MetadataSectionKind::Forms);
        if (!forms)
            return false;
        const MetadataEntry *formEntry = nullptr;
        for (const MetadataEntry &entry : forms->entries) {
            if (entry.name.compare(formName, Qt::CaseInsensitive) == 0) {
                formEntry = &entry;
                break;
            }
        }
        if (!formEntry)
            return false;
        const V8Element *element = reader.container().find(formEntry->guid + QStringLiteral(".0"));
        if (!element)
            return false;
        QByteArray payload;
        if (!reader.container().elementPayload(*element, &payload, nullptr, &error))
            return false;
        FormBindingResolver resolver;
        resolver.addObject(*object);
        if (!FormReader::parseBytes(payload, document, &error, &resolver))
            return false;
        applyOwnerMetadata(*object, document);
        return true;
    };

    // Вид поля записан в самой форме отдельным кодом, поэтому переключатель,
    // флажок и картинка больше не выводятся из типа реквизита: до этого
    // «Способ установки курса» рисовался полем ввода, а колонка-картинка —
    // надписью.
    FormDocument catalogForm;
    QVERIFY2(loadForm(QStringLiteral("Справочник"), QStringLiteral("Валюты"),
                      QStringLiteral("ФормаЭлемента"), &catalogForm),
             qPrintable(error));

    const FormItem *radio =
        findItemByName(catalogForm.items, QStringLiteral("КурсВалютыВводитсяВручную"));
    QVERIFY(radio);
    QVERIFY(radio->fieldKind == FormFieldKind::RadioButton);
    QCOMPARE(radio->fieldKindCode, 5);
    // Подпись переключателя задана в форме списком выбора.
    QCOMPARE(radio->choiceItems, QStringList{QStringLiteral("вводится вручную")});

    // Заголовок поля лежит отдельно от подсказки: у «Наценки» в форме есть
    // только подсказка, поэтому заголовок берётся из метаданных, а длинный
    // текст остаётся подсказкой.
    const FormItem *markup = findItemByName(catalogForm.items, QStringLiteral("Наценка"));
    QVERIFY(markup);
    QVERIFY(markup->synonym.isEmpty());
    QVERIFY(!markup->tooltip.isEmpty());

    // Стандартные реквизиты: код -2 у справочника — это «Код», код -3 —
    // «Наименование»; у документа те же коды означают «Номер» и «Дата».
    const FormItem *code = findItemByName(catalogForm.items, QStringLiteral("Код"));
    QVERIFY(code);
    QVERIFY(code->fieldKind == FormFieldKind::Input);
    QCOMPARE(code->dataPathText, QStringLiteral("Объект.Code"));
    const FormItem *description =
        findItemByName(catalogForm.items, QStringLiteral("Наименование"));
    QVERIFY(description);
    QCOMPARE(description->dataPathText, QStringLiteral("Объект.Description"));

    const QJsonObject catalogModel =
        QJsonDocument::fromJson(formWebModelJson(catalogForm)).object();
    const QJsonObject radioNode = findModelNode(
        catalogModel.value(QStringLiteral("childItemsRoot")).toArray(),
        QStringLiteral("КурсВалютыВводитсяВручную"));
    QVERIFY(!radioNode.isEmpty());
    QCOMPARE(radioNode.value(QStringLiteral("tag")).toString(),
             QStringLiteral("RadioButtonField"));
    const QJsonArray choiceItems = radioNode.value(QStringLiteral("properties")).toObject()
                                       .value(QStringLiteral("ChoiceListItems")).toArray();
    QCOMPARE(choiceItems.size(), 1);
    QCOMPARE(choiceItems.at(0).toString(), QStringLiteral("вводится вручную"));
    const QJsonObject markupNode = findModelNode(
        catalogModel.value(QStringLiteral("childItemsRoot")).toArray(),
        QStringLiteral("Наценка"));
    QVERIFY(!markupNode.isEmpty());
    const QJsonObject markupProperties =
        markupNode.value(QStringLiteral("properties")).toObject();
    QCOMPARE(markupProperties.value(QStringLiteral("Title")).toString(),
             QStringLiteral("Наценка"));
    QVERIFY(markupProperties.value(QStringLiteral("ToolTip")).toString()
                .startsWith(QStringLiteral("Коэффициент")));

    // Форма списка задач: флажок «Выполнена», поля-картинки в первой колонке
    // и надписи «Ссылка»/«Предмет» — тоже по коду вида.
    FormDocument taskForm;
    QVERIFY2(loadForm(QStringLiteral("Задача"), QStringLiteral("ЗадачаИсполнителя"),
                      QStringLiteral("ФормаСписка"), &taskForm),
             qPrintable(error));

    const FormItem *executed = findItemByName(taskForm.items, QStringLiteral("Выполнена"));
    QVERIFY(executed);
    QVERIFY(executed->fieldKind == FormFieldKind::CheckBox);
    const FormItem *stopped = findItemByName(taskForm.items, QStringLiteral("Остановлена"));
    QVERIFY(stopped);
    QVERIFY(stopped->fieldKind == FormFieldKind::Picture);
    QVERIFY(stopped->labelOnly);
    const FormItem *link = findItemByName(taskForm.items, QStringLiteral("Ссылка"));
    QVERIFY(link);
    QVERIFY(link->fieldKind == FormFieldKind::Label);

    const QJsonObject taskModel =
        QJsonDocument::fromJson(formWebModelJson(taskForm)).object();
    const QJsonArray taskRoot = taskModel.value(QStringLiteral("childItemsRoot")).toArray();
    QCOMPARE(findModelNode(taskRoot, QStringLiteral("Выполнена"))
                 .value(QStringLiteral("tag")).toString(),
             QStringLiteral("CheckBoxField"));
    QCOMPARE(findModelNode(taskRoot, QStringLiteral("Остановлена"))
                 .value(QStringLiteral("tag")).toString(),
             QStringLiteral("PictureField"));
    QCOMPARE(findModelNode(taskRoot, QStringLiteral("Ссылка"))
                 .value(QStringLiteral("tag")).toString(),
             QStringLiteral("LabelField"));

    // А у документа те же коды дают «Номер» и «Дата».
    FormDocument documentForm;
    QVERIFY2(loadForm(QStringLiteral("Документ"), QStringLiteral("АвансовыйОтчет"),
                      QStringLiteral("ФормаДокумента"), &documentForm),
             qPrintable(error));
    const FormItem *number = findItemById(documentForm.items, 236);
    QVERIFY(number);
    QCOMPARE(number->dataPathText, QStringLiteral("Объект.Number"));
    const FormItem *date = findItemById(documentForm.items, 238);
    QVERIFY(date);
    QCOMPARE(date->dataPathText, QStringLiteral("Объект.Date"));

    // Колонки таблицы значений и списка значений, объявленных реквизитами
    // формы, адресуются номером колонки. Состав таблицы записан в её
    // реквизите, состав списка значений задан платформой.
    FormDocument tableForm;
    QVERIFY2(loadForm(QStringLiteral("Обработка"),
                      QStringLiteral("WildberriesПереходРВБ"),
                      QStringLiteral("Форма"), &tableForm),
             qPrintable(error));
    const FormItem *goods = findItemByName(tableForm.items,
                                           QStringLiteral("ТаблицаТоваровНоменклатура"));
    QVERIFY(goods);
    QCOMPARE(goods->dataPathText, QStringLiteral("ТаблицаТоваров.Номенклатура"));
    const FormItem *mark = findItemByName(tableForm.items,
                                          QStringLiteral("СписокДокументовПометка"));
    QVERIFY(mark);
    QCOMPARE(mark->dataPathText, QStringLiteral("СписокДокументов.Check"));
    const FormItem *value = findItemByName(tableForm.items,
                                           QStringLiteral("СписокДокументовЗначение"));
    QVERIFY(value);
    QCOMPARE(value->dataPathText, QStringLiteral("СписокДокументов.Value"));
}

// Вид декорации (надпись или картинка), гиперссылка и картинка записаны в
// записи оформления, а не в имени: по Form.xml формы «Авансовый отчёт»
// проверяются обе декорации баннера — обычная надпись и картинка-крестик,
// которая закрывает баннер.
void TestForm::decorationKindsAndHyperlinks()
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
    FormDocument document;
    QVERIFY2(FormReader::parseBytes(payload, &document, &error), qPrintable(error));

    // id 784 — надпись баннера, id 787 — картинка-крестик с гиперссылкой.
    const FormItem *text = findItemById(document.items, 784);
    QVERIFY(text);
    QVERIFY(!text->pictureDecoration);
    QVERIFY(!text->hyperlink);

    const FormItem *close = findItemById(document.items, 787);
    QVERIFY(close);
    QVERIFY(close->pictureDecoration);
    QVERIFY(close->hyperlink);
    // Картинка крестика — стандартная (−2), её имя известно платформе.
    QCOMPARE(close->pictureStandardCode, -2);

    // Надпись-гиперссылка «Параметры прописи валюты» в форме справочника.
    MetadataObject *catalog = nullptr;
    for (MetadataCategory &category : reader.configuration().categories) {
        for (MetadataObject &candidate : category.objects) {
            if (candidate.name.compare(QStringLiteral("Валюты"), Qt::CaseInsensitive) == 0) {
                catalog = &candidate;
                break;
            }
        }
        if (catalog)
            break;
    }
    QVERIFY(catalog);
    reader.loadSections(*catalog);
    const MetadataSection *catalogForms = catalog->section(MetadataSectionKind::Forms);
    QVERIFY(catalogForms);
    const MetadataEntry *elementForm = nullptr;
    for (const MetadataEntry &entry : catalogForms->entries) {
        if (entry.name == QStringLiteral("ФормаЭлемента")) {
            elementForm = &entry;
            break;
        }
    }
    QVERIFY(elementForm);
    const V8Element *currencyForm =
        reader.container().find(elementForm->guid + QStringLiteral(".0"));
    QVERIFY(currencyForm);
    QVERIFY2(reader.container().elementPayload(*currencyForm, &payload, nullptr, &error),
             qPrintable(error));
    FormDocument currency;
    QVERIFY2(FormReader::parseBytes(payload, &currency, &error), qPrintable(error));
    const FormItem *link = findItemByName(currency.items,
                                          QStringLiteral("ГиперссылкаПараметрыПрописиВалюты"));
    QVERIFY(link);
    QVERIFY(link->hyperlink);
    QVERIFY(!link->pictureDecoration);

    const QJsonObject model = QJsonDocument::fromJson(formWebModelJson(currency)).object();
    const QJsonArray root = model.value(QStringLiteral("childItemsRoot")).toArray();
    const QJsonObject linkNode = findModelNode(root,
                                               QStringLiteral("ГиперссылкаПараметрыПрописиВалюты"));
    QVERIFY(!linkNode.isEmpty());
    QCOMPARE(linkNode.value(QStringLiteral("tag")).toString(),
             QStringLiteral("LabelDecoration"));
    QCOMPARE(linkNode.value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("Hyperlink")).toString(),
             QStringLiteral("true"));

    const QJsonObject bannerModel = QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonObject closeNode = findModelNode(
        bannerModel.value(QStringLiteral("childItemsRoot")).toArray(),
        QStringLiteral("КартинкаЗакрытьБаннер"));
    QVERIFY(!closeNode.isEmpty());
    QCOMPARE(closeNode.value(QStringLiteral("tag")).toString(),
             QStringLiteral("PictureDecoration"));
    const QJsonObject closeProperties = closeNode.value(QStringLiteral("properties")).toObject();
    QCOMPARE(closeProperties.value(QStringLiteral("Picture")).toString(),
             QStringLiteral("StdPicture.InputFieldClear"));
    QCOMPARE(closeProperties.value(QStringLiteral("Hyperlink")).toString(),
             QStringLiteral("true"));
}

// Своя командная панель формы записана группой с кодом 0, панель
// автозаполнения — кодом 9; раньше коды были перепутаны, и панель с кнопками
// вместе с её содержимым пропадала из модели.
void TestForm::commandBarGroupKinds()
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
            if (candidate.name.compare(QStringLiteral("Банки"), Qt::CaseInsensitive) == 0) {
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

    const FormItem *bar = findItemByName(document.items,
                                         QStringLiteral("ГруппаКоманднаяПанель"));
    QVERIFY(bar);
    QCOMPARE(bar->groupKind, 0);
    QVERIFY(!bar->children.isEmpty());

    const FormItem *autoBar = findItemByName(document.items,
                                             QStringLiteral("ФормаКоманднаяПанель"));
    QVERIFY(autoBar);
    QCOMPARE(autoBar->groupKind, 9);

    // Панель остаётся в дереве формы узлом CommandBar, а панель автозаполнения
    // уходит в отдельный узел autoCommandBar.
    const QJsonObject model = QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonArray root = model.value(QStringLiteral("childItemsRoot")).toArray();
    QJsonObject barNode;
    for (const QJsonValue &value : root) {
        const QJsonObject node = value.toObject();
        if (node.value(QStringLiteral("name")).toString()
            == QStringLiteral("ГруппаКоманднаяПанель")) {
            barNode = node;
        }
    }
    QVERIFY(!barNode.isEmpty());
    QCOMPARE(barNode.value(QStringLiteral("tag")).toString(), QStringLiteral("CommandBar"));
    QVERIFY(!barNode.value(QStringLiteral("childItems")).toArray().isEmpty());

    // Дополнения поиска самой таблицы в модель не попадают (в Form.xml их нет),
    // а вынесенные в панель — попадают как элементы панели.
    const QJsonObject autoNode = model.value(QStringLiteral("autoCommandBar")).toObject();
    QCOMPARE(autoNode.value(QStringLiteral("tag")).toString(),
             QStringLiteral("AutoCommandBar"));
    QVERIFY(autoNode.value(QStringLiteral("childItems")).toArray().isEmpty());
    bool sawSearchAddition = false;
    const QJsonArray barChildren = barNode.value(QStringLiteral("childItems")).toArray();
    for (const QJsonValue &value : barChildren) {
        const QJsonObject node = value.toObject();
        if (node.value(QStringLiteral("tag")).toString()
            == QLatin1String("SearchStringAddition")) {
            sawSearchAddition = true;
        }
    }
    QVERIFY(sawSearchAddition);
}

// Стили оформления: в форме они записаны GUID-ами общих стилей конфигурации и
// отрицательными кодами стилей платформы, а Form.xml и рендерер ждут имя
// «style:Имя». Проверяем фон группы-баннера (стиль конфигурации), цвет и шрифт
// поля (стили конфигурации), рамку поля (стиль платформы BorderColor).
void TestForm::styleReferences()
{
    const QString sample = QStringLiteral("C:/work/cpp/utils/1Cv8_bp.cf");
    if (!QFileInfo::exists(sample))
        QSKIP("Большая конфигурация 1Cv8_bp.cf не найдена");

    ConfigurationReader reader;
    QString error;
    QVERIFY2(reader.open(sample, &error), qPrintable(error));
    FormStyleTable styles;
    styles.loadFromConfiguration(reader.configuration(), reader.container());
    QVERIFY(!styles.isEmpty());
    // GUID общего стиля «ПоясняющийТекст» из списка «Элементы стиля».
    QCOMPARE(styles.reference(QStringLiteral("ad87bd29-0ad1-4da4-ac62-38e714e0cb9f")),
             QStringLiteral("style:ПоясняющийТекст"));
    // Коды стилей платформы из записи цвета.
    QCOMPARE(FormStyleTable::platformStyleName(-1), QStringLiteral("FormBackColor"));
    QCOMPARE(FormStyleTable::platformStyleName(-22), QStringLiteral("BorderColor"));
    // Значение стиля-цвета лежит в данных стиля отдельным элементом контейнера.
    QCOMPARE(styles.colorValue(QStringLiteral("ЦветБаннераБежевый")),
             QStringLiteral("#FCEB9F"));
    // Web-цвета записаны в контейнере номером, а Form.xml — именем.
    QCOMPARE(FormStyleTable::webColorName(48), QStringLiteral("Gainsboro"));
    QCOMPARE(styles.colorValue(QStringLiteral("ИтогиФонГруппы")),
             QStringLiteral("web:Gainsboro"));

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
    FormDocument document;
    QVERIFY2(FormReader::parseBytes(payload, &document, &error, nullptr, &styles),
             qPrintable(error));

    // Группа-баннер: фон записан GUID-ом стиля «ЦветБаннераБежевый».
    const FormItem *banner = findItemByName(document.items,
                                            QStringLiteral("ГруппаБаннерПодключенАппаратныйСканер"));
    QVERIFY(banner);
    QCOMPARE(banner->backColor, QStringLiteral("style:ЦветБаннераБежевый"));

    // Поле с цветом и шрифтом из стилей конфигурации.
    const FormItem *field = findItemByName(document.items, QStringLiteral("ОплатаПоставщикамНадписьКурс"));
    QVERIFY(field);
    QCOMPARE(field->textColor, QStringLiteral("style:ЗаголовкиСтрокЦветТекста"));
    QCOMPARE(field->font.reference, QStringLiteral("style:ЗаголовкиСтрокШрифт"));

    // Без таблицы стилей ссылки не разрешаются: свойства остаются пустыми.
    FormDocument plain;
    QVERIFY2(FormReader::parseBytes(payload, &plain, &error), qPrintable(error));
    const FormItem *plainBanner = findItemByName(plain.items,
                                                 QStringLiteral("ГруппаБаннерПодключенАппаратныйСканер"));
    QVERIFY(plainBanner);
    QVERIFY(plainBanner->backColor.isEmpty());

    // В модели рендерера цвета — строками, шрифт — объектом FontSpec.
    const QJsonObject model = QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonArray root = model.value(QStringLiteral("childItemsRoot")).toArray();
    const QJsonObject bannerNode =
        findModelNode(root, QStringLiteral("ГруппаБаннерПодключенАппаратныйСканер"));
    QVERIFY(!bannerNode.isEmpty());
    QCOMPARE(bannerNode.value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("BackColor")).toString(),
             QStringLiteral("style:ЦветБаннераБежевый"));
    // Рендерер получает таблицу значений стилей: без неё цвет не нарисовать.
    QCOMPARE(model.value(QStringLiteral("styleItems")).toObject()
                 .value(QStringLiteral("ЦветБаннераБежевый")).toString(),
             QStringLiteral("#FCEB9F"));
    const QJsonObject fieldNode =
        findModelNode(root, QStringLiteral("ОплатаПоставщикамНадписьКурс"));
    QVERIFY(!fieldNode.isEmpty());
    const QJsonObject fieldProperties = fieldNode.value(QStringLiteral("properties")).toObject();
    QCOMPARE(fieldProperties.value(QStringLiteral("TextColor")).toString(),
             QStringLiteral("style:ЗаголовкиСтрокЦветТекста"));
    const QJsonObject font =
        fieldProperties.value(QStringLiteral("FontSpec")).toObject();
    QCOMPARE(font.value(QStringLiteral("kind")).toString(), QStringLiteral("StyleItem"));
    QCOMPARE(font.value(QStringLiteral("ref")).toString(),
             QStringLiteral("style:ЗаголовкиСтрокШрифт"));

    // Рамка поля — стиль платформы BorderColor (в форме записан кодом −22).
    MetadataObject *tasks = nullptr;
    for (MetadataCategory &category : reader.configuration().categories) {
        for (MetadataObject &candidate : category.objects) {
            if (candidate.name.compare(QStringLiteral("ЗадачаИсполнителя"),
                                       Qt::CaseInsensitive) == 0) {
                tasks = &candidate;
                break;
            }
        }
        if (tasks)
            break;
    }
    QVERIFY(tasks);
    reader.loadSections(*tasks);
    const MetadataSection *taskForms = tasks->section(MetadataSectionKind::Forms);
    QVERIFY(taskForms);
    const MetadataEntry *listForm = nullptr;
    for (const MetadataEntry &entry : taskForms->entries) {
        if (entry.name == QStringLiteral("ФормаСписка")) {
            listForm = &entry;
            break;
        }
    }
    QVERIFY(listForm);
    const V8Element *listElement =
        reader.container().find(listForm->guid + QStringLiteral(".0"));
    QVERIFY(listElement);
    QVERIFY2(reader.container().elementPayload(*listElement, &payload, nullptr, &error),
             qPrintable(error));
    FormDocument list;
    QVERIFY2(FormReader::parseBytes(payload, &list, &error, nullptr, &styles),
             qPrintable(error));
    const FormItem *nameField = findItemById(list.items, 6);
    QVERIFY(nameField);
    QCOMPARE(nameField->borderColor, QStringLiteral("style:BorderColor"));
}

} // namespace

int runFormTests(int argc, char **argv){
    TestForm test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_form.moc"
