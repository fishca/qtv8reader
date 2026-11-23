#include "core/ConfigurationReader.h"
#include "core/FormBindings.h"
#include "core/FormCommands.h"
#include "core/FormDocument.h"
#include "core/FormOwner.h"
#include "core/FormReader.h"
#include "core/FormPictures.h"
#include "core/FormStyles.h"
#include "core/FormWebModel.h"
#include "core/V8Text.h"
#include "core/V8Container.h"
#include "TestPaths.h"

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
        "\"Кнопка1\",{1,1,{\"ru\",\"Кнопка\"}}},"
        // группа без детей с «хвостом»: GroupHorizontalAlign 1 (Center),
        // GroupVerticalAlign 2 (Bottom), затем служебный слот.
        "{22,{6,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"5\","
        "\"ГруппаХвост\",{1,1,{\"ru\",\"Хвост\"}},"
        "{29,\"1\",\"0\",\"0\",\"1\",{\"0\"},{\"1\",\"0\"}},"
        "\"1\",\"2\",\"0\"}"
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
    void standardCommandTables();
    void tableAndColumnProperties();
    void fieldTableListProperties();
    void columnGroupModes();
    void groupAlignment();
    void sizesStretchAndPages();
    void formattedTitlesAndGroupFlags();
    void pageOrientation();
    void commandBarAutofillAndCompactGroups();
    void formattedDocumentCommandBar();
    void tableRepresentationAndRowFlags();
    void userVisiblePageAlignAndRowsPicture();
    void collapsedPageTitleCommandBarAndSource();
    void tableSearchLocations();
    void layoutNodeIsNotItemReference();
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
    QCOMPARE(group.children.size(), 3);
    QCOMPARE(group.children.first().orientation, 0);
    QCOMPARE(group.children.at(2).name, QStringLiteral("ГруппаХвост"));
    QVERIFY(findItemByName(document.items, QStringLiteral("Поле1")) != nullptr);
    QVERIFY(findItemByName(document.items, QStringLiteral("Кнопка1")) != nullptr);
    QCOMPARE(document.fieldCount(), 2);
    QVERIFY(document.itemCount() >= 5);
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

    // Свойства колонок таблицы «Товары» из Form.xml: закреплённая колонка
    // «Номер строки» (FixingInTable=Left), скрытые шапка и подвал у колонок
    // входящего документа и подсказки ввода (InputHint).
    const FormItem *lineNumber = findItemById(document.items, 172);
    QVERIFY(lineNumber);
    QCOMPARE(lineNumber->fixingInTable, 1);
    QVERIFY(!lineNumber->readOnly);
    QVERIFY(lineNumber->showInHeader);
    QVERIFY(lineNumber->showInFooter);
    QVERIFY(lineNumber->inputHint.isEmpty());
    QCOMPARE(lineNumber->widthUnits, 3);

    const FormItem *documentKind = findItemById(document.items, 174);
    QVERIFY(documentKind);
    QVERIFY(!documentKind->showInFooter);
    QCOMPARE(documentKind->inputHint, QStringLiteral("Вид документа"));

    const FormItem *documentNumber = findItemById(document.items, 176);
    QVERIFY(documentNumber);
    QVERIFY(!documentNumber->showInHeader);
    QVERIFY(!documentNumber->showInFooter);
    QCOMPARE(documentNumber->inputHint, QStringLiteral("Вх.номер"));
    QCOMPARE(documentNumber->editMode, 2);
    QVERIFY(documentNumber->horizontalStretch == 0);

    // У колонки «БилетыПеревозчик» в Form.xml явно задан ReadOnly.
    const FormItem *readOnlyColumn = findItemById(document.items, 798);
    QVERIFY(readOnlyColumn);
    QVERIFY(readOnlyColumn->readOnly);

    // Таблица «Товары»: платформа пишет AutoInsertNewRow, EnableStartDrag и
    // EnableDrag, но не чередование цветов строк.
    const FormItem *goods = findItemById(document.items, 169);
    QVERIFY(goods);
    QCOMPARE(goods->type, FormItem::Type::Table);
    QVERIFY(goods->autoInsertNewRow);
    QVERIFY(goods->enableStartDrag);
    QVERIFY(goods->enableDrag);
    QVERIFY(!goods->useAlternationRowColor);

    // Режим ориентации группы лежит в узле макета и различает Horizontal
    // (1) и AlwaysHorizontal (3) — в Form.xml «ГруппаШапка» горизонтальная.
    QCOMPARE(findItemById(document.items, 233)->groupMode, 1);
}

void TestForm::realBpListFormGeometry()
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
    const QString sample = test::corpusFile(QStringLiteral("1Cv8_bp.cf"));
    if (sample.isEmpty())
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
    const QString sample = test::corpusFile(QStringLiteral("1Cv8_bp.cf"));
    if (sample.isEmpty())
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
    const QString sample = test::corpusFile(QStringLiteral("1Cv8_bp.cf"));
    if (sample.isEmpty())
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
    const QString sample = test::corpusFile(QStringLiteral("1Cv8_bp.cf"));
    if (sample.isEmpty())
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
    // У поля нет собственного заголовка: подпись платформа (и рендерер)
    // выводит из пути к данным и метаданных, поэтому в модели Title не нужен.
    QVERIFY(!markupProperties.contains(QStringLiteral("Title")));
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
    // Стандартные картинки платформы: код из записи картинки — отрицательное
    // число; имена сняты с корпуса (Form.xml) и проверены по контейнеру.
    QCOMPARE(FormPictureTable::standardPictureName(-1),
             QStringLiteral("StdPicture.InputFieldSelect"));
    QCOMPARE(FormPictureTable::standardPictureName(-2),
             QStringLiteral("StdPicture.InputFieldClear"));
    QCOMPARE(FormPictureTable::standardPictureName(-10),
             QStringLiteral("StdPicture.CheckAll"));
    QCOMPARE(FormPictureTable::standardPictureName(-11),
             QStringLiteral("StdPicture.UncheckAll"));
    QVERIFY(FormPictureTable::standardPictureName(-999).isEmpty());
}

// Своя командная панель формы записана группой с кодом 0, панель
// автозаполнения — кодом 9; раньше коды были перепутаны, и панель с кнопками
// вместе с её содержимым пропадала из модели.
void TestForm::commandBarGroupKinds()
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
    const QString sample = test::corpusFile(QStringLiteral("1Cv8_bp.cf"));
    if (sample.isEmpty())
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
    FormPictureTable pictures;
    pictures.loadFromConfiguration(reader.configuration(), reader.container());
    QVERIFY(!pictures.isEmpty());
    // Общая картинка конфигурации: GUID в форме, имя и данные — в контейнере.
    QCOMPARE(pictures.reference(QStringLiteral("188d8f0e-94da-44bb-8ed3-21aa01e973b9")),
             QStringLiteral("CommonPicture.Внимание16"));
    QVERIFY(!pictures.dataByName(QStringLiteral("Внимание16")).isEmpty());
    QVERIFY2(FormReader::parseBytes(payload, &document, &error, nullptr, &styles,
                                    &pictures),
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

    // Картинки: значок баннера и картинка значений поля — общие картинки
    // конфигурации, их данные уходят в модель узлом commonPictures.
    const FormItem *bannerPicture = findItemByName(
        document.items, QStringLiteral("КартинкаБаннерЧекиПодотчетныхЛицВнимание"));
    QVERIFY(bannerPicture);
    QCOMPARE(bannerPicture->pictureRef, QStringLiteral("CommonPicture.Внимание16"));
    const FormItem *stateField = findItemByName(document.items,
                                                QStringLiteral("СостояниеДокумента"));
    QVERIFY(stateField);
    QCOMPARE(stateField->valuesPictureRef,
             QStringLiteral("CommonPicture.СостоянияДокумента"));

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

    // Картинка баннера в модели — ссылкой на общую картинку, а её данные
    // (base64 ZIP) лежат в узле commonPictures.
    const QJsonObject pictureNode =
        findModelNode(root, QStringLiteral("КартинкаБаннерЧекиПодотчетныхЛицВнимание"));
    QVERIFY(!pictureNode.isEmpty());
    QCOMPARE(pictureNode.value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("Picture")).toString(),
             QStringLiteral("CommonPicture.Внимание16"));
    const QJsonObject pictureEntry = model.value(QStringLiteral("commonPictures"))
        .toObject().value(QStringLiteral("Внимание16")).toObject();
    QCOMPARE(pictureEntry.value(QStringLiteral("mime")).toString(),
             QStringLiteral("application/zip"));
    QVERIFY(pictureEntry.value(QStringLiteral("data")).toString()
                .startsWith(QStringLiteral("UEsDB")));

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

// Кнопки в бинарной форме ссылаются на команды парой «номер, GUID»: платформа
// хранит стандартные команды отдельно и GUID-ы постоянны. Таблица команд живёт
// в FormCommands.h; здесь проверяем её и то, что у кнопки со стандартной
// командой в модели нет подставленного заголовка — подпись рендерер выводит из
// CommandName, как и платформа.
void TestForm::standardCommandTables()
{
    QCOMPARE(formStandardCommandName(
                 QStringLiteral("4f834c38-add1-45e4-a9f3-cefe3efac5c9")),
             QStringLiteral("Create"));
    QCOMPARE(formStandardCommandName(
                 QStringLiteral("fe558fde-99b3-45d0-a060-9fc2905309f6")),
             QStringLiteral("Write"));
    QCOMPARE(formStandardCommandName(
                 QStringLiteral("342c531d-dc73-458a-8ac4-6a746916a33b")),
             QStringLiteral("Copy"));
    QCOMPARE(itemStandardCommandName(
                 QStringLiteral("b0016a68-ec64-4e6d-b905-c71fd62efc4c")),
             QStringLiteral("Add"));
    QCOMPARE(itemStandardCommandName(
                 QStringLiteral("f20eefc2-f819-4ab1-be67-87b3ca2e26e6")),
             QStringLiteral("Bold"));
    QVERIFY(formStandardCommandName(QString()).isEmpty());
    QVERIFY(itemStandardCommandName(
                QStringLiteral("00000000-0000-0000-0000-000000000000")).isEmpty());

    FormDocument document;
    FormItem button;
    button.type = FormItem::Type::Button;
    button.name = QStringLiteral("КнопкаКопировать");
    button.commandRef = 0;
    button.commandGuid = QStringLiteral("342c531d-dc73-458a-8ac4-6a746916a33b");
    button.commandName = QStringLiteral("Form.StandardCommand.Copy");
    document.items.append(button);

    const QJsonObject model =
        QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonObject node = findModelNode(
        model.value(QStringLiteral("childItemsRoot")).toArray(),
        QStringLiteral("КнопкаКопировать"));
    QVERIFY2(!node.isEmpty(), "кнопка не попала в модель");
    const QJsonObject properties = node.value(QStringLiteral("properties")).toObject();
    QCOMPARE(properties.value(QStringLiteral("CommandName")).toString(),
             QStringLiteral("Form.StandardCommand.Copy"));
    QVERIFY(!properties.contains(QStringLiteral("Title")));

    // Подсказка группы: в Form.xml это отдельное свойство, в контейнере —
    // вторая локализованная запись после имени.
    FormDocument withGroup;
    FormItem group;
    group.type = FormItem::Type::Group;
    group.groupKind = 0;
    group.name = QStringLiteral("ГруппаОтбор");
    group.synonym = QStringLiteral("Группа отбор");
    group.tooltip = QStringLiteral("Группа оформлено отбор");
    withGroup.items.append(group);
    const QJsonObject groupModel =
        QJsonDocument::fromJson(formWebModelJson(withGroup)).object();
    const QJsonObject groupNode = findModelNode(
        groupModel.value(QStringLiteral("childItemsRoot")).toArray(),
        QStringLiteral("ГруппаОтбор"));
    QCOMPARE(groupNode.value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("ToolTip")).toString(),
             QStringLiteral("Группа оформлено отбор"));
}

// Свойства таблицы и колонок, которые платформа пишет в Form.xml только при
// отличии от значения по умолчанию. Проверяется и разбор, и модель для
// рендерера: лишних свойств (значение по умолчанию) в модели быть не должно.
void TestForm::tableAndColumnProperties()
{
    FormDocument document;
    FormItem table;
    table.type = FormItem::Type::Table;
    table.typeCode = 55;
    table.name = QStringLiteral("Товары");
    table.dataPathText = QStringLiteral("Объект.Товары");
    table.autoInsertNewRow = true;
    table.useAlternationRowColor = true;
    table.enableStartDrag = true;
    table.enableDrag = false;
    table.commandBarLocation = 0;
    table.tableTitleLocation = 3;
    table.allowGettingCurrentRowUrl = 1;

    FormItem column;
    column.type = FormItem::Type::Field;
    column.typeCode = 37;
    column.fieldKind = FormFieldKind::Input;
    column.name = QStringLiteral("ТоварыКоличество");
    column.dataPathText = QStringLiteral("Объект.Товары.Количество");
    column.valueKind = FormValueKind::Number;
    column.editMode = 2;
    column.inputHint = QStringLiteral("Количество");
    column.fixingInTable = 1;
    column.readOnly = true;
    column.showInHeader = false;
    column.showInFooter = false;
    column.cellHyperlink = true;
    column.horizontalAlign = 2;
    column.skipOnInput = 0;
    column.horizontalStretch = 0;
    table.children.append(column);

    // Колонка без заданных свойств: значения по умолчанию в модель не попадают.
    FormItem plainColumn;
    plainColumn.type = FormItem::Type::Field;
    plainColumn.typeCode = 37;
    plainColumn.fieldKind = FormFieldKind::Input;
    plainColumn.name = QStringLiteral("ТоварыЦена");
    plainColumn.dataPathText = QStringLiteral("Объект.Товары.Цена");
    plainColumn.valueKind = FormValueKind::Number;
    plainColumn.horizontalStretch = 2;
    table.children.append(plainColumn);

    document.items.append(table);

    const QJsonObject model =
        QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonArray roots = model.value(QStringLiteral("childItemsRoot")).toArray();
    const QJsonObject tableNode = findModelNode(roots, QStringLiteral("Товары"));
    QVERIFY2(!tableNode.isEmpty(), "таблица не попала в модель");
    const QJsonObject tableProps = tableNode.value(QStringLiteral("properties")).toObject();
    QCOMPARE(tableProps.value(QStringLiteral("AutoInsertNewRow")).toString(),
             QStringLiteral("true"));
    QCOMPARE(tableProps.value(QStringLiteral("UseAlternationRowColor")).toString(),
             QStringLiteral("true"));
    QCOMPARE(tableProps.value(QStringLiteral("EnableStartDrag")).toString(),
             QStringLiteral("true"));
    QVERIFY(!tableProps.contains(QStringLiteral("EnableDrag")));
    QCOMPARE(tableProps.value(QStringLiteral("CommandBarLocation")).toString(),
             QStringLiteral("None"));
    QCOMPARE(tableProps.value(QStringLiteral("TitleLocation")).toString(),
             QStringLiteral("Top"));
    QCOMPARE(tableProps.value(QStringLiteral("AllowGettingCurrentRowURL")).toString(),
             QStringLiteral("true"));

    const QJsonObject columnNode = findModelNode(
        tableNode.value(QStringLiteral("childItems")).toArray(),
        QStringLiteral("ТоварыКоличество"));
    QVERIFY2(!columnNode.isEmpty(), "колонка не попала в модель");
    const QJsonObject columnProps =
        columnNode.value(QStringLiteral("properties")).toObject();
    QCOMPARE(columnProps.value(QStringLiteral("ReadOnly")).toString(),
             QStringLiteral("true"));
    QCOMPARE(columnProps.value(QStringLiteral("ShowInHeader")).toString(),
             QStringLiteral("false"));
    QCOMPARE(columnProps.value(QStringLiteral("ShowInFooter")).toString(),
             QStringLiteral("false"));
    QCOMPARE(columnProps.value(QStringLiteral("CellHyperlink")).toString(),
             QStringLiteral("true"));
    QCOMPARE(columnProps.value(QStringLiteral("FixingInTable")).toString(),
             QStringLiteral("Left"));
    QCOMPARE(columnProps.value(QStringLiteral("HorizontalAlign")).toString(),
             QStringLiteral("Right"));
    QCOMPARE(columnProps.value(QStringLiteral("SkipOnInput")).toString(),
             QStringLiteral("false"));
    QCOMPARE(columnProps.value(QStringLiteral("EditMode")).toString(),
             QStringLiteral("EnterOnInput"));
    QCOMPARE(columnProps.value(QStringLiteral("InputHint")).toString(),
             QStringLiteral("Количество"));
    QCOMPARE(columnProps.value(QStringLiteral("HorizontalStretch")).toString(),
             QStringLiteral("false"));

    const QJsonObject plainNode = findModelNode(
        tableNode.value(QStringLiteral("childItems")).toArray(),
        QStringLiteral("ТоварыЦена"));
    QVERIFY(!plainNode.isEmpty());
    const QJsonObject plainProps =
        plainNode.value(QStringLiteral("properties")).toObject();
    QVERIFY(!plainProps.contains(QStringLiteral("ReadOnly")));
    QVERIFY(!plainProps.contains(QStringLiteral("ShowInHeader")));
    QVERIFY(!plainProps.contains(QStringLiteral("ShowInFooter")));
    QVERIFY(!plainProps.contains(QStringLiteral("FixingInTable")));
    QVERIFY(!plainProps.contains(QStringLiteral("EditMode")));
    QVERIFY(!plainProps.contains(QStringLiteral("InputHint")));
    // Auto — значение по умолчанию, в XML его нет.
    QVERIFY(!plainProps.contains(QStringLiteral("HorizontalStretch")));

    // Группа: Group выводится из режима ориентации, «горизонтально, если
    // помещается» (2) — значение по умолчанию и в модель не попадает.
    FormDocument groups;
    FormItem always;
    always.type = FormItem::Type::Group;
    always.groupKind = 5;
    always.name = QStringLiteral("ГруппаВсегда");
    always.groupMode = 3;
    groups.items.append(always);
    FormItem horizontalIfPossible;
    horizontalIfPossible.type = FormItem::Type::Group;
    horizontalIfPossible.groupKind = 5;
    horizontalIfPossible.name = QStringLiteral("ГруппаПоШирине");
    horizontalIfPossible.groupMode = 2;
    groups.items.append(horizontalIfPossible);

    const QJsonObject groupModel =
        QJsonDocument::fromJson(formWebModelJson(groups)).object();
    const QJsonArray groupRoots =
        groupModel.value(QStringLiteral("childItemsRoot")).toArray();
    QCOMPARE(findModelNode(groupRoots, QStringLiteral("ГруппаВсегда"))
                 .value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("Group")).toString(),
             QStringLiteral("AlwaysHorizontal"));
    QVERIFY(!findModelNode(groupRoots, QStringLiteral("ГруппаПоШирине"))
                 .value(QStringLiteral("properties")).toObject()
                 .contains(QStringLiteral("Group")));
}

// Свойства поля из записи макета, настройки динамического списка и свойства
// кнопки: платформа пишет их в Form.xml только при отличии от значения по
// умолчанию, поэтому модель получает ровно то, что задано в форме.
void TestForm::fieldTableListProperties()
{
    FormDocument document;

    FormItem column;
    column.type = FormItem::Type::Field;
    column.typeCode = 37;
    column.fieldKind = FormFieldKind::Input;
    column.name = QStringLiteral("КолонкаСвойства");
    column.dataPathText = QStringLiteral("Объект.Товары.Количество");
    column.valueKind = FormValueKind::Number;
    column.autoMaxWidth = false;
    column.autoMaxHeight = false;
    column.maxWidth = 28;
    column.wrap = false;
    column.choiceButton = 0;
    column.clearButton = 1;
    column.spinButton = 0;
    column.openButton = 1;
    column.dropListButton = 0;
    column.choiceListButton = 1;
    column.chooseType = false;
    column.textEdit = false;
    column.listChoiceMode = true;
    column.autoMarkIncomplete = 1;
    column.autoChoiceIncomplete = 1;
    column.choiceHistoryOnInput = 1;
    column.choiceButtonRepresentation = 3;
    column.choiceFoldersAndItems = 2;
    column.fileDragMode = 0;

    FormItem pictureField;
    pictureField.type = FormItem::Type::Field;
    pictureField.typeCode = 37;
    pictureField.fieldKind = FormFieldKind::Picture;
    pictureField.name = QStringLiteral("ПолеКартинка");
    pictureField.hyperlink = true;

    FormItem table;
    table.type = FormItem::Type::Table;
    table.typeCode = 55;
    table.name = QStringLiteral("Список");
    table.dataPathText = QStringLiteral("Список");
    table.tableHeader = false;
    table.autoRefresh = 0;
    table.autoRefreshPeriod = 60;
    table.period = QStringLiteral("Custom, 0001-01-01T00:00:00, 0001-01-01T00:00:00");
    table.tableChoiceFolders = 2;
    table.restoreCurrentRow = 1;
    table.showRoot = 0;
    table.allowRootChoice = 1;
    table.hasRowFilter = true;
    table.updateOnDataChange = 0;
    table.hasTopLevelParent = true;
    table.children.append(column);
    table.children.append(pictureField);

    FormItem button;
    button.type = FormItem::Type::Button;
    button.typeCode = 31;
    button.name = QStringLiteral("КнопкаСвойства");
    button.buttonType = 1;
    button.buttonRepresentation = 1;
    button.skipOnInput = 0;
    button.displayImportance = 3;
    button.buttonGroupHorizontalAlign = 2;

    FormItem pictureDecoration;
    pictureDecoration.type = FormItem::Type::Decoration;
    pictureDecoration.typeCode = 12;
    pictureDecoration.name = QStringLiteral("ДекорацияКартинка");
    pictureDecoration.pictureDecoration = true;
    pictureDecoration.autoMaxWidth = false;
    pictureDecoration.fileDragMode = 0;

    FormItem labelDecoration;
    labelDecoration.type = FormItem::Type::Decoration;
    labelDecoration.typeCode = 12;
    labelDecoration.name = QStringLiteral("ДекорацияНадпись");
    labelDecoration.autoMaxWidth = false;

    document.items.append(table);
    document.items.append(button);
    document.items.append(pictureDecoration);
    document.items.append(labelDecoration);

    const QJsonObject model =
        QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonArray roots = model.value(QStringLiteral("childItemsRoot")).toArray();

    const QJsonObject tableNode = findModelNode(roots, QStringLiteral("Список"));
    QVERIFY2(!tableNode.isEmpty(), "таблица не попала в модель");
    const QJsonObject tableProps =
        tableNode.value(QStringLiteral("properties")).toObject();
    QCOMPARE(tableProps.value(QStringLiteral("Header")).toString(),
             QStringLiteral("false"));
    QCOMPARE(tableProps.value(QStringLiteral("AutoRefresh")).toString(),
             QStringLiteral("false"));
    QCOMPARE(tableProps.value(QStringLiteral("AutoRefreshPeriod")).toString(),
             QStringLiteral("60"));
    QCOMPARE(tableProps.value(QStringLiteral("Period")).toString(),
             QStringLiteral("Custom, 0001-01-01T00:00:00, 0001-01-01T00:00:00"));
    QCOMPARE(tableProps.value(QStringLiteral("ChoiceFoldersAndItems")).toString(),
             QStringLiteral("FoldersAndItems"));
    QCOMPARE(tableProps.value(QStringLiteral("RestoreCurrentRow")).toString(),
             QStringLiteral("true"));
    QCOMPARE(tableProps.value(QStringLiteral("ShowRoot")).toString(),
             QStringLiteral("false"));
    QCOMPARE(tableProps.value(QStringLiteral("AllowRootChoice")).toString(),
             QStringLiteral("true"));
    QVERIFY(tableProps.contains(QStringLiteral("RowFilter")));
    QVERIFY(tableProps.value(QStringLiteral("RowFilter")).toString().isEmpty());
    QCOMPARE(tableProps.value(QStringLiteral("UpdateOnDataChange")).toString(),
             QStringLiteral("Auto"));
    QVERIFY(tableProps.contains(QStringLiteral("TopLevelParent")));
    QVERIFY(tableProps.value(QStringLiteral("TopLevelParent")).toString().isEmpty());

    const QJsonObject columnNode = findModelNode(
        tableNode.value(QStringLiteral("childItems")).toArray(),
        QStringLiteral("КолонкаСвойства"));
    QVERIFY2(!columnNode.isEmpty(), "колонка не попала в модель");
    const QJsonObject columnProps =
        columnNode.value(QStringLiteral("properties")).toObject();
    QCOMPARE(columnProps.value(QStringLiteral("AutoMaxWidth")).toString(),
             QStringLiteral("false"));
    QCOMPARE(columnProps.value(QStringLiteral("AutoMaxHeight")).toString(),
             QStringLiteral("false"));
    QCOMPARE(columnProps.value(QStringLiteral("MaxWidth")).toString(),
             QStringLiteral("28"));
    QCOMPARE(columnProps.value(QStringLiteral("Wrap")).toString(),
             QStringLiteral("false"));
    QCOMPARE(columnProps.value(QStringLiteral("ChoiceButton")).toString(),
             QStringLiteral("false"));
    QCOMPARE(columnProps.value(QStringLiteral("ClearButton")).toString(),
             QStringLiteral("true"));
    QCOMPARE(columnProps.value(QStringLiteral("SpinButton")).toString(),
             QStringLiteral("false"));
    QCOMPARE(columnProps.value(QStringLiteral("OpenButton")).toString(),
             QStringLiteral("true"));
    QCOMPARE(columnProps.value(QStringLiteral("DropListButton")).toString(),
             QStringLiteral("false"));
    QCOMPARE(columnProps.value(QStringLiteral("ChoiceListButton")).toString(),
             QStringLiteral("true"));
    QCOMPARE(columnProps.value(QStringLiteral("ChooseType")).toString(),
             QStringLiteral("false"));
    QCOMPARE(columnProps.value(QStringLiteral("TextEdit")).toString(),
             QStringLiteral("false"));
    QCOMPARE(columnProps.value(QStringLiteral("ListChoiceMode")).toString(),
             QStringLiteral("true"));
    QCOMPARE(columnProps.value(QStringLiteral("AutoMarkIncomplete")).toString(),
             QStringLiteral("true"));
    QCOMPARE(columnProps.value(QStringLiteral("AutoChoiceIncomplete")).toString(),
             QStringLiteral("true"));
    QCOMPARE(columnProps.value(QStringLiteral("ChoiceHistoryOnInput")).toString(),
             QStringLiteral("DontUse"));
    QCOMPARE(columnProps.value(QStringLiteral("ChoiceButtonRepresentation")).toString(),
             QStringLiteral("ShowInInputField"));
    QCOMPARE(columnProps.value(QStringLiteral("ChoiceFoldersAndItems")).toString(),
             QStringLiteral("FoldersAndItems"));
    QCOMPARE(columnProps.value(QStringLiteral("FileDragMode")).toString(),
             QStringLiteral("AsFile"));

    const QJsonObject pictureProps = findModelNode(
        tableNode.value(QStringLiteral("childItems")).toArray(),
        QStringLiteral("ПолеКартинка"))
        .value(QStringLiteral("properties")).toObject();
    QCOMPARE(pictureProps.value(QStringLiteral("Hyperlink")).toString(),
             QStringLiteral("true"));
    // FileDragMode по умолчанию (UseAsFile) в модель не попадает.
    QVERIFY(!pictureProps.contains(QStringLiteral("FileDragMode")));

    const QJsonObject buttonProps = findModelNode(roots, QStringLiteral("КнопкаСвойства"))
        .value(QStringLiteral("properties")).toObject();
    QCOMPARE(buttonProps.value(QStringLiteral("Type")).toString(),
             QStringLiteral("UsualButton"));
    QCOMPARE(buttonProps.value(QStringLiteral("Representation")).toString(),
             QStringLiteral("Picture"));
    QCOMPARE(buttonProps.value(QStringLiteral("SkipOnInput")).toString(),
             QStringLiteral("false"));
    QCOMPARE(buttonProps.value(QStringLiteral("DisplayImportance")).toString(),
             QStringLiteral("Usual"));
    QCOMPARE(buttonProps.value(QStringLiteral("GroupHorizontalAlign")).toString(),
             QStringLiteral("Right"));

    const QJsonObject pictureDecorationProps =
        findModelNode(roots, QStringLiteral("ДекорацияКартинка"))
            .value(QStringLiteral("properties")).toObject();
    QCOMPARE(pictureDecorationProps.value(QStringLiteral("FileDragMode")).toString(),
             QStringLiteral("AsFile"));
    // AutoMaxWidth есть и у декорации-картинки; AutoMaxHeight по умолчанию
    // (true) платформа не пишет.
    QCOMPARE(pictureDecorationProps.value(QStringLiteral("AutoMaxWidth")).toString(),
             QStringLiteral("false"));
    QVERIFY(!pictureDecorationProps.contains(QStringLiteral("AutoMaxHeight")));
    const QJsonObject labelDecorationProps =
        findModelNode(roots, QStringLiteral("ДекорацияНадпись"))
            .value(QStringLiteral("properties")).toObject();
    QCOMPARE(labelDecorationProps.value(QStringLiteral("AutoMaxWidth")).toString(),
             QStringLiteral("false"));
}

// Таблица с группами колонок: вид группировки записан в записи {2, режим, …}
// (0 — Horizontal, 2 — InCell, 1 — значение по умолчанию). Без Group рендерер
// считает группу вертикальной, поэтому Horizontal пишется в модель явно.
void TestForm::columnGroupModes()
{
    const QString text = QStringLiteral(
        "{4,"
        "{50,0,"
        "{55,{1,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"0\","
        "\"Таблица\",{1,1,{\"ru\",\"Таблица\"}},"
        // Группа колонок по горизонтали: режим 0, заголовок показан.
        "{22,{2,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",2,"
        "\"ГруппаКолонок\",{1,1,{\"ru\",\"Группа колонок\"}},"
        "{2,0,1,1,3,{\"0\"},{1,0},{\"0\"},0,0,0,0},"
        "{37,{3,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"0\","
        "\"Колонка1\",{1,1,{\"ru\",\"Колонка 1\"}}},"
        // Вложенная группа в ячейке: режим 2, закреплена слева.
        "{22,{4,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",2,"
        "\"ГруппаВЯчейке\",{1,1,{\"ru\",\"В ячейке\"}},"
        "{2,2,1,0,3,{\"0\"},{1,0},{\"0\"},0,0,0,1},"
        "{37,{5,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"0\","
        "\"Колонка2\",{1,1,{\"ru\",\"Колонка 2\"}}}"
        "},"
        // Группа без заданной группировки: режим 1, в XML свойств нет.
        "{22,{6,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",2,"
        "\"ГруппаПоУмолчанию\",{1,1,{\"ru\",\"Обычная\"}},"
        "{2,1,1,0,3,{\"0\"},{1,0},{\"0\"},0,0,0,0},"
        "{37,{7,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"0\","
        "\"Колонка3\",{1,1,{\"ru\",\"Колонка 3\"}}}"
        "}"
        "}"
        "},"
        "\"Процедура Тест()\\nКонецПроцедуры\","
        "{4,\"2\","
        "{9,{\"1\"},\"0\",\"Атрибут1\",{1,1,{\"ru\",\"Атрибут 1\"}},{\"Pattern\",{\"S\"}}}"
        "},"
        "{0,0},"
        "{0,\"1\",{9,{\"1\"},\"Команда1\",{1,1,{\"ru\",\"Команда\"}}}}"
        "}"
        "}");

    FormDocument document;
    QString error;
    QVERIFY2(FormReader::parseBytes(text.toUtf8(), &document, &error),
             qPrintable(error));

    const FormItem *horizontal = findItemByName(document.items,
                                                QStringLiteral("ГруппаКолонок"));
    QVERIFY(horizontal);
    QCOMPARE(horizontal->groupKind, 2);
    QCOMPARE(horizontal->columnGroupMode, 0);
    QVERIFY(horizontal->showInHeader);
    QCOMPARE(horizontal->fixingInTable, -1);
    const FormItem *inCell = findItemByName(document.items,
                                            QStringLiteral("ГруппаВЯчейке"));
    QVERIFY(inCell);
    QCOMPARE(inCell->columnGroupMode, 2);
    QVERIFY(!inCell->showInHeader);
    QCOMPARE(inCell->fixingInTable, 1);
    const FormItem *fallback = findItemByName(document.items,
                                              QStringLiteral("ГруппаПоУмолчанию"));
    QVERIFY(fallback);
    QCOMPARE(fallback->columnGroupMode, 1);
    QVERIFY(!fallback->showInHeader);
    QCOMPARE(fallback->fixingInTable, -1);

    const QJsonObject model =
        QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonArray roots = model.value(QStringLiteral("childItemsRoot")).toArray();
    const QJsonArray columns = findModelNode(roots, QStringLiteral("Таблица"))
                                   .value(QStringLiteral("childItems")).toArray();
    QCOMPARE(findModelNode(columns, QStringLiteral("ГруппаКолонок"))
                 .value(QStringLiteral("tag")).toString(),
             QStringLiteral("ColumnGroup"));
    const QJsonObject horizontalProps =
        findModelNode(columns, QStringLiteral("ГруппаКолонок"))
            .value(QStringLiteral("properties")).toObject();
    QCOMPARE(horizontalProps.value(QStringLiteral("Group")).toString(),
             QStringLiteral("Horizontal"));
    QCOMPARE(horizontalProps.value(QStringLiteral("ShowInHeader")).toString(),
             QStringLiteral("true"));
    QVERIFY(!horizontalProps.contains(QStringLiteral("FixingInTable")));
    const QJsonObject inCellProps =
        findModelNode(columns, QStringLiteral("ГруппаВЯчейке"))
            .value(QStringLiteral("properties")).toObject();
    QCOMPARE(inCellProps.value(QStringLiteral("Group")).toString(),
             QStringLiteral("InCell"));
    QCOMPARE(inCellProps.value(QStringLiteral("FixingInTable")).toString(),
             QStringLiteral("Left"));
    QVERIFY(!inCellProps.contains(QStringLiteral("ShowInHeader")));
    QVERIFY(!findModelNode(columns, QStringLiteral("ГруппаПоУмолчанию"))
                 .value(QStringLiteral("properties")).toObject()
                 .contains(QStringLiteral("Group")));
    QVERIFY(!findModelNode(columns, QStringLiteral("ГруппаПоУмолчанию"))
                 .value(QStringLiteral("properties")).toObject()
                 .contains(QStringLiteral("ShowInHeader")));
}

// Выравнивание элемента внутри группы (GroupHorizontalAlign и
// GroupVerticalAlign) лежит в «хвосте» записи: у группы — 3-й и 2-й слоты с
// конца, у декорации — 4-й и 3-й, у поля — 6-й и 5-й. Коды 0..2 — Left/Top,
// Center, Right/Bottom, 3 — Auto (платформа в Form.xml не пишет).
void TestForm::groupAlignment()
{
    FormDocument document;
    QString error;
    QVERIFY2(FormReader::parseBytes(makeSyntheticForm(), &document, &error),
             qPrintable(error));
    const FormItem *group = findItemByName(document.items, QStringLiteral("ГруппаХвост"));
    QVERIFY(group);
    QCOMPARE(group->groupHorizontalAlign, 1);
    QCOMPARE(group->groupVerticalAlign, 2);
    const FormItem *plain = findItemByName(document.items, QStringLiteral("ГруппаЛевая"));
    QVERIFY(plain);
    QCOMPARE(plain->groupHorizontalAlign, -1);
    QCOMPARE(plain->groupVerticalAlign, -1);

    const QJsonObject model =
        QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonArray roots = model.value(QStringLiteral("childItemsRoot")).toArray();
    const QJsonObject properties =
        findModelNode(roots, QStringLiteral("ГруппаХвост"))
            .value(QStringLiteral("properties")).toObject();
    QCOMPARE(properties.value(QStringLiteral("GroupHorizontalAlign")).toString(),
             QStringLiteral("Center"));
    QCOMPARE(properties.value(QStringLiteral("GroupVerticalAlign")).toString(),
             QStringLiteral("Bottom"));
    const QJsonObject plainProperties =
        findModelNode(roots, QStringLiteral("ГруппаЛевая"))
            .value(QStringLiteral("properties")).toObject();
    QVERIFY(!plainProperties.contains(QStringLiteral("GroupHorizontalAlign")));
    QVERIFY(!plainProperties.contains(QStringLiteral("GroupVerticalAlign")));
}

// Размеры и растяжение элементов, представление страниц и высота поля: всё это
// лежит в записи формы и сверяется по справочнику «Календари» из большой
// конфигурации (таблица «Шаблон заполнения» 45×7 без растяжения, группа
// «Страницы» без закладок, надпись с высотой 1 и без вертикального растяжения).
void TestForm::sizesStretchAndPages()
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
            if (candidate.name.compare(QStringLiteral("Календари"), Qt::CaseInsensitive) == 0) {
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
        if (entry.name == QStringLiteral("ФормаЭлемента")) {
            formEntry = &entry;
            break;
        }
    }
    QVERIFY(formEntry);

    const V8Element *element = reader.container().find(formEntry->guid + QStringLiteral(".0"));
    QVERIFY(element);
    QByteArray payload;
    QVERIFY2(reader.container().elementPayload(*element, &payload, nullptr, &error),
             qPrintable(error));

    FormDocument document;
    QVERIFY2(FormReader::parseBytes(payload, &document, &error), qPrintable(error));

    // Таблица: ширина и высота в единицах формы, растяжение — код 0 (нет).
    const FormItem *table = findItemByName(document.items, QStringLiteral("ШаблонЗаполнения"));
    QVERIFY(table);
    QCOMPARE(table->widthUnits, 45);
    QCOMPARE(table->heightUnits, 7);
    QCOMPARE(table->horizontalStretch, 0);
    QCOMPARE(table->verticalStretch, 0);

    // Группа-страницы: 0 — PagesRepresentation None (закладок нет).
    const FormItem *pages = findItemByName(document.items, QStringLiteral("Страницы"));
    QVERIFY(pages);
    QCOMPARE(pages->pagesRepresentation, 0);

    // Надпись: высота из записи макета и явное отсутствие растяжения.
    const FormItem *label = findItemByName(
        document.items, QStringLiteral("РучноеРедактированиеИнформационныйТекст"));
    QVERIFY(label);
    QCOMPARE(label->heightUnits, 1);
    QCOMPARE(label->verticalStretch, 0);

    const QJsonObject model = QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonArray roots = model.value(QStringLiteral("childItemsRoot")).toArray();
    const QJsonObject tableProperties = findModelNode(roots, QStringLiteral("ШаблонЗаполнения"))
                                            .value(QStringLiteral("properties")).toObject();
    QCOMPARE(tableProperties.value(QStringLiteral("Width")).toString(), QStringLiteral("45"));
    QCOMPARE(tableProperties.value(QStringLiteral("Height")).toString(), QStringLiteral("7"));
    QCOMPARE(tableProperties.value(QStringLiteral("HorizontalStretch")).toString(),
             QStringLiteral("false"));
    QCOMPARE(tableProperties.value(QStringLiteral("VerticalStretch")).toString(),
             QStringLiteral("false"));
    const QJsonObject pagesProperties = findModelNode(roots, QStringLiteral("Страницы"))
                                            .value(QStringLiteral("properties")).toObject();
    QCOMPARE(pagesProperties.value(QStringLiteral("PagesRepresentation")).toString(),
             QStringLiteral("None"));
    const QJsonObject labelProperties = findModelNode(
        roots, QStringLiteral("РучноеРедактированиеИнформационныйТекст"))
                                            .value(QStringLiteral("properties")).toObject();
    QCOMPARE(labelProperties.value(QStringLiteral("Height")).toString(), QStringLiteral("1"));
    QCOMPARE(labelProperties.value(QStringLiteral("VerticalStretch")).toString(),
             QStringLiteral("false"));
}

// Форматированные заголовки, пределы авторазмера, перетаскивание файлов в
// таблицу, поведение групп и путь к данным по номеру реквизита формы.
void TestForm::formattedTitlesAndGroupFlags()
{
    const QString sample = test::corpusFile(QStringLiteral("1Cv8_bp.cf"));
    if (sample.isEmpty())
        QSKIP("Большая конфигурация 1Cv8_bp.cf не найдена");

    ConfigurationReader reader;
    QString error;
    QVERIFY2(reader.open(sample, &error), qPrintable(error));

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

    FormDocument calendar;
    QVERIFY2(loadForm(QStringLiteral("Справочник"), QStringLiteral("Календари"),
                      QStringLiteral("ФормаЭлемента"), &calendar),
             qPrintable(error));

    // Заголовок «Период заполнения графика» задан как неформатированный: флаг
    // в записи есть, разметки нет.
    const FormItem *period =
        findItemByName(calendar.items, QStringLiteral("НадписьПериодЗаполнения"));
    QVERIFY(period);
    QVERIFY(period->hasTitleFormatted);
    QVERIFY(!period->titleFormatted);

    // Предел авторазмера надписи задан явно (AutoMaxWidth false).
    const FormItem *text =
        findItemByName(calendar.items, QStringLiteral("ЗаполненностьИнформационныйТекст"));
    QVERIFY(text);
    QVERIFY(!text->autoMaxWidth);

    // Перетаскивание файлов в таблицу: код 0 — AsFile.
    const FormItem *table = findItemByName(calendar.items, QStringLiteral("ШаблонЗаполнения"));
    QVERIFY(table);
    QCOMPARE(table->fileDragMode, 0);

    // Поведение группы: у «Настройки группа» задано обычное, а отображение —
    // None. У «Настройка заполнения» отображение не задано (значение по
    // умолчанию), поэтому свойства в модели нет.
    const FormItem *plainGroup =
        findItemByName(calendar.items, QStringLiteral("НастройкаЗаполненияГруппа"));
    QVERIFY(plainGroup);
    const FormItem *settingsGroup =
        findItemByName(calendar.items, QStringLiteral("НастройкиГруппа"));
    QVERIFY(settingsGroup);
    QVERIFY(settingsGroup->hasBehavior);
    QCOMPARE(static_cast<int>(settingsGroup->representation), 0);

    const QJsonObject model = QJsonDocument::fromJson(formWebModelJson(calendar)).object();
    const QJsonArray roots = model.value(QStringLiteral("childItemsRoot")).toArray();
    auto propertiesOf = [&roots](const QString &name) {
        return findModelNode(roots, name).value(QStringLiteral("properties")).toObject();
    };
    QCOMPARE(propertiesOf(QStringLiteral("НадписьПериодЗаполнения"))
                 .value(QStringLiteral("TitleFormatted")).toString(),
             QStringLiteral("false"));
    QCOMPARE(propertiesOf(QStringLiteral("ЗаполненностьИнформационныйТекст"))
                 .value(QStringLiteral("AutoMaxWidth")).toString(),
             QStringLiteral("false"));
    QCOMPARE(propertiesOf(QStringLiteral("ШаблонЗаполнения"))
                 .value(QStringLiteral("FileDragMode")).toString(),
             QStringLiteral("AsFile"));
    QCOMPARE(propertiesOf(QStringLiteral("НастройкиГруппа"))
                 .value(QStringLiteral("Representation")).toString(),
             QStringLiteral("None"));
    QCOMPARE(propertiesOf(QStringLiteral("НастройкиГруппа"))
                 .value(QStringLiteral("Behavior")).toString(),
             QStringLiteral("Usual"));

    // Группа без поведения: «ГруппаМетки» не всплывающая — раньше читатель
    // считал её всплывающей по десятому полю записи макета.
    FormDocument cards;
    QVERIFY2(loadForm(QStringLiteral("Справочник"), QStringLiteral("КонтактныеЛица"),
                      QStringLiteral("ФормаЛиды"), &cards),
             qPrintable(error));
    const FormItem *marks = findItemByName(cards.items, QStringLiteral("ГруппаМетки"));
    QVERIFY(marks);
    QVERIFY(!marks->hasBehavior);
    const QJsonObject cardsModel =
        QJsonDocument::fromJson(formWebModelJson(cards)).object();
    const QJsonArray cardsRoots =
        cardsModel.value(QStringLiteral("childItemsRoot")).toArray();
    QVERIFY(!findModelNode(cardsRoots, QStringLiteral("ГруппаМетки"))
                 .value(QStringLiteral("properties")).toObject()
                 .contains(QStringLiteral("Behavior")));

    // Выравнивание содержимого группы и размещение кнопки в командной панели.
    QCOMPARE(findModelNode(cardsRoots, QStringLiteral("ГруппаПодвалКанбан"))
                 .value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("HorizontalAlign")).toString(),
             QStringLiteral("Right"));
    const FormItem *create = findItemByName(cards.items, QStringLiteral("ФормаСоздать"));
    QVERIFY(create);
    QCOMPARE(create->commandBarLocationCode, 3);
    QCOMPARE(findModelNode(cardsRoots, QStringLiteral("ФормаСоздать"))
                 .value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("LocationInCommandBar")).toString(),
             QStringLiteral("InCommandBarAndInAdditionalSubmenu"));

    // Размеченный заголовок декорации: шрифт и ссылка остаются в тексте,
    // TitleFormatted — true.
    FormDocument processing;
    QVERIFY2(loadForm(QStringLiteral("Обработка"),
                      QStringLiteral("WildberriesПереходРВБ"),
                      QStringLiteral("Форма"), &processing),
             qPrintable(error));
    const FormItem *banner = findItemByName(processing.items, QStringLiteral("Декорация1"));
    QVERIFY(banner);
    QVERIFY(banner->hasTitleFormatted);
    QVERIFY(banner->titleFormatted);
    QVERIFY(banner->synonym.contains(QStringLiteral("<link ")));

    // Путь к данным разрешается по номеру реквизита формы: у «Итого предыдущий
    // аванс» номер 3, и поле называется иначе, чем реквизит.
    FormDocument documentForm;
    QVERIFY2(loadForm(QStringLiteral("Документ"), QStringLiteral("АвансовыйОтчет"),
                      QStringLiteral("ФормаДокумента"), &documentForm),
             qPrintable(error));
    const FormItem *total =
        findItemByName(documentForm.items, QStringLiteral("НадписьВсегоПредыдущийАванс"));
    QVERIFY(total);
    QCOMPARE(total->dataPathText, QStringLiteral("ИтогоПредыдущийАванс"));
}

// Ориентация страницы: у тега Page нет узла макета обычной группы, вместо него
// в записи лежит блок {18,…}, 17-е поле которого задаёт Group.
void TestForm::pageOrientation()
{
    const QString sample = test::corpusFile(QStringLiteral("1Cv8_bp.cf"));
    if (sample.isEmpty())
        QSKIP("Большая конфигурация 1Cv8_bp.cf не найдена");

    ConfigurationReader reader;
    QString error;
    QVERIFY2(reader.open(sample, &error), qPrintable(error));

    MetadataObject *object = nullptr;
    for (MetadataCategory &category : reader.configuration().categories) {
        if (!category.typeTitle.startsWith(QStringLiteral("Справочник")))
            continue;
        for (MetadataObject &candidate : category.objects) {
            if (candidate.name == QStringLiteral("Новости")) {
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
    const V8Element *element = reader.container().find(formEntry->guid + QStringLiteral(".0"));
    QVERIFY(element);
    QByteArray payload;
    QVERIFY2(reader.container().elementPayload(*element, &payload, nullptr, &error),
             qPrintable(error));
    FormBindingResolver resolver;
    resolver.addObject(*object);
    FormDocument document;
    QVERIFY2(FormReader::parseBytes(payload, &document, &error, &resolver), qPrintable(error));

    // Страница с горизонтальным содержимым: код 1 в семнадцатом поле блока.
    const FormItem *page = findItemByName(
        document.items, QStringLiteral("СтраницаПростоТекстДляВнешнихИсточников"));
    QVERIFY(page);
    QCOMPARE(page->pageMode, 1);

    const QJsonObject model = QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonArray roots = model.value(QStringLiteral("childItemsRoot")).toArray();
    QCOMPARE(findModelNode(roots, QStringLiteral("СтраницаПростоТекстДляВнешнихИсточников"))
                 .value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("Group")).toString(),
             QStringLiteral("Horizontal"));
}

// Автозаполнение командной панели и компактная группа кнопок: в записи панели
// признак стоит через 14 полей после имени (третье поле списка из трёх чисел),
// у группы кнопок — четвёртым полем блока {2, …, 2, вид}.
void TestForm::commandBarAutofillAndCompactGroups()
{
    const QString sample = test::corpusFile(QStringLiteral("1Cv8_bp.cf"));
    if (sample.isEmpty())
        QSKIP("Большая конфигурация 1Cv8_bp.cf не найдена");

    ConfigurationReader reader;
    QString error;
    QVERIFY2(reader.open(sample, &error), qPrintable(error));

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

    // Панель без автозаполнения: разработчик снял флажок, Form.xml пишет
    // Autofill=false, и модель отдаёт признак рендереру.
    FormDocument withoutAutofill;
    QVERIFY2(loadForm(QStringLiteral("Справочник"),
                      QStringLiteral("АктыОтбораПробЗЕРНО"),
                      QStringLiteral("ФормаСписка"), &withoutAutofill),
             qPrintable(error));
    QJsonObject model =
        QJsonDocument::fromJson(formWebModelJson(withoutAutofill)).object();
    QCOMPARE(model.value(QStringLiteral("autoCommandBar")).toObject()
                 .value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("Autofill")).toString(),
             QStringLiteral("false"));
    // Разрешить получение ссылки на текущую строку: в блоке настроек
    // динамического списка это код 20 со значением ["B","1"]; в Form.xml
    // свойство пишется только при включении.
    QCOMPARE(findModelNode(model.value(QStringLiteral("childItemsRoot")).toArray(),
                           QStringLiteral("Список"))
                 .value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("AllowGettingCurrentRowURL")).toString(),
             QStringLiteral("true"));

    // Панель с автозаполнением (значение по умолчанию): свойства в модели нет,
    // рендерер сам подставляет стандартные команды.
    FormDocument withAutofill;
    QVERIFY2(loadForm(QStringLiteral("Справочник"),
                      QStringLiteral("БанкиУниверсальногоОбмена"),
                      QStringLiteral("ФормаЭлемента"), &withAutofill),
             qPrintable(error));
    model = QJsonDocument::fromJson(formWebModelJson(withAutofill)).object();
    QVERIFY(!model.value(QStringLiteral("autoCommandBar")).toObject()
                 .value(QStringLiteral("properties")).toObject()
                 .contains(QStringLiteral("Autofill")));

    // Группа кнопок «компактно»: платформа пишет Representation=Compact,
    // кнопки рисуются значками без подписей.
    FormDocument compact;
    QVERIFY2(loadForm(QStringLiteral("Отчет"),
                      QStringLiteral("АнализДвиженийДенежныхСредств"),
                      QStringLiteral("ФормаОтчета"), &compact),
             qPrintable(error));
    model = QJsonDocument::fromJson(formWebModelJson(compact)).object();
    QCOMPARE(findModelNode(model.value(QStringLiteral("childItemsRoot")).toArray(),
                           QStringLiteral("ГруппаРаботаВТаблице"))
                 .value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("Representation")).toString(),
             QStringLiteral("Compact"));
}

// Панель форматированного документа: в бинарной форме она пустая, а платформа
// заполняет её командами форматирования по CommandSource поля. Модель
// восстанавливает состав: «Шрифт» (шрифт, A+, A-), «Начертание», «Цвет»,
// вставка картинки и зачеркивание с выравниванием в подменю «Ещё».
void TestForm::formattedDocumentCommandBar()
{
    const QString sample = test::corpusFile(QStringLiteral("1Cv8_bp.cf"));
    if (sample.isEmpty())
        QSKIP("Большая конфигурация 1Cv8_bp.cf не найдена");

    ConfigurationReader reader;
    QString error;
    QVERIFY2(reader.open(sample, &error), qPrintable(error));

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

    FormDocument document;
    QVERIFY2(loadForm(QStringLiteral("Справочник"), QStringLiteral("Новости"),
                      QStringLiteral("ФормаДокумента"), &document),
             qPrintable(error));
    const QJsonObject model = QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonArray roots = model.value(QStringLiteral("childItemsRoot")).toArray();
    const QJsonObject bar =
        findModelNode(roots, QStringLiteral("КоманднаяПанельФорматированныйТекст"));
    QVERIFY(!bar.isEmpty());
    QCOMPARE(bar.value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("CommandSource")).toString(),
             QStringLiteral("Item.ФорматированныйТекст"));

    const QJsonArray children = bar.value(QStringLiteral("childItems")).toArray();
    QCOMPARE(children.size(), 9);
    const QJsonObject fontGroup = children.at(0).toObject();
    QCOMPARE(fontGroup.value(QStringLiteral("tag")).toString(), QStringLiteral("ButtonGroup"));
    QCOMPARE(fontGroup.value(QStringLiteral("name")).toString(), QStringLiteral("_fmtFont"));
    const QJsonArray fontButtons = fontGroup.value(QStringLiteral("childItems")).toArray();
    QCOMPARE(fontButtons.size(), 3);
    const QJsonObject increase = fontButtons.at(1).toObject()
                                     .value(QStringLiteral("properties")).toObject();
    QCOMPARE(increase.value(QStringLiteral("Title")).toString(),
             QStringLiteral("Увеличить шрифт"));
    QCOMPARE(increase.value(QStringLiteral("Picture")).toString(),
             QStringLiteral("StdPicture.IncreaseFontSize"));
    QCOMPARE(increase.value(QStringLiteral("Representation")).toString(),
             QStringLiteral("Picture"));
    const QJsonObject bold = children.at(1).toObject()
                                 .value(QStringLiteral("childItems")).toArray()
                                 .at(0).toObject().value(QStringLiteral("properties")).toObject();
    QCOMPARE(bold.value(QStringLiteral("CommandName")).toString(),
             QStringLiteral("Item.ФорматированныйТекст.StandardCommand.Bold"));
    // Зачеркивание и выравнивание платформа убирает из панели в подменю «Ещё».
    const QJsonObject last = children.at(children.size() - 1).toObject();
    QCOMPARE(last.value(QStringLiteral("name")).toString(), QStringLiteral("_fmtAlignJustify"));
    QCOMPARE(last.value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("LocationInCommandBar")).toString(),
             QStringLiteral("InAdditionalSubmenu"));
}

// Отображение таблицы и признаки строк: Representation (List/Tree/умолчание),
// представление подсказки, запрет менять состав и порядок строк, пропуск при
// вводе и DefaultItem. Все значения сверены с Form.xml корпуса.
void TestForm::tableRepresentationAndRowFlags()
{
    const QString sample = test::corpusFile(QStringLiteral("1Cv8_bp.cf"));
    if (sample.isEmpty())
        QSKIP("Большая конфигурация 1Cv8_bp.cf не найдена");

    ConfigurationReader reader;
    QString error;
    QVERIFY2(reader.open(sample, &error), qPrintable(error));

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

    const auto props = [](const QJsonObject &model, const QString &name) {
        return findModelNode(model.value(QStringLiteral("childItemsRoot")).toArray(), name)
            .value(QStringLiteral("properties")).toObject();
    };

    // Табличная часть документа: Representation=List, а подсказка выводится
    // сверху (ToolTipRepresentation=ShowTop). У соседней таблицы «Билеты»
    // отображение не задано — в модели свойства нет.
    FormDocument document;
    QVERIFY2(loadForm(QStringLiteral("Документ"), QStringLiteral("АвансовыйОтчет"),
                      QStringLiteral("ФормаДокумента"), &document),
             qPrintable(error));
    QJsonObject model = QJsonDocument::fromJson(formWebModelJson(document)).object();
    QCOMPARE(props(model, QStringLiteral("Товары"))
                 .value(QStringLiteral("Representation")).toString(),
             QStringLiteral("List"));
    QCOMPARE(props(model, QStringLiteral("Билеты"))
                 .value(QStringLiteral("ToolTipRepresentation")).toString(),
             QStringLiteral("ShowTop"));
    QVERIFY(!props(model, QStringLiteral("Билеты"))
                 .contains(QStringLiteral("Representation")));

    // Данные отчёта — дерево: Representation=Tree, а ввод в таблицу настроек
    // не переходит (SkipOnInput=false). Важность отображения группы кнопок
    // платформа пишет значением Usual (код 3).
    FormDocument report;
    QVERIFY2(loadForm(QStringLiteral("Отчет"),
                      QStringLiteral("АнализДвиженийДенежныхСредств"),
                      QStringLiteral("ФормаОтчета"), &report),
             qPrintable(error));
    model = QJsonDocument::fromJson(formWebModelJson(report)).object();
    const QJsonObject filter = props(model, QStringLiteral("Отборы"));
    QCOMPARE(filter.value(QStringLiteral("Representation")).toString(), QStringLiteral("Tree"));
    QCOMPARE(filter.value(QStringLiteral("SkipOnInput")).toString(), QStringLiteral("false"));
    QCOMPARE(props(model, QStringLiteral("ГруппаРаботаВТаблице"))
                 .value(QStringLiteral("DisplayImportance")).toString(),
             QStringLiteral("Usual"));

    // Дерево значений обработки: строки неизменяемы — платформа пишет
    // ChangeRowSet и ChangeRowOrder значением false.
    FormDocument processor;
    QVERIFY2(loadForm(QStringLiteral("Обработка"),
                      QStringLiteral("КлассификацияВыручкиПоОКВЭД"),
                      QStringLiteral("Форма"), &processor),
             qPrintable(error));
    model = QJsonDocument::fromJson(formWebModelJson(processor)).object();
    const QJsonObject valueTree = props(model, QStringLiteral("ДеревоДанных"));
    QCOMPARE(valueTree.value(QStringLiteral("Representation")).toString(), QStringLiteral("Tree"));
    QCOMPARE(valueTree.value(QStringLiteral("ChangeRowSet")).toString(), QStringLiteral("false"));
    QCOMPARE(valueTree.value(QStringLiteral("ChangeRowOrder")).toString(), QStringLiteral("false"));

    // Поле-число: кнопка прокрутки, предел значения и формат из записи макета;
    // поле года — формат редактирования.
    FormDocument calendar;
    QVERIFY2(loadForm(QStringLiteral("Справочник"), QStringLiteral("Календари"),
                      QStringLiteral("ФормаЭлемента"), &calendar),
             qPrintable(error));
    model = QJsonDocument::fromJson(formWebModelJson(calendar)).object();
    const QJsonObject cycleLength = props(model, QStringLiteral("ДлинаЦикла"));
    QCOMPARE(cycleLength.value(QStringLiteral("SpinButton")).toString(), QStringLiteral("true"));
    QCOMPARE(cycleLength.value(QStringLiteral("MinValue")).toString(), QStringLiteral("1"));
    QCOMPARE(props(model, QStringLiteral("НомерТекущегоГода"))
                 .value(QStringLiteral("EditFormat")).toString(),
             QStringLiteral("ЧГ="));

    // Оформление группы: ширина дочерних элементов (ChildItemsWidth) лежит
    // вторым полем записи макета, объединение (United) — двадцать первым,
    // сквозное выравнивание (ThroughAlign) — девятнадцатым.
    QCOMPARE(props(model, QStringLiteral("НастройкиГруппа"))
                 .value(QStringLiteral("ChildItemsWidth")).toString(),
             QStringLiteral("Equal"));
    QCOMPARE(props(model, QStringLiteral("РезультатЗаполненияГруппа"))
                 .value(QStringLiteral("ChildItemsWidth")).toString(),
             QStringLiteral("LeftWidest"));
    FormDocument advance;
    QVERIFY2(loadForm(QStringLiteral("Документ"), QStringLiteral("АвансовыйОтчет"),
                      QStringLiteral("ФормаДокумента"), &advance),
             qPrintable(error));
    const QJsonObject advanceModel =
        QJsonDocument::fromJson(formWebModelJson(advance)).object();
    QCOMPARE(props(advanceModel, QStringLiteral("ГруппаШапкаЛевая"))
                 .value(QStringLiteral("United")).toString(),
             QStringLiteral("false"));
    FormDocument inventory;
    QVERIFY2(loadForm(QStringLiteral("Документ"),
                      QStringLiteral("АктИнвентаризацииСАТУРН"),
                      QStringLiteral("ФормаДокумента"), &inventory),
             qPrintable(error));
    const QJsonObject inventoryModel =
        QJsonDocument::fromJson(formWebModelJson(inventory)).object();
    QCOMPARE(props(inventoryModel,
                   QStringLiteral("ГруппаНередактируемыеПослеОтправкиРеквизитыОсновное"))
                 .value(QStringLiteral("ThroughAlign")).toString(),
             QStringLiteral("Use"));

    // Линии сетки таблицы (HorizontalLines и VerticalLines, 27-е и 28-е поля
    // записи) и начальное раскрытие дерева (InitialTreeView, 34-е поле).
    // У шаблона заполнения календаря выключены обе сетки, у дерева «Список»
    // актов отбора развёрнут верхний уровень, у дерева отбора отчёта — все.
    FormDocument calendar2;
    QVERIFY2(loadForm(QStringLiteral("Справочник"), QStringLiteral("Календари"),
                      QStringLiteral("ФормаЭлемента"), &calendar2),
             qPrintable(error));
    const QJsonObject calendarModel =
        QJsonDocument::fromJson(formWebModelJson(calendar2)).object();
    const QJsonObject templateTable = props(calendarModel, QStringLiteral("ШаблонЗаполнения"));
    QCOMPARE(templateTable.value(QStringLiteral("HorizontalLines")).toString(),
             QStringLiteral("false"));
    QCOMPARE(templateTable.value(QStringLiteral("VerticalLines")).toString(),
             QStringLiteral("false"));
    FormDocument acts;
    QVERIFY2(loadForm(QStringLiteral("Справочник"), QStringLiteral("АктыОтбораПробЗЕРНО"),
                      QStringLiteral("ФормаСписка"), &acts),
             qPrintable(error));
    const QJsonObject actsModel = QJsonDocument::fromJson(formWebModelJson(acts)).object();
    QCOMPARE(props(actsModel, QStringLiteral("Список"))
                 .value(QStringLiteral("InitialTreeView")).toString(),
             QStringLiteral("ExpandTopLevel"));
    const QJsonObject reportModel =
        QJsonDocument::fromJson(formWebModelJson(report)).object();
    QCOMPARE(props(reportModel, QStringLiteral("Отборы"))
                 .value(QStringLiteral("InitialTreeView")).toString(),
             QStringLiteral("ExpandAllLevels"));

    // Путь картинки строки (RowPictureDataPath) пишется только таблице, у
    // которой в записи задано поле картинки: у динамического списка — стандартное
    // DefaultPicture, у таблицы значений — номер её колонки («ИндексКартинки»).
    // У списка перечисления путь записан с «~».
    QCOMPARE(props(actsModel, QStringLiteral("Список"))
                 .value(QStringLiteral("RowPictureDataPath")).toString(),
             QStringLiteral("Список.DefaultPicture"));
    QVERIFY(!props(advanceModel, QStringLiteral("Товары"))
                 .contains(QStringLiteral("RowPictureDataPath")));
    FormDocument upload;
    QVERIFY2(loadForm(QStringLiteral("Обработка"),
                      QStringLiteral("ЗагрузкаДокументовИзФайлов"),
                      QStringLiteral("Форма"), &upload),
             qPrintable(error));
    const QJsonObject uploadModel =
        QJsonDocument::fromJson(formWebModelJson(upload)).object();
    QCOMPARE(props(uploadModel, QStringLiteral("Файлы"))
                 .value(QStringLiteral("RowPictureDataPath")).toString(),
             QStringLiteral("Файлы.ИндексКартинки"));
    FormDocument enumList;
    QVERIFY2(loadForm(QStringLiteral("Перечисление"),
                      QStringLiteral("ВидыДвиженийДенежныхСредств"),
                      QStringLiteral("ФормаВыбора"), &enumList),
             qPrintable(error));
    const QJsonObject enumModel = QJsonDocument::fromJson(formWebModelJson(enumList)).object();
    QCOMPARE(props(enumModel, QStringLiteral("Список"))
                 .value(QStringLiteral("RowPictureDataPath")).toString(),
             QStringLiteral("~Список.DefaultPicture"));

    // Заголовок страницы из реквизита (TitleDataPath): платформа связывает
    // заголовок страницы с количеством строк табличной части. Путь лежит
    // четвёртым полем блока {18,…}, а стандартное поле RowsCount закодировано
    // номером 100000000; пустой путь записан как {0} — свойства в модели нет.
    QCOMPARE(props(advanceModel, QStringLiteral("ГруппаТовары"))
                 .value(QStringLiteral("TitleDataPath")).toString(),
             QStringLiteral("Объект.Товары.RowsCount"));
    QCOMPARE(props(inventoryModel, QStringLiteral("СтраницаТовары"))
                 .value(QStringLiteral("TitleDataPath")).toString(),
             QStringLiteral("Объект.Товары.RowsCount"));
    QVERIFY(!props(inventoryModel, QStringLiteral("Страницы"))
                 .contains(QStringLiteral("TitleDataPath")));

    // Разрешать изменение содержимого (EnableContentChange) — третье поле после
    // имени записи группы; платформа пишет свойство только значением true.
    const QJsonObject processorModel =
        QJsonDocument::fromJson(formWebModelJson(processor)).object();
    QCOMPARE(props(processorModel, QStringLiteral("ГруппаПериодСверки"))
                 .value(QStringLiteral("EnableContentChange")).toString(),
             QStringLiteral("true"));
    QVERIFY(!props(processorModel, QStringLiteral("ГруппаШапка"))
                 .contains(QStringLiteral("EnableContentChange")));

    // Вид переключателя (RadioButtonType): блок {8, {3, вид, …}} есть только у
    // переключателя; вид 1 — Auto, 2 — Tumbler.
    QCOMPARE(props(calendarModel, QStringLiteral("СпособЗаполненияПоНеделям"))
                 .value(QStringLiteral("RadioButtonType")).toString(),
             QStringLiteral("Auto"));

    // Пределы авторазмера декорации (AutoMaxWidth — двадцать первое поле после
    // имени, AutoMaxHeight — двадцать четвёртое): платформа пишет только false.
    const QJsonObject unitDecoration =
        props(calendarModel, QStringLiteral("ДлинаЦиклаЕдиницаИзмерения"));
    QCOMPARE(unitDecoration.value(QStringLiteral("AutoMaxWidth")).toString(),
             QStringLiteral("false"));
    QVERIFY(!unitDecoration.contains(QStringLiteral("AutoMaxHeight")));
    const QJsonObject resultText =
        props(calendarModel, QStringLiteral("РезультатЗаполненияИнформационныйТекст"));
    QCOMPARE(resultText.value(QStringLiteral("AutoMaxHeight")).toString(),
             QStringLiteral("false"));
    QCOMPARE(props(uploadModel, QStringLiteral("ПредложениеПеретащитьИзображение"))
                 .value(QStringLiteral("AutoMaxWidth")).toString(),
             QStringLiteral("false"));

    // Размер картинки декорации (PictureSize) — третье поле записи оформления
    // {4,…}: 1 — Stretch, 2 — Proportionally; у соседних декораций свойства нет.
    QCOMPARE(props(reportModel, QStringLiteral("Разделитель"))
                 .value(QStringLiteral("PictureSize")).toString(),
             QStringLiteral("Stretch"));
    QVERIFY(!props(reportModel, QStringLiteral("КартинкаБаннера"))
                 .contains(QStringLiteral("PictureSize")));

    // Пределы размера декорации: MaxWidth — двадцать второе поле после имени,
    // MaxHeight — двадцать пятое (в том же блоке, что и признаки авторазмера).
    const QJsonObject navigationPicture = props(uploadModel, QStringLiteral("НавигацияКартинка"));
    QCOMPARE(navigationPicture.value(QStringLiteral("MaxWidth")).toString(),
             QStringLiteral("90"));
    QCOMPARE(navigationPicture.value(QStringLiteral("MaxHeight")).toString(),
             QStringLiteral("3"));
    const QJsonObject waitingDecoration =
        props(processorModel, QStringLiteral("ДекорацияОжидание"));
    QCOMPARE(waitingDecoration.value(QStringLiteral("MaxWidth")).toString(),
             QStringLiteral("4"));
    QCOMPARE(waitingDecoration.value(QStringLiteral("MaxHeight")).toString(),
             QStringLiteral("1"));

    // Пределы размера таблицы стоят блоком в конце записи (AutoMaxWidth,
    // MaxWidth, AutoMaxHeight, MaxHeight): у шаблона заполнения календаря
    // авторазмер по ширине выключен, у соседней таблицы свойства нет.
    QCOMPARE(templateTable.value(QStringLiteral("AutoMaxWidth")).toString(),
             QStringLiteral("false"));
    QVERIFY(!templateTable.contains(QStringLiteral("MaxWidth")));
    QVERIFY(!props(actsModel, QStringLiteral("Список"))
                 .contains(QStringLiteral("AutoMaxWidth")));

    // Пределы размера кнопки — блок через двадцать девять полей после имени:
    // у кнопки «Показать быстрые настройки» авторазмер по ширине выключен.
    QCOMPARE(props(reportModel, QStringLiteral("ПоказатьБыстрыеНастройки"))
                 .value(QStringLiteral("AutoMaxWidth")).toString(),
             QStringLiteral("false"));
    QVERIFY(!props(reportModel, QStringLiteral("СформироватьОтчет"))
                 .contains(QStringLiteral("AutoMaxWidth")));
    // Кнопка по умолчанию (DefaultButton) — шестое поле после имени.
    QCOMPARE(props(reportModel, QStringLiteral("СформироватьОтчет"))
                 .value(QStringLiteral("DefaultButton")).toString(),
             QStringLiteral("true"));

    // Многострочный режим (MultiLine) — восьмое поле записи макета поля ввода:
    // 1 — true, 0 — false, 2 — значение не задано.
    FormDocument news;
    QVERIFY2(loadForm(QStringLiteral("Справочник"), QStringLiteral("Новости"),
                      QStringLiteral("ФормаДокумента"), &news),
             qPrintable(error));
    const QJsonObject newsModel = QJsonDocument::fromJson(formWebModelJson(news)).object();
    QCOMPARE(props(newsModel, QStringLiteral("БинарныеДанныеДанныеСтрока64"))
                 .value(QStringLiteral("MultiLine")).toString(),
             QStringLiteral("true"));
    QVERIFY(!props(newsModel, QStringLiteral("Наименование"))
                 .contains(QStringLiteral("MultiLine")));

    // Пределы размера HTML-документа лежат блоком в записи макета {3,…}:
    // у примеров новости на странице текстов выключены оба авторазмера.
    const QJsonObject newsExample =
        props(newsModel, QStringLiteral("ПримерНовостиДляВнешнихИсточников"));
    QCOMPARE(newsExample.value(QStringLiteral("AutoMaxWidth")).toString(),
             QStringLiteral("false"));
    QCOMPARE(newsExample.value(QStringLiteral("AutoMaxHeight")).toString(),
             QStringLiteral("false"));

    // Предел высоты надписи (MaxHeight) — девятнадцатое поле записи {11,…}.
    QCOMPARE(resultText.value(QStringLiteral("MaxHeight")).toString(),
             QStringLiteral("2"));
}

// «Видимость для пользователя», выравнивание содержимого страницы, выравнивание
// командной панели и картинки строк таблицы.
void TestForm::userVisiblePageAlignAndRowsPicture()
{
    const QString text = QStringLiteral(
        "{4,"
        "{50,0,"
        // Панель автозаполнения с выравниванием вправо: список из трёх чисел
        // стоит через 14 полей после имени, второе число — HorizontalAlign.
        "{22,{-1,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"9\","
        "\"ФормаКоманднаяПанель\",{1,0},{1,0},\"0\",\"1\",\"0\",\"0\",\"0\",\"2\",\"2\","
        "{\"3\",\"4\",{\"0\"}},{\"7\",\"3\",\"0\",\"1\",\"100\"},{\"0\",\"0\",\"0\"},\"1\","
        "{\"0\",\"2\",\"0\"}},"
        // Страница: выравнивание содержимого — 12-е и 13-е поля блока {18,…}.
        "{22,{1,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"4\",\"Страница1\","
        "{1,1,{\"ru\",\"Страница\"}},{1,0},\"0\",\"1\",\"0\",\"0\",\"0\",\"2\",\"2\","
        "{\"3\",\"4\",{\"0\"}},{\"7\",\"3\",\"0\",\"1\",\"100\"},{\"0\",\"0\",\"0\"},\"1\","
        "{\"18\",{\"4\",\"0\",{\"0\"},\"\",\"-1\",\"-1\",\"1\",\"0\",\"\"},\"0\",\"0\",{\"0\"},"
        "{\"1\",\"0\"},\"1\",\"Pattern\",\"\",{\"3\",\"4\",{\"0\"}},\"0\",\"0\",\"1\",\"1\","
        "\"0\",\"0\",\"0\",\"0\",{\"3\",\"4\",{\"0\"}},{\"7\",\"3\",\"0\",\"1\",\"100\"}}},"
        // Скрытое пользователем поле и обычное поле рядом с ним.
        "{37,{2,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"1\","
        "{0,{0,{\"B\",\"0\"},\"0\"}},\"1\",\"СкрытоеПоле\"},"
        "{37,{3,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"1\",\"ОбычноеПоле\"}"
        "},"
        "\"Процедура Тест()\\nКонецПроцедуры\","
        "{4,\"2\",{9,{\"1\"},\"0\",\"Атрибут1\",{1,1,{\"ru\",\"Атрибут 1\"}},"
        "{\"Pattern\",{\"S\"}}}},"
        "{0,0},"
        "{0,\"1\",{9,{\"1\"},\"Команда1\",{1,1,{\"ru\",\"Команда\"}}}}"
        "}");

    FormDocument document;
    QString error;
    QVERIFY2(FormReader::parseBytes(text.toUtf8(), &document, &error), qPrintable(error));

    // Запись {0,{0,{"B",0},"0"}} перед именем скрывает элемент уже при первом
    // показе формы: Form.xml пишет в этом случае UserVisible=false, хотя Visible
    // у элемента не задан.
    const FormItem *hidden = findItemByName(document.items, QStringLiteral("СкрытоеПоле"));
    QVERIFY(hidden);
    QCOMPARE(hidden->userVisible, 0);
    const FormItem *plain = findItemByName(document.items, QStringLiteral("ОбычноеПоле"));
    QVERIFY(plain);
    QCOMPARE(plain->userVisible, -1);
    const FormItem *page = findItemByName(document.items, QStringLiteral("Страница1"));
    QVERIFY(page);
    QCOMPARE(page->contentHorizontalAlign, 1);
    QCOMPARE(page->contentVerticalAlign, 1);

    const QJsonObject model = QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonArray roots = model.value(QStringLiteral("childItemsRoot")).toArray();
    const QJsonObject hiddenProps = findModelNode(roots, QStringLiteral("СкрытоеПоле"))
                                        .value(QStringLiteral("properties")).toObject();
    QCOMPARE(hiddenProps.value(QStringLiteral("UserVisible")).toString(),
             QStringLiteral("false"));
    QCOMPARE(hiddenProps.value(QStringLiteral("Visible")).toString(),
             QStringLiteral("false"));
    const QJsonObject plainProps = findModelNode(roots, QStringLiteral("ОбычноеПоле"))
                                       .value(QStringLiteral("properties")).toObject();
    QVERIFY(!plainProps.contains(QStringLiteral("UserVisible")));
    QVERIFY(!plainProps.contains(QStringLiteral("Visible")));
    const QJsonObject pageProps = findModelNode(roots, QStringLiteral("Страница1"))
                                      .value(QStringLiteral("properties")).toObject();
    QCOMPARE(pageProps.value(QStringLiteral("HorizontalAlign")).toString(),
             QStringLiteral("Center"));
    QCOMPARE(pageProps.value(QStringLiteral("VerticalAlign")).toString(),
             QStringLiteral("Center"));
    QCOMPARE(model.value(QStringLiteral("autoCommandBar")).toObject()
                 .value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("HorizontalAlign")).toString(),
             QStringLiteral("Right"));

    // «Картинки строк» таблицы: ссылка на общую картинку из записи картинки
    // {4,вид,{0,GUID},…} уходит в модель свойством RowsPicture.
    FormDocument tableDocument;
    FormItem table;
    table.type = FormItem::Type::Table;
    table.typeCode = 55;
    table.name = QStringLiteral("Таблица");
    table.rowsPicture = QStringLiteral("CommonPicture.Пиктограммы");
    tableDocument.items.append(table);
    const QJsonObject tableModel =
        QJsonDocument::fromJson(formWebModelJson(tableDocument)).object();
    QCOMPARE(findModelNode(tableModel.value(QStringLiteral("childItemsRoot")).toArray(),
                           QStringLiteral("Таблица"))
                 .value(QStringLiteral("properties")).toObject()
                 .value(QStringLiteral("RowsPicture")).toString(),
             QStringLiteral("CommonPicture.Пиктограммы"));
}

// Начальное раскрытие группы (Collapsed), скрытый заголовок страницы (ShowTitle),
// положение своей командной панели (HorizontalLocation) и источник команд группы
// кнопок (CommandSource).
void TestForm::collapsedPageTitleCommandBarAndSource()
{
    const QString text = QStringLiteral(
        "{4,"
        "{50,0,"
        // Раскрываемая группа со свёрнутым начальным состоянием: двенадцатое
        // поле узла макета {29,…} — Collapsed, одиннадцатое — признак
        // раскрываемой группы.
        "{22,{10,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"5\","
        "\"ГруппаСвернутая\",{1,0},{1,0},\"0\",\"1\",\"0\",\"0\",\"0\",\"2\",\"2\","
        "{\"3\",\"4\",{\"0\"}},{\"7\",\"3\",\"0\",\"1\",\"100\"},{\"0\",\"0\",\"0\"},\"1\","
        "{\"29\",\"0\",\"0\",\"0\",\"1\",{\"0\"},{\"1\",\"0\"},{\"Pattern\"},\"\","
        "{\"3\",\"4\",{\"0\"}},\"1\",\"1\",\"1\",\"1\",{\"1\",\"0\"},\"0\",\"0\",\"3\",\"3\","
        "\"2\",\"0\",\"1\",\"0\",{\"3\",\"4\",{\"0\"}},\"0\",\"2\",\"0\",\"0\",\"0\"}},"
        // Та же раскрываемая группа без Collapsed.
        "{22,{11,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"5\","
        "\"ГруппаРаскрытая\",{1,0},{1,0},\"0\",\"1\",\"0\",\"0\",\"0\",\"2\",\"2\","
        "{\"3\",\"4\",{\"0\"}},{\"7\",\"3\",\"0\",\"1\",\"100\"},{\"0\",\"0\",\"0\"},\"1\","
        "{\"29\",\"0\",\"0\",\"0\",\"1\",{\"0\"},{\"1\",\"0\"},{\"Pattern\"},\"\","
        "{\"3\",\"4\",{\"0\"}},\"1\",\"1\",\"0\",\"1\",{\"1\",\"0\"},\"0\",\"0\",\"3\",\"3\","
        "\"2\",\"0\",\"1\",\"0\",{\"3\",\"4\",{\"0\"}},\"0\",\"2\",\"0\",\"0\",\"0\"}},"
        // Своя командная панель: список из трёх чисел через 14 полей после
        // имени, второе число 2 — HorizontalLocation=Right.
        "{22,{12,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"0\","
        "\"ПанельСправа\",{1,0},{1,0},\"0\",\"1\",\"0\",\"0\",\"0\",\"2\",\"2\","
        "{\"3\",\"4\",{\"0\"}},{\"7\",\"3\",\"0\",\"1\",\"100\"},{\"0\",\"0\",\"0\"},\"1\","
        "{\"1\",\"2\",{\"0\"}}},"
        // Группа кнопок: источник команд — сама форма (GUID формы из записи имени).
        "{22,{13,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"6\","
        "\"ГруппаКнопок\",{1,0},{1,0},\"0\",\"1\",\"0\",\"0\",\"0\",\"2\",\"2\","
        "{\"3\",\"4\",{\"0\"}},{\"7\",\"3\",\"0\",\"1\",\"100\"},{\"0\",\"0\",\"0\"},\"1\","
        "{\"2\",{0,02023637-7868-4a5f-8576-835a76e0c9ba},\"2\",\"0\"}},"
        // Страница со скрытым заголовком: седьмое поле блока {18,…} равно нулю.
        "{22,{1,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"4\","
        "\"СтраницаБезЗаголовка\",{1,0},{1,0},\"0\",\"1\",\"0\",\"0\",\"0\",\"2\",\"2\","
        "{\"3\",\"4\",{\"0\"}},{\"7\",\"3\",\"0\",\"1\",\"100\"},{\"0\",\"0\",\"0\"},\"1\","
        "{\"18\",{\"4\",\"0\",{\"0\"}},\"0\",\"0\",{\"0\"},{\"1\",\"0\"},\"0\",\"Pattern\",\"\","
        "{\"3\",\"4\",{\"0\"}},\"0\",\"0\",\"1\",\"1\",\"0\",\"0\",\"0\",\"0\","
        "{\"3\",\"4\",{\"0\"}},{\"7\",\"3\",\"0\",\"1\",\"100\"}}}"
        "},"
        "\"Процедура Тест()\\nКонецПроцедуры\","
        "{4,\"2\",{9,{\"1\"},\"0\",\"Атрибут1\",{1,1,{\"ru\",\"Атрибут 1\"}},"
        "{\"Pattern\",{\"S\"}}}},"
        "{0,0},"
        "{0,\"1\",{9,{\"1\"},\"Команда1\",{1,1,{\"ru\",\"Команда\"}}}}"
        "}");

    FormDocument document;
    QString error;
    QVERIFY2(FormReader::parseBytes(text.toUtf8(), &document, &error), qPrintable(error));

    const FormItem *collapsed = findItemByName(document.items, QStringLiteral("ГруппаСвернутая"));
    QVERIFY(collapsed);
    QVERIFY(collapsed->collapsed);
    QCOMPARE(collapsed->behavior, FormGroupBehavior::Collapsible);
    const FormItem *expanded = findItemByName(document.items, QStringLiteral("ГруппаРаскрытая"));
    QVERIFY(expanded);
    QVERIFY(!expanded->collapsed);
    const FormItem *bar = findItemByName(document.items, QStringLiteral("ПанельСправа"));
    QVERIFY(bar);
    QCOMPARE(bar->commandBarHorizontalLocation, 2);
    const FormItem *buttons = findItemByName(document.items, QStringLiteral("ГруппаКнопок"));
    QVERIFY(buttons);
    QCOMPARE(buttons->commandSource, 0);
    const FormItem *page = findItemByName(document.items, QStringLiteral("СтраницаБезЗаголовка"));
    QVERIFY(page);
    QVERIFY(!page->showTitle);

    const QJsonObject model = QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonArray roots = model.value(QStringLiteral("childItemsRoot")).toArray();
    auto props = [&roots](const QString &name) {
        return findModelNode(roots, name).value(QStringLiteral("properties")).toObject();
    };
    QCOMPARE(props(QStringLiteral("ГруппаСвернутая")).value(QStringLiteral("Collapsed")).toString(),
             QStringLiteral("true"));
    QCOMPARE(props(QStringLiteral("ГруппаСвернутая")).value(QStringLiteral("Behavior")).toString(),
             QStringLiteral("Collapsible"));
    QVERIFY(!props(QStringLiteral("ГруппаРаскрытая")).contains(QStringLiteral("Collapsed")));
    QCOMPARE(props(QStringLiteral("ПанельСправа")).value(QStringLiteral("HorizontalLocation")).toString(),
             QStringLiteral("Right"));
    QCOMPARE(props(QStringLiteral("ГруппаКнопок")).value(QStringLiteral("CommandSource")).toString(),
             QStringLiteral("Form"));
    QCOMPARE(props(QStringLiteral("СтраницаБезЗаголовка")).value(QStringLiteral("ShowTitle")).toString(),
             QStringLiteral("false"));
}

// Место дополнений поиска таблицы: три поля перед вложенными записями
// (25-е с конца SearchStringLocation, 24-е ViewStatusLocation, 23-е
// SearchControlLocation). Единица — None, ноль — значение по умолчанию,
// которое платформа в Form.xml не пишет.
void TestForm::tableSearchLocations()
{
    const QString text = QStringLiteral(
        "{4,"
        "{50,0,"
        // Таблица со всеми тремя дополнениями поиска, скрытыми платформой.
        "{55,{1,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"1\",\"0\","
        "\"СписокСПоиском\","
        // Шестое и седьмое поля — DefaultItem, ChangeRowSet и ChangeRowOrder.
        "\"0\",\"0\",\"1\",\"1\",\"1\","
        "\"0\",\"0\",\"0\",\"0\",\"0\",\"1\",\"1\",\"1\",\"0\",\"0\",\"1\",\"0\","
        "\"0\",\"0\",\"0\",\"0\",\"0\",\"0\",\"0\",\"0\",\"0\",\"0\"},"
        // Та же таблица без свойств: те же поля, но нули на месте признаков.
        "{55,{2,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"1\",\"0\","
        "\"СписокБезПоиска\","
        "\"0\",\"0\",\"0\",\"0\",\"0\","
        "\"0\",\"0\",\"0\",\"0\",\"0\",\"1\",\"1\",\"1\",\"0\",\"0\",\"1\",\"0\","
        "\"0\",\"0\",\"0\",\"0\",\"0\",\"0\",\"0\",\"0\",\"0\",\"0\"}"
        "},"
        "\"Процедура Тест()\\nКонецПроцедуры\","
        "{4,\"2\",{9,{\"1\"},\"0\",\"Атрибут1\",{1,1,{\"ru\",\"Атрибут 1\"}},"
        "{\"Pattern\",{\"S\"}}}},"
        "{0,0},"
        "{0,\"1\",{9,{\"1\"},\"Команда1\",{1,1,{\"ru\",\"Команда\"}}}}"
        "}");

    FormDocument document;
    QString error;
    QVERIFY2(FormReader::parseBytes(text.toUtf8(), &document, &error), qPrintable(error));

    const FormItem *hidden = findItemByName(document.items, QStringLiteral("СписокСПоиском"));
    QVERIFY(hidden);
    QCOMPARE(hidden->searchStringLocation, 1);
    QCOMPARE(hidden->viewStatusLocation, 1);
    QCOMPARE(hidden->searchControlLocation, 1);
    const FormItem *shown = findItemByName(document.items, QStringLiteral("СписокБезПоиска"));
    QVERIFY(shown);
    QCOMPARE(shown->searchStringLocation, 0);
    QCOMPARE(shown->viewStatusLocation, 0);
    QCOMPARE(shown->searchControlLocation, 0);

    const QJsonObject model = QJsonDocument::fromJson(formWebModelJson(document)).object();
    const QJsonArray roots = model.value(QStringLiteral("childItemsRoot")).toArray();
    auto props = [&roots](const QString &name) {
        return findModelNode(roots, name).value(QStringLiteral("properties")).toObject();
    };
    QCOMPARE(props(QStringLiteral("СписокСПоиском")).value(QStringLiteral("SearchStringLocation")).toString(),
             QStringLiteral("None"));
    QCOMPARE(props(QStringLiteral("СписокСПоиском")).value(QStringLiteral("ViewStatusLocation")).toString(),
             QStringLiteral("None"));
    QCOMPARE(props(QStringLiteral("СписокСПоиском")).value(QStringLiteral("SearchControlLocation")).toString(),
             QStringLiteral("None"));
    QVERIFY(!props(QStringLiteral("СписокБезПоиска")).contains(QStringLiteral("SearchStringLocation")));
    QVERIFY(!props(QStringLiteral("СписокБезПоиска")).contains(QStringLiteral("ViewStatusLocation")));
    QVERIFY(!props(QStringLiteral("СписокБезПоиска")).contains(QStringLiteral("SearchControlLocation")));
}

// Ссылка на элемент формы — {номер, GUID}, и у группы номер часто равен 28
// или 29, то есть совпадает с кодом узла макета {28|29, …}. Макет должен
// находиться, а не ссылка: иначе группа теряет ориентацию, поведение,
// отступы и сквозное выравнивание.
void TestForm::layoutNodeIsNotItemReference()
{
    const QString text = QStringLiteral(
        "{4,{50,0,"
        // Группа с номером 29: макет записан кодом 29 и стоит после имени.
        "{22,{29,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"5\","
        "\"ГруппаВертикальная\",{1,1,{\"ru\",\"Вертикальная\"}},"
        "{29,\"0\",\"0\",\"0\",\"0\",{\"0\"},{\"1\",\"0\"},{\"Pattern\"},\"\","
        "{\"3\",\"4\",{\"0\"}},\"0\",\"0\",\"0\",\"0\",{\"1\",\"0\"},\"0\",\"0\","
        "\"3\",\"3\",\"2\",\"0\",\"1\",\"0\",{\"3\",\"4\",{\"0\"}},\"0\",\"2\",\"0\",\"0\"},"
        // Вложенная группа с другим номером и горизонтальным макетом.
        "{22,{30,02023637-7868-4a5f-8576-835a76e0c9ba},\"0\",\"0\",\"0\",\"5\","
        "\"ГруппаГоризонтальная\",{1,1,{\"ru\",\"Горизонтальная\"}},"
        "{28,\"1\",\"0\",\"0\",\"1\"}"
        "}"
        "}"
        "},"
        "\"\","
        "{4,\"0\"},"
        "{0,0},"
        "{0,\"1\",{9,{\"1\"},\"Команда1\",{1,1,{\"ru\",\"Команда\"}}}}"
        "}");

    FormDocument document;
    QString error;
    QVERIFY2(FormReader::parseBytes(text.toUtf8(), &document, &error), qPrintable(error));

    const FormItem *vertical =
        findItemByName(document.items, QStringLiteral("ГруппаВертикальная"));
    QVERIFY(vertical);
    QCOMPARE(vertical->id, 29);
    QCOMPARE(vertical->orientation, 0);
    QCOMPARE(vertical->groupMode, 0);
    QVERIFY(!vertical->showTitle);
    QCOMPARE(static_cast<int>(vertical->representation),
             static_cast<int>(FormGroupRepresentation::None));
    QCOMPARE(static_cast<int>(vertical->behavior),
             static_cast<int>(FormGroupBehavior::Usual));
    QVERIFY(vertical->hasBehavior);

    const FormItem *horizontal =
        findItemByName(document.items, QStringLiteral("ГруппаГоризонтальная"));
    QVERIFY(horizontal);
    QCOMPARE(horizontal->id, 30);
    QCOMPARE(horizontal->orientation, 1);
    QVERIFY(horizontal->showTitle);
}

} // namespace

int runFormTests(int argc, char **argv){    TestForm test;    return QTest::qExec(&test, argc, argv);
}

#include "test_form.moc"
