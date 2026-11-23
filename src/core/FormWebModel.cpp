#include "core/FormWebModel.h"

#include "core/FormDocument.h"
#include "core/FormOwner.h"
#include "core/FormPictures.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

#include <functional>

namespace v8reader {

namespace {

// Коды типа элемента в сериализации управляемой формы.
enum : int {
    kItemAutoCommandBar = 2,
    kItemSearch = 5,
    kItemDecoration = 12,
    kItemGroup = 22,
    kItemButton = 31,
    kItemField = 37,
    kItemTable = 55
};

// Коды вида группы (второй код в узле группы). Сверено с Form.xml по 1387
// группам 26 форм: каждый код встретился только со своим тегом и ни разу
// с чужим.
enum : int {
    kGroupCommandBar = 0,   // CommandBar  — своя командная панель формы/таблицы
    kGroupPopup = 1,
    kGroupColumnGroup = 2,
    kGroupPages = 3,
    kGroupPage = 4,
    kGroupUsual = 5,
    kGroupButtons = 6,
    kGroupContextMenu = 8,
    kGroupAutoCommandBar = 9 // AutoCommandBar — панель, которую заполняет платформа
};

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString groupText(int orientation)
{
    return orientation == 0 ? QStringLiteral("Vertical")
                            : QStringLiteral("Horizontal");
}

// Значение Group обычной группы: режим ориентации из узла макета
// (2 — «горизонтально, если помещается» — это значение по умолчанию,
// платформа Group в Form.xml не пишет).
QString groupModeText(int mode, int orientation)
{
    switch (mode) {
    case 0:
        return QStringLiteral("Vertical");
    case 1:
        return QStringLiteral("Horizontal");
    case 3:
        return QStringLiteral("AlwaysHorizontal");
    case 2:
        return QString();
    default:
        break;
    }
    return groupText(orientation);
}

// Выравнивание поля и колонки: платформа пишет свойство, только если оно
// отличается от значения по умолчанию. Коды общие для HorizontalAlign,
// HeaderHorizontalAlign и FooterHorizontalAlign: 0 — Left, 1 — Center,
// 2 — Right, 3 — Auto (по умолчанию, в XML свойства нет).
QString alignText(int code)
{
    switch (code) {
    case 0:
        return QStringLiteral("Left");
    case 1:
        return QStringLiteral("Center");
    case 2:
        return QStringLiteral("Right");
    default:
        return QString();
    }
}

QString verticalAlignText(int code)
{
    switch (code) {
    case 0:
        return QStringLiteral("Top");
    case 1:
        return QStringLiteral("Center");
    case 2:
        return QStringLiteral("Bottom");
    default:
        return QString();
    }
}

QString editModeText(int code)
{
    switch (code) {
    case 0:
        return QStringLiteral("Directly");
    case 2:
        return QStringLiteral("EnterOnInput");
    default:
        return QString();
    }
}

QString fixingInTableText(int code)
{
    switch (code) {
    case 1:
        return QStringLiteral("Left");
    case 2:
        return QStringLiteral("Right");
    default:
        return QString();
    }
}

QString toolTipRepresentationText(int code)
{
    switch (code) {
    case 3:
        return QStringLiteral("Button");
    case 5:
        return QStringLiteral("ShowTop");
    case 7:
        return QStringLiteral("ShowBottom");
    default:
        return QString();
    }
}

QString commandBarLocationText(int code)
{
    switch (code) {
    case 0:
        return QStringLiteral("None");
    case 2:
        return QStringLiteral("Top");
    case 3:
        return QStringLiteral("Bottom");
    default:
        return QString();
    }
}

// Представление страниц группы-страниц: 0 — None, 1 — TabsOnTop, 2 — TabsOnBottom.
// Значение по умолчанию — TabsOnTop, поэтому непрочитанный код (Pages без
// записи) тоже даёт TabsOnTop.
QString pagesRepresentationText(int code)
{
    switch (code) {
    case 0:
        return QStringLiteral("None");
    case 2:
        return QStringLiteral("TabsOnBottom");
    default:
        return QStringLiteral("TabsOnTop");
    }
}

QString representationText(FormGroupRepresentation representation)
{
    switch (representation) {
    case FormGroupRepresentation::Strong:
        return QStringLiteral("StrongSeparation");
    case FormGroupRepresentation::Normal:
        return QStringLiteral("NormalSeparation");
    case FormGroupRepresentation::None:
        return QStringLiteral("None");
    case FormGroupRepresentation::Weak:
        break;
    }
    // Слабое выделение у платформы — значение по умолчанию: в Form.xml оно
    // не пишется.
    return QString();
}

QString behaviorText(FormGroupBehavior behavior)
{
    switch (behavior) {
    case FormGroupBehavior::Collapsible:
        return QStringLiteral("Collapsible");
    case FormGroupBehavior::PopUp:
        return QStringLiteral("PopUp");
    case FormGroupBehavior::Usual:
        break;
    }
    return QStringLiteral("Usual");
}

QString titleLocationText(FormTitleLocation location)
{
    switch (location) {
    case FormTitleLocation::None:
        return QStringLiteral("None");
    case FormTitleLocation::Left:
        return QStringLiteral("Left");
    case FormTitleLocation::Top:
        return QStringLiteral("Top");
    case FormTitleLocation::Right:
        return QStringLiteral("Right");
    case FormTitleLocation::Bottom:
        return QStringLiteral("Bottom");
    case FormTitleLocation::Auto:
        break;
    }
    return QStringLiteral("Auto");
}

bool isServiceTooltip(const FormItem &item)
{
    // Расширенная подсказка элемента: в XCF это отдельный узел ExtendedTooltip,
    // рендереру он не нужен.
    return item.name.endsWith(QStringLiteral("РасширеннаяПодсказка"))
        || item.name.endsWith(QStringLiteral("ExtendedTooltip"));
}

bool isPictureDecoration(const FormItem &item)
{
    return item.pictureDecoration;
}

// Спецификация шрифта в модели рендерера. Ссылка на стиль записана именем
// («style:ЗаголовкиСтрокШрифт»), явно заданные атрибуты — значениями, а
// незаданные — null: тогда рендерер берёт их из стиля, как и для Form.xml.
QJsonObject fontSpec(const FormFont &font)
{
    auto attribute = [](int value) {
        if (value < 0)
            return QJsonValue(QJsonValue::Null);
        return QJsonValue(value != 0);
    };
    QJsonObject spec;
    spec.insert(QStringLiteral("kind"), QStringLiteral("StyleItem"));
    spec.insert(QStringLiteral("ref"), font.reference);
    spec.insert(QStringLiteral("faceName"), QString());
    spec.insert(QStringLiteral("height"), font.height);
    spec.insert(QStringLiteral("scale"), font.scale);
    spec.insert(QStringLiteral("bold"), attribute(font.bold));
    spec.insert(QStringLiteral("italic"), attribute(font.italic));
    spec.insert(QStringLiteral("underline"), attribute(font.underline));
    spec.insert(QStringLiteral("strikeout"), attribute(font.strikeout));
    return spec;
}

// Оформление элемента: цвета и шрифты. Значения — те же строки, что пишет
// Form.xml: «style:Имя» (стиль конфигурации или платформы) либо «#RRGGBB».
void addStyleProperties(const FormItem &item, QJsonObject *properties)
{
    if (!item.textColor.isEmpty())
        properties->insert(QStringLiteral("TextColor"), item.textColor);
    if (!item.titleTextColor.isEmpty())
        properties->insert(QStringLiteral("TitleTextColor"), item.titleTextColor);
    if (!item.backColor.isEmpty())
        properties->insert(QStringLiteral("BackColor"), item.backColor);
    if (!item.borderColor.isEmpty())
        properties->insert(QStringLiteral("BorderColor"), item.borderColor);
    if (!item.font.isEmpty())
        properties->insert(QStringLiteral("FontSpec"), fontSpec(item.font));
    if (!item.titleFont.isEmpty())
        properties->insert(QStringLiteral("TitleFontSpec"), fontSpec(item.titleFont));
}

// Элемент, который имеет смысл отдавать рендереру.
bool isRenderable(const FormItem &item)
{
    switch (item.type) {
    case FormItem::Type::Group:
        // Панель, заполняемую платформой, рендерер получает отдельным узлом
        // autoCommandBar, а не элементом дерева.
        return item.groupKind != kGroupContextMenu
            && item.groupKind != kGroupAutoCommandBar;
    case FormItem::Type::Field:
    case FormItem::Type::Button:
    case FormItem::Type::Table:
    case FormItem::Type::Search:
        return true;
    case FormItem::Type::Decoration:
        return !isServiceTooltip(item);
    case FormItem::Type::AutoCommandBar:
    case FormItem::Type::Column:
    case FormItem::Type::Other:
        break;
    }
    return false;
}

// Имя команды по имени кнопки: платформа называет кнопки, порождённые
// общими командами, «ОбщаяКоманда<Имя>», а команды формы — «ФормаКоманда<Имя>».
QString commandNameForButton(const FormItem &item, const FormDocument &document)
{
    // Разобранная ссылка кнопки на команду: имя команды формы по её
    // идентификатору, стандартная команда платформы по GUID, стандартная
    // команда элемента-владельца.
    if (!item.commandName.isEmpty())
        return item.commandName;
    for (const FormCommand &command : document.commands) {
        if (command.name == item.name) {
            return QStringLiteral("Form.Command.") + command.name;
        }
    }
    static const QString commonPrefix = QStringLiteral("ОбщаяКоманда");
    if (item.name.startsWith(commonPrefix) && item.name.size() > commonPrefix.size()) {
        return QStringLiteral("CommonCommand.")
            + item.name.mid(commonPrefix.size());
    }
    static const QString formPrefix = QStringLiteral("ФормаОбщаяКоманда");
    if (item.name.startsWith(formPrefix) && item.name.size() > formPrefix.size()) {
        return QStringLiteral("CommonCommand.")
            + item.name.mid(formPrefix.size());
    }
    if (item.name == QStringLiteral("ФормаСправка"))
        return QStringLiteral("Form.StandardCommand.Help");
    if (item.name == QStringLiteral("ФормаПоказатьКонтекстныеНовости"))
        return QStringLiteral("Form.Command.ПоказатьКонтекстныеНовости");
    return QString();
}

// Пиктограмма платформенной команды, которую 1С выводит на командную панель
// одной картинкой: заголовок становится подсказкой, поэтому он же служит
// текстом пункта в «Ещё».
struct StandardBarCommand
{
    QString name;    // имя общей команды без префикса CommonCommand.
    QString title;   // заголовок и подсказка кнопки
    QString picture; // картинка платформенной библиотеки (std-pictures/)
};

const StandardBarCommand *standardBarCommand(const QString &commandName)
{
    static const QString prefix = QStringLiteral("CommonCommand.");
    static const QVector<StandardBarCommand> commands = {
        {QStringLiteral("ПоказатьДвиженияДокумента"),
         QStringLiteral("Движения документа"),
         QStringLiteral("StdPicture.ДебетКредит")},
        {QStringLiteral("СтруктураПодчиненности"),
         QStringLiteral("Структура подчинённости"),
         QStringLiteral("StdPicture.СтруктураПодчиненности")},
        {QStringLiteral("ПрисоединенныеФайлы"),
         QStringLiteral("Присоединённые файлы"),
         QStringLiteral("StdPicture.Attach")},
        {QStringLiteral("ДополнительныеСведенияКоманднаяПанель"),
         QStringLiteral("Дополнительные сведения"),
         QStringLiteral("StdPicture.ДополнительныеСведения")},
        {QStringLiteral("ИсторияИзменений"),
         QStringLiteral("История изменений"),
         QStringLiteral("StdPicture.DataHistory")}
    };
    if (!commandName.startsWith(prefix))
        return nullptr;
    const QString name = commandName.mid(prefix.size());
    for (const StandardBarCommand &command : commands) {
        if (command.name == name)
            return &command;
    }
    return nullptr;
}

QString elementTag(const FormItem &item, bool insideTable = false)
{
    switch (item.type) {
    case FormItem::Type::Group:
        switch (item.groupKind) {
        case kGroupPopup:
            return QStringLiteral("Popup");
        case kGroupColumnGroup:
            return QStringLiteral("ColumnGroup");
        case kGroupPages:
            return QStringLiteral("Pages");
        case kGroupPage:
            return QStringLiteral("Page");
        case kGroupButtons:
            return QStringLiteral("ButtonGroup");
        case kGroupCommandBar:
            return QStringLiteral("CommandBar");
        case kGroupAutoCommandBar:
            return QStringLiteral("AutoCommandBar");
        case kGroupUsual:
            return QStringLiteral("UsualGroup");
        default:
            break;
        }
        return insideTable ? QStringLiteral("ColumnGroup") : QStringLiteral("UsualGroup");
    case FormItem::Type::Field:
        // Вид поля берём из самой формы: код вида записан в ней отдельным
        // числом. Это точнее вывода из типа реквизита — им пользуемся только
        // когда код не прочитался.
        switch (item.fieldKind) {
        case FormFieldKind::Label:
            return QStringLiteral("LabelField");
        case FormFieldKind::Input:
            return QStringLiteral("InputField");
        case FormFieldKind::CheckBox:
            return QStringLiteral("CheckBoxField");
        case FormFieldKind::Picture:
            return QStringLiteral("PictureField");
        case FormFieldKind::RadioButton:
            return QStringLiteral("RadioButtonField");
        case FormFieldKind::SpreadSheetDocument:
            return QStringLiteral("SpreadSheetDocumentField");
        case FormFieldKind::TextDocument:
            return QStringLiteral("TextDocumentField");
        case FormFieldKind::Calendar:
            return QStringLiteral("CalendarField");
        case FormFieldKind::ProgressBar:
            return QStringLiteral("ProgressBarField");
        case FormFieldKind::TrackBar:
            return QStringLiteral("TrackBarField");
        case FormFieldKind::Chart:
            return QStringLiteral("ChartField");
        case FormFieldKind::GraphicalSchema:
            return QStringLiteral("GraphicalSchemaField");
        case FormFieldKind::HTMLDocument:
            return QStringLiteral("HTMLDocumentField");
        case FormFieldKind::FormattedDocument:
            return QStringLiteral("FormattedDocumentField");
        case FormFieldKind::Planner:
            return QStringLiteral("PlannerField");
        case FormFieldKind::PDFDocument:
            return QStringLiteral("PDFDocumentField");
        default:
            break;
        }
        // Порядок важен: булево значение рисуется флажком даже если у элемента
        // нет поля ввода (labelOnly), а картинка — PictureField.
        if (item.valueKind == FormValueKind::Boolean)
            return QStringLiteral("CheckBoxField");
        if (item.labelOnly && item.valueKind == FormValueKind::Picture)
            return QStringLiteral("PictureField");
        if (item.labelOnly)
            return QStringLiteral("LabelField");
        return QStringLiteral("InputField");
    case FormItem::Type::Button:
        return QStringLiteral("Button");
    case FormItem::Type::Table:
        return QStringLiteral("Table");
    case FormItem::Type::Decoration:
        return item.pictureDecoration ? QStringLiteral("PictureDecoration")
                                      : QStringLiteral("LabelDecoration");
    case FormItem::Type::Search:
        // Дополнение панели: строка поиска таблицы, состояние просмотра или
        // управление поиском. Код вида записан так же, как у групп, — перед
        // именем (0, 1 и 2 соответственно).
        switch (item.groupKind) {
        case 1:
            return QStringLiteral("ViewStatusAddition");
        case 2:
            return QStringLiteral("SearchControlAddition");
        default:
            return QStringLiteral("SearchStringAddition");
        }
    default:
        break;
    }
    return QStringLiteral("UsualGroup");
}

QJsonObject makeItem(const FormItem &item, const FormDocument &document);

QString kindTagFor(const QString &ownerTitle);

QJsonObject buildCommandBarNode(const FormItem *bar, const FormDocument &document,
                                const QJsonArray &leading, bool autofill,
                                const QString &defaultName);

// Своя командная панель форматированного документа: платформа заполняет её
// командами форматирования (шрифт, начертание, выравнивание, цвет, картинка),
// а в бинарной форме сама панель пустая — состав команд задаёт CommandSource
// на поле форматированного документа. Рендерер собирает те же кнопки из
// CommandSource, поэтому пустую панель достаточно связать с полем рядом с ней.
void linkFormattedDocumentBars(QJsonArray *children)
{
    QString fieldName;
    for (int i = 0; i < children->size(); ++i) {
        const QJsonObject entry = children->at(i).toObject();
        if (entry.value(QStringLiteral("tag")).toString()
            == QLatin1String("FormattedDocumentField")) {
            fieldName = entry.value(QStringLiteral("name")).toString();
            break;
        }
    }
    if (fieldName.isEmpty())
        return;
    for (int i = 0; i < children->size(); ++i) {
        QJsonObject entry = children->at(i).toObject();
        if (entry.value(QStringLiteral("tag")).toString() != QLatin1String("CommandBar"))
            continue;
        if (!entry.value(QStringLiteral("childItems")).toArray().isEmpty())
            continue;
        const QJsonObject source = entry.value(QStringLiteral("properties")).toObject();
        if (source.contains(QStringLiteral("CommandSource")))
            continue;
        QJsonObject properties = source;
        properties.insert(QStringLiteral("CommandSource"),
                          QStringLiteral("Item.") + fieldName);
        entry.insert(QStringLiteral("properties"), properties);
        entry.insert(QStringLiteral("authoredProperties"), properties);
        (*children)[i] = entry;
    }
}

void appendChildren(const QVector<FormItem> &items, const FormDocument &document,
                    QJsonArray *target)
{
    for (const FormItem &item : items) {
        if (!isRenderable(item))
            continue;
        if (item.type == FormItem::Type::AutoCommandBar)
            continue;
        target->append(makeItem(item, document));
    }
    linkFormattedDocumentBars(target);
}

QString buttonTitle(const FormItem &item)
{
    // Собственный заголовок кнопки из формы. Подписи стандартных команд и
    // команд формы рендерер выводит из CommandName, а заголовок из имени
    // кнопки был бы неверным («Записать изакрыть»).
    return item.synonym;
}

QJsonObject makeButton(const FormItem &item, const FormDocument &document)
{
    QJsonObject properties;
    // Вид кнопки: 0 — CommandBarButton (по умолчанию), 1 — UsualButton,
    // 2 — Hyperlink. Сверено с Form.xml по 294 кнопкам корпуса.
    QString buttonType = QStringLiteral("CommandBarButton");
    if (item.buttonType == 1)
        buttonType = QStringLiteral("UsualButton");
    else if (item.buttonType == 2)
        buttonType = QStringLiteral("Hyperlink");
    properties.insert(QStringLiteral("Type"), buttonType);
    // Отображение: 0 — Text, 1 — Picture, 2 — PictureAndText; 3 — Auto
    // (значение по умолчанию, в XML не пишется).
    if (item.buttonRepresentation >= 0 && item.buttonRepresentation <= 2) {
        static const char *const representations[] = {"Text", "Picture",
                                                      "PictureAndText"};
        properties.insert(QStringLiteral("Representation"),
                          QString::fromLatin1(representations[item.buttonRepresentation]));
    }
    if (item.skipOnInput >= 0)
        properties.insert(QStringLiteral("SkipOnInput"), boolText(item.skipOnInput != 0));
    if (item.displayImportance == 3)
        properties.insert(QStringLiteral("DisplayImportance"), QStringLiteral("Usual"));
    const auto groupAlign = [](int code) -> QString {
        switch (code) {
        case 0:
            return QStringLiteral("Left");
        case 1:
            return QStringLiteral("Center");
        case 2:
            return QStringLiteral("Right");
        default:
            return QString();
        }
    };
    if (const QString align = groupAlign(item.buttonGroupHorizontalAlign);
        !align.isEmpty()) {
        properties.insert(QStringLiteral("GroupHorizontalAlign"), align);
    }
    // Вертикальное выравнивание кнопки — Top/Center/Bottom, поэтому его код
    // переводится отдельно: та же цифра 2 значит «Bottom», а не «Right».
    if (const QString align = verticalAlignText(item.buttonGroupVerticalAlign);
        !align.isEmpty()) {
        properties.insert(QStringLiteral("GroupVerticalAlign"), align);
    }
    const QString commandName = commandNameForButton(item, document);
    if (!commandName.isEmpty())
        properties.insert(QStringLiteral("CommandName"), commandName);
    const QString title = buttonTitle(item);
    if (!title.isEmpty())
        properties.insert(QStringLiteral("Title"), title);
    if (!item.tooltip.isEmpty())
        properties.insert(QStringLiteral("ToolTip"), item.tooltip);
    // Собственная картинка кнопки: общая картинка конфигурации или
    // стандартная картинка платформы. Пиктограммы платформенных команд
    // (см. ниже) важнее — их подставляет рендерер по имени команды.
    if (!item.pictureRef.isEmpty())
        properties.insert(QStringLiteral("Picture"), item.pictureRef);
    // Платформенные команды («Движения документа», «Структура подчинённости»,
    // «Присоединённые файлы», «Дополнительные сведения», «История изменений»)
    // клиент выводит на панель одними пиктограммами: заголовок остаётся
    // подсказкой кнопки и текстом пункта в «Ещё».
    if (const StandardBarCommand *appearance = standardBarCommand(commandName)) {
        properties.insert(QStringLiteral("Title"), appearance->title);
        properties.insert(QStringLiteral("Representation"), QStringLiteral("Picture"));
        properties.insert(QStringLiteral("Picture"), appearance->picture);
    }
    if (!item.visible)
        properties.insert(QStringLiteral("Visible"), boolText(false));
    if (!item.onCommandBar)
        properties.insert(QStringLiteral("LocationInCommandBar"),
                          QStringLiteral("InAdditionalSubmenu"));
    addStyleProperties(item, &properties);

    QJsonObject json;
    json.insert(QStringLiteral("tag"), QStringLiteral("Button"));
    json.insert(QStringLiteral("name"), item.name);
    json.insert(QStringLiteral("id"), QString::number(item.id));
    json.insert(QStringLiteral("properties"), properties);
    json.insert(QStringLiteral("authoredProperties"), properties);
    json.insert(QStringLiteral("childItems"), QJsonArray());
    json.insert(QStringLiteral("events"), QJsonArray());
    return json;
}

// Путь к данным колонки таблицы в форме записан от корня формы
// («Объект.?.Количество»). Неразобранный сегмент — это табличная часть, в
// которой лежит колонка: без него рендерер не найдёт заголовок реквизита в
// objectMeta.captions, поэтому для колонок заголовок подставляется явно
// (dataTitle), а путь с «?» не отдаётся.
QString columnDataPath(const QString &dataPath)
{
    if (dataPath.contains(QLatin1Char('?')))
        return QString();
    return dataPath;
}

QJsonObject makeField(const FormItem &item, const FormDocument &document, bool insideTable,
                      const QString &tableName = QString())
{
    Q_UNUSED(tableName);
    QJsonObject properties;
    const QString dataPath = insideTable ? columnDataPath(item.dataPathText)
                                         : item.dataPathText;
    if (!dataPath.isEmpty())
        properties.insert(QStringLiteral("DataPath"), dataPath);

    // Заголовок колонки таблицы рендерер выводит из DataPath и метаданных,
    // поэтому собственный заголовок отдаём только если он задан в форме:
    // иначе стандартная колонка «Номер строки» получила бы подпись
    // «Выданные авансы номер строки» вместо «N».
    // Заголовок поля отдаём только если он задан в самой форме: иначе
    // подпись платформа (и рендерер) выводит из пути к данным и метаданных,
    // а заголовок из метаданных в модели был бы лишним и мог не совпасть
    // с подписью платформы.
    const QString title = item.synonym;
    if (!title.isEmpty())
        properties.insert(QStringLiteral("Title"), title);
    if (!item.tooltip.isEmpty())
        properties.insert(QStringLiteral("ToolTip"), item.tooltip);
    // Флажок: вид поля из формы важнее типа реквизита — у надписи с булевым
    // реквизитом флажка нет, а у привязанного к таблице флажка тип может быть
    // неизвестен. CheckBoxType платформа пишет как Auto, если не задан
    // переключатель.
    if (item.fieldKind == FormFieldKind::CheckBox
        || (item.valueKind == FormValueKind::Boolean
            && item.fieldKind != FormFieldKind::Label && !item.labelOnly)) {
        properties.insert(QStringLiteral("CheckBoxType"), QStringLiteral("Auto"));
    }
    // EditMode: 0 — Directly, 2 — EnterOnInput; 1 — значение по умолчанию,
    // платформа свойства в Form.xml не пишет. Пишется и у флажков: в форме
    // у них тоже есть режим редактирования.
    if (const QString editMode = editModeText(item.editMode); !editMode.isEmpty())
        properties.insert(QStringLiteral("EditMode"), editMode);
    if (!item.showTitle)
        properties.insert(QStringLiteral("TitleLocation"), QStringLiteral("None"));
    else if (item.titleLocation != FormTitleLocation::Auto
             && item.titleLocation != FormTitleLocation::Left) {
        properties.insert(QStringLiteral("TitleLocation"),
                          titleLocationText(item.titleLocation));
    }
    if (item.widthUnits > 0) {
        properties.insert(QStringLiteral("Width"),
                          QString::number(item.widthUnits));
    }
    if (item.heightUnits > 0) {
        properties.insert(QStringLiteral("Height"),
                          QString::number(item.heightUnits));
    }
    // Растяжение поля: платформа пишет свойство только при отличии от
    // значения по умолчанию. У полей ввода, надписей и флажков это Auto (2),
    // у поля-картинки и календаря — true (1): так в корпусе (610 полей), иначе
    // у 17 картинок появлялось лишнее HorizontalStretch=true.
    const bool stretchTrueByDefault = item.fieldKind == FormFieldKind::Picture
        || item.fieldKind == FormFieldKind::Calendar;
    const int stretchDefault = stretchTrueByDefault ? 1 : 2;
    const auto stretchText = [stretchDefault](int code) {
        if (code < 0 || code == stretchDefault)
            return QString();
        return code == 0 ? QStringLiteral("false")
             : code == 1 ? QStringLiteral("true")
                         : QStringLiteral("Auto");
    };
    if (const QString stretch = stretchText(item.horizontalStretch); !stretch.isEmpty())
        properties.insert(QStringLiteral("HorizontalStretch"), stretch);
    if (const QString stretch = stretchText(item.verticalStretch); !stretch.isEmpty())
        properties.insert(QStringLiteral("VerticalStretch"), stretch);
    if (!item.visible)
        properties.insert(QStringLiteral("Visible"), QStringLiteral("false"));

    // Свойства поля и колонки таблицы, которые платформа пишет в Form.xml
    // только при отличии от значения по умолчанию: серые ячейки только для
    // просмотра, ячейки-ссылки, закрепление колонки, скрытые шапка и подвал,
    // выравнивание, пропуск при вводе и подсказка ввода.
    if (item.readOnly)
        properties.insert(QStringLiteral("ReadOnly"), boolText(true));
    if (!item.showInHeader)
        properties.insert(QStringLiteral("ShowInHeader"), boolText(false));
    if (!item.showInFooter)
        properties.insert(QStringLiteral("ShowInFooter"), boolText(false));
    if (item.cellHyperlink)
        properties.insert(QStringLiteral("CellHyperlink"), boolText(true));
    if (item.skipOnInput >= 0)
        properties.insert(QStringLiteral("SkipOnInput"), boolText(item.skipOnInput != 0));
    if (item.defaultItem)
        properties.insert(QStringLiteral("DefaultItem"), boolText(true));
    if (item.autoCellHeight)
        properties.insert(QStringLiteral("AutoCellHeight"), boolText(true));
    if (const QString fixing = fixingInTableText(item.fixingInTable); !fixing.isEmpty())
        properties.insert(QStringLiteral("FixingInTable"), fixing);
    if (const QString align = alignText(item.horizontalAlign); !align.isEmpty())
        properties.insert(QStringLiteral("HorizontalAlign"), align);
    // У заголовка и подвала колонки значение по умолчанию — Auto (3), поэтому
    // Left — это код 0, Center — 1, Right — 2.
    if (const QString align = alignText(item.headerHorizontalAlign);
        !align.isEmpty() && item.headerHorizontalAlign != 0) {
        properties.insert(QStringLiteral("HeaderHorizontalAlign"), align);
    }
    if (const QString align = alignText(item.footerHorizontalAlign); !align.isEmpty())
        properties.insert(QStringLiteral("FooterHorizontalAlign"), align);
    if (const QString align = verticalAlignText(item.verticalAlign); !align.isEmpty())
        properties.insert(QStringLiteral("VerticalAlign"), align);
    // Выравнивание поля внутри группы: платформа пишет эти свойства только при
    // отличии от Auto, поэтому сюда доходят коды 0..2.
    if (const QString align = alignText(item.groupHorizontalAlign); !align.isEmpty())
        properties.insert(QStringLiteral("GroupHorizontalAlign"), align);
    if (const QString align = verticalAlignText(item.groupVerticalAlign);
        !align.isEmpty()) {
        properties.insert(QStringLiteral("GroupVerticalAlign"), align);
    }
    if (const QString representation = toolTipRepresentationText(item.toolTipRepresentation);
        !representation.isEmpty()) {
        properties.insert(QStringLiteral("ToolTipRepresentation"), representation);
    }
    if (!item.inputHint.isEmpty())
        properties.insert(QStringLiteral("InputHint"), item.inputHint);

    // Остальные свойства поля из записи макета: платформа пишет их в Form.xml
    // только при отличии от значения по умолчанию. Сверено с Form.xml по
    // 326 полям ввода и 56 полям-картинкам корпуса.
    if (!item.autoMaxWidth)
        properties.insert(QStringLiteral("AutoMaxWidth"), boolText(false));
    if (!item.autoMaxHeight)
        properties.insert(QStringLiteral("AutoMaxHeight"), boolText(false));
    if (item.maxWidth > 0)
        properties.insert(QStringLiteral("MaxWidth"), QString::number(item.maxWidth));
    if (!item.wrap)
        properties.insert(QStringLiteral("Wrap"), boolText(false));
    const auto triBool = [&properties](const QString &name, int value) {
        if (value == 0 || value == 1)
            properties.insert(name, boolText(value == 1));
    };
    triBool(QStringLiteral("ChoiceButton"), item.choiceButton);
    triBool(QStringLiteral("ClearButton"), item.clearButton);
    triBool(QStringLiteral("SpinButton"), item.spinButton);
    triBool(QStringLiteral("OpenButton"), item.openButton);
    triBool(QStringLiteral("DropListButton"), item.dropListButton);
    triBool(QStringLiteral("ChoiceListButton"), item.choiceListButton);
    triBool(QStringLiteral("AutoMarkIncomplete"), item.autoMarkIncomplete);
    if (item.autoChoiceIncomplete == 1)
        properties.insert(QStringLiteral("AutoChoiceIncomplete"), boolText(true));
    if (!item.chooseType)
        properties.insert(QStringLiteral("ChooseType"), boolText(false));
    if (!item.textEdit)
        properties.insert(QStringLiteral("TextEdit"), boolText(false));
    if (item.listChoiceMode)
        properties.insert(QStringLiteral("ListChoiceMode"), boolText(true));
    if (item.choiceHistoryOnInput == 1) {
        properties.insert(QStringLiteral("ChoiceHistoryOnInput"),
                          QStringLiteral("DontUse"));
    }
    if (item.choiceButtonRepresentation == 2 || item.choiceButtonRepresentation == 3) {
        properties.insert(QStringLiteral("ChoiceButtonRepresentation"),
                          item.choiceButtonRepresentation == 3
                              ? QStringLiteral("ShowInInputField")
                              : QStringLiteral("ShowInDropListAndInInputField"));
    }
    if (item.choiceFoldersAndItems >= 0 && item.choiceFoldersAndItems <= 2) {
        static const char *const names[] = {"Items", "Folders", "FoldersAndItems"};
        properties.insert(QStringLiteral("ChoiceFoldersAndItems"),
                          QString::fromLatin1(names[item.choiceFoldersAndItems]));
    }
    // Режим перетаскивания файлов: 0 — AsFile; 1 — UseAsFile (значение по
    // умолчанию, в XML не пишется). Сверено с Form.xml по 56 полям-картинкам.
    if (item.fileDragMode == 0)
        properties.insert(QStringLiteral("FileDragMode"), QStringLiteral("AsFile"));
    if (item.fieldKind == FormFieldKind::Picture && item.hyperlink)
        properties.insert(QStringLiteral("Hyperlink"), boolText(true));

    // Подписи списка выбора: у переключателя по ним рисуется одна радиокнопка
    // на элемент (список без представлений приходит из метаданных — его не
    // отдаём, иначе рендерер нарисует пустые подписи).
    if (!item.choiceItems.isEmpty()) {
        QJsonArray choiceItems;
        bool hasText = false;
        for (const QString &text : item.choiceItems) {
            choiceItems.append(text);
            hasText = hasText || !text.isEmpty();
        }
        if (hasText)
            properties.insert(QStringLiteral("ChoiceListItems"), choiceItems);
    }

    // Картинка значений поля и значок в заголовке колонки — ссылками на
    // общие картинки конфигурации («CommonPicture.Имя»).
    if (!item.valuesPictureRef.isEmpty())
        properties.insert(QStringLiteral("ValuesPicture"), item.valuesPictureRef);
    if (!item.headerPictureRef.isEmpty())
        properties.insert(QStringLiteral("HeaderPicture"), item.headerPictureRef);

    addStyleProperties(item, &properties);

    QJsonObject json;
    json.insert(QStringLiteral("tag"), elementTag(item));
    json.insert(QStringLiteral("name"), item.name);
    json.insert(QStringLiteral("id"), QString::number(item.id));
    json.insert(QStringLiteral("properties"), properties);
    json.insert(QStringLiteral("authoredProperties"), properties);
    if (item.children.isEmpty()) {
        json.insert(QStringLiteral("childItems"), QJsonArray());
    } else {
        QJsonArray children;
        for (const FormItem &child : item.children) {
            if (!isRenderable(child))
                continue;
            // Дополнения поиска принадлежат панели, а не полю: в выгрузке
            // Form.xml их у поля нет.
            if (child.type == FormItem::Type::Search)
                continue;
            if (insideTable && child.type == FormItem::Type::Field) {
                children.append(makeField(child, document, true, tableName));
                continue;
            }
            appendChildren({child}, document, &children);
        }
        json.insert(QStringLiteral("childItems"), children);
    }
    json.insert(QStringLiteral("events"), QJsonArray());
    return json;
}

QJsonObject makeGroup(const FormItem &item, const FormDocument &document)
{
    const QString tag = elementTag(item);
    QJsonObject properties;
    const QString title = item.titleText();
    if (!title.isEmpty())
        properties.insert(QStringLiteral("Title"), title);
    if (!item.tooltip.isEmpty())
        properties.insert(QStringLiteral("ToolTip"), item.tooltip);
    // Выравнивание элемента внутри группы: платформа пишет GroupHorizontalAlign
    // и GroupVerticalAlign только при отличии от Auto, поэтому значения 0..2
    // уходят в модель как Left/Center/Right и Top/Center/Bottom.
    if (const QString align = alignText(item.groupHorizontalAlign); !align.isEmpty())
        properties.insert(QStringLiteral("GroupHorizontalAlign"), align);
    if (const QString align = verticalAlignText(item.groupVerticalAlign);
        !align.isEmpty()) {
        properties.insert(QStringLiteral("GroupVerticalAlign"), align);
    }
    // Размеры группы и растяжение записаны в самой форме: ширина и высота — в
    // единицах формы, растяжение — кодами 0 (нет), 1 (да) и 2 (авто, значение
    // по умолчанию, в Form.xml не пишется). Сверено с Form.xml по всем 361
    // группе, странице, страницам, панели и всплывающей группе корпуса.
    if (item.widthUnits > 0)
        properties.insert(QStringLiteral("Width"), QString::number(item.widthUnits));
    if (item.heightUnits > 0)
        properties.insert(QStringLiteral("Height"), QString::number(item.heightUnits));
    if (item.horizontalStretch == 0)
        properties.insert(QStringLiteral("HorizontalStretch"), QStringLiteral("false"));
    else if (item.horizontalStretch == 1)
        properties.insert(QStringLiteral("HorizontalStretch"), QStringLiteral("true"));
    if (item.verticalStretch == 0)
        properties.insert(QStringLiteral("VerticalStretch"), QStringLiteral("false"));
    else if (item.verticalStretch == 1)
        properties.insert(QStringLiteral("VerticalStretch"), QStringLiteral("true"));

    if (tag == QLatin1String("Popup")) {
        // Отображение всплывающей группы (Text/Picture/PictureAndText) в
        // бинарной форме не читается: умолчание не подставляем, иначе
        // картинка заголовка заменялась бы текстом.
    } else if (tag == QLatin1String("Pages")) {
        // Представление страниц: 0 — None, 1 — TabsOnTop (по умолчанию),
        // 2 — TabsOnBottom. Код лежит в записи самой группы.
        properties.insert(QStringLiteral("PagesRepresentation"),
                          pagesRepresentationText(item.pagesRepresentation));
    } else if (tag == QLatin1String("Page")) {
        // Заголовок страницы уже в Title.
    } else if (tag == QLatin1String("CommandBar")) {
        // Своя командная панель формы: платформа хранит у неё только заголовок,
        // группировка и оформление у неё не настраиваются.
        if (!item.visible)
            properties.insert(QStringLiteral("Visible"), boolText(false));
    } else if (tag == QLatin1String("AutoCommandBar")) {
        // Автозаполнение командной панели: в Form.xml платформа его не пишет
        // (значение по умолчанию — истина), поэтому и в модели свойства нет:
        // рендерер без Autofill сам подставляет стандартные команды.
    } else if (tag == QLatin1String("ButtonGroup")) {
        // У группы кнопок платформа пишет Representation только при
        // «компактном» отображении, а в бинарной форме этот признак не
        // читается — значение по умолчанию не подставляем.
    } else if (tag == QLatin1String("ColumnGroup")) {
        // Группа колонок таблицы: группировка, закрепление и заголовок.
        // Группировка группы колонок: Horizontal (0) или InCell (2). Значение
        // по умолчанию (1) платформа в XML не пишет; рендерер без свойства
        // считает группу вертикальной, поэтому Horizontal пишем явно.
        if (item.columnGroupMode == 0)
            properties.insert(QStringLiteral("Group"), QStringLiteral("Horizontal"));
        else if (item.columnGroupMode == 2)
            properties.insert(QStringLiteral("Group"), QStringLiteral("InCell"));
        // Заголовок группы колонок платформа показывает только при ShowInHeader
        // (по умолчанию скрыт), закрепление — при отличии от None.
        if (item.showInHeader)
            properties.insert(QStringLiteral("ShowInHeader"), boolText(true));
        if (const QString fixing = fixingInTableText(item.fixingInTable); !fixing.isEmpty())
            properties.insert(QStringLiteral("FixingInTable"), fixing);
        if (!item.visible)
            properties.insert(QStringLiteral("Visible"), boolText(false));
    } else if (tag == QLatin1String("UsualGroup")) {
        // У обычной группы платформа показывает заголовок только если он
        // задан, а ориентацию по умолчанию считает горизонтальной — как и
        // рендерер, поэтому Group отдаём всегда, а ShowTitle только для
        // скрытого заголовка.
        if (const QString group = groupModeText(item.groupMode, item.orientation);
            !group.isEmpty()) {
            properties.insert(QStringLiteral("Group"), group);
        }
        if (const QString representation = representationText(item.representation);
            !representation.isEmpty()) {
            properties.insert(QStringLiteral("Representation"), representation);
        }
        if (item.hasBehavior)
            properties.insert(QStringLiteral("Behavior"), behaviorText(item.behavior));
        if (!item.showTitle)
            properties.insert(QStringLiteral("ShowTitle"), boolText(false));
        if (!item.visible)
            properties.insert(QStringLiteral("Visible"), boolText(false));
    }

    addStyleProperties(item, &properties);

    QJsonObject json;
    json.insert(QStringLiteral("tag"), tag);
    json.insert(QStringLiteral("name"), item.name);
    json.insert(QStringLiteral("id"), QString::number(item.id));
    json.insert(QStringLiteral("properties"), properties);
    json.insert(QStringLiteral("authoredProperties"), properties);
    QJsonArray children;
    appendChildren(item.children, document, &children);
    json.insert(QStringLiteral("childItems"), children);
    json.insert(QStringLiteral("events"), QJsonArray());
    return json;
}

// Группа колонок таблицы: заголовок над несколькими колонками. В таблице
// рендерера это узел ColumnGroup с дочерними InputField.
QJsonObject makeColumnGroup(const FormItem &item, const FormDocument &document,
                            const QString &tableName)
{
    QJsonObject properties;
    if (!item.synonym.isEmpty())
        properties.insert(QStringLiteral("Title"), item.synonym);
    if (!item.tooltip.isEmpty())
        properties.insert(QStringLiteral("ToolTip"), item.tooltip);
    if (item.widthUnits > 0)
        properties.insert(QStringLiteral("Width"), QString::number(item.widthUnits));
    // Группировка группы колонок: Horizontal (0) или InCell (2). Значение по
    // умолчанию (1) платформа в XML не пишет — без свойства рендерер считает
    // группу вертикальной, поэтому Horizontal пишем явно.
    if (item.columnGroupMode == 0)
        properties.insert(QStringLiteral("Group"), QStringLiteral("Horizontal"));
    else if (item.columnGroupMode == 2)
        properties.insert(QStringLiteral("Group"), QStringLiteral("InCell"));
    // Заголовок группы колонок платформа показывает только при ShowInHeader
    // (по умолчанию скрыт), закрепление — при отличии от None.
    if (item.showInHeader)
        properties.insert(QStringLiteral("ShowInHeader"), boolText(true));
    if (const QString fixing = fixingInTableText(item.fixingInTable); !fixing.isEmpty())
        properties.insert(QStringLiteral("FixingInTable"), fixing);
    // Растяжение группы колонок: Auto (2) — по умолчанию, в XML не пишется.
    if (item.horizontalStretch == 0)
        properties.insert(QStringLiteral("HorizontalStretch"), QStringLiteral("false"));
    else if (item.horizontalStretch == 1)
        properties.insert(QStringLiteral("HorizontalStretch"), QStringLiteral("true"));
    if (!item.visible)
        properties.insert(QStringLiteral("Visible"), QStringLiteral("false"));

    addStyleProperties(item, &properties);

    QJsonArray children;
    for (const FormItem &child : item.children) {
        if (!isRenderable(child))
            continue;
        if (child.type == FormItem::Type::Field)
            children.append(makeField(child, document, true, tableName));
        else if (child.type == FormItem::Type::Group
                 && child.groupKind == kGroupColumnGroup)
            children.append(makeColumnGroup(child, document, tableName));
    }

    QJsonObject json;
    json.insert(QStringLiteral("tag"), QStringLiteral("ColumnGroup"));
    json.insert(QStringLiteral("name"), item.name);
    json.insert(QStringLiteral("id"), QString::number(item.id));
    json.insert(QStringLiteral("properties"), properties);
    json.insert(QStringLiteral("authoredProperties"), properties);
    json.insert(QStringLiteral("childItems"), children);
    json.insert(QStringLiteral("events"), QJsonArray());
    return json;
}

// Имя главного реквизита формы: «Список» у формы списка, «Объект» у формы
// элемента. Таблица, привязанная к нему, — основная: своей командной панели
// у неё нет, команды живут в панели формы.
QString mainAttributeName(const FormDocument &document)
{
    for (const FormAttribute &attribute : document.attributes) {
        if (attribute.dynamicList)
            return attribute.name;
    }
    for (const FormAttribute &attribute : document.attributes) {
        if (attribute.name.compare(QStringLiteral("Список"), Qt::CaseInsensitive) == 0
            || attribute.name.compare(QStringLiteral("Объект"), Qt::CaseInsensitive) == 0
            || attribute.name.compare(QStringLiteral("Object"), Qt::CaseInsensitive) == 0) {
            return attribute.name;
        }
    }
    return QString();
}

QJsonObject makeTable(const FormItem &item, const FormDocument &document)
{
    QJsonObject properties;
    if (!item.dataPathText.isEmpty() && !item.dataPathText.contains(QLatin1Char('?')))
        properties.insert(QStringLiteral("DataPath"), item.dataPathText);
    properties.insert(QStringLiteral("Representation"), QStringLiteral("List"));
    // AutoInsertNewRow платформа пишет только для true; по умолчанию строка
    // не добавляется автоматически.
    if (item.autoInsertNewRow)
        properties.insert(QStringLiteral("AutoInsertNewRow"), QStringLiteral("true"));
    if (item.useAlternationRowColor)
        properties.insert(QStringLiteral("UseAlternationRowColor"), boolText(true));
    if (item.enableStartDrag)
        properties.insert(QStringLiteral("EnableStartDrag"), boolText(true));
    if (item.enableDrag)
        properties.insert(QStringLiteral("EnableDrag"), boolText(true));
    // Перетаскивание файлов в таблицу: платформа пишет только AsFile.
    if (item.fileDragMode == 0)
        properties.insert(QStringLiteral("FileDragMode"), QStringLiteral("AsFile"));
    if (const QString location = commandBarLocationText(item.commandBarLocation);
        !location.isEmpty()) {
        properties.insert(QStringLiteral("CommandBarLocation"), location);
    }
    // Положение заголовка таблицы: 0 — None (по умолчанию), 1 — Auto,
    // 2 — Left, 3 — Top, 4 — Right.
    if (item.tableTitleLocation > 0 && item.tableTitleLocation <= 5) {
        properties.insert(QStringLiteral("TitleLocation"),
                          titleLocationText(static_cast<FormTitleLocation>(
                              item.tableTitleLocation)));
    }
    // Заголовок таблицы — только собственный: подпись из метаданных рендерер
    // получает по пути к данным, как и для полей.
    if (!item.synonym.isEmpty())
        properties.insert(QStringLiteral("Title"), item.synonym);
    if (!item.tooltip.isEmpty())
        properties.insert(QStringLiteral("ToolTip"), item.tooltip);
    if (item.widthUnits > 0)
        properties.insert(QStringLiteral("Width"), QString::number(item.widthUnits));
    if (item.heightUnits > 0)
        properties.insert(QStringLiteral("Height"), QString::number(item.heightUnits));
    // Растяжение таблицы: платформа пишет его только у таблицы с заданными
    // шириной и высотой, значения — 0 (нет) и 1 (да); Auto (2) не пишется.
    if (item.horizontalStretch == 0)
        properties.insert(QStringLiteral("HorizontalStretch"), QStringLiteral("false"));
    else if (item.horizontalStretch == 1)
        properties.insert(QStringLiteral("HorizontalStretch"), QStringLiteral("true"));
    if (item.verticalStretch == 0)
        properties.insert(QStringLiteral("VerticalStretch"), QStringLiteral("false"));
    else if (item.verticalStretch == 1)
        properties.insert(QStringLiteral("VerticalStretch"), QStringLiteral("true"));
    if (!item.visible)
        properties.insert(QStringLiteral("Visible"), QStringLiteral("false"));

    // Заголовок таблицы и настройки динамического списка. В XML они пишутся
    // только у таблиц, связанных с динамическим списком; значения — как их
    // пишет платформа. Сверено с Form.xml по 87 таблицам динамических списков.
    if (!item.tableHeader)
        properties.insert(QStringLiteral("Header"), boolText(false));
    if (item.autoRefresh >= 0)
        properties.insert(QStringLiteral("AutoRefresh"), boolText(item.autoRefresh != 0));
    if (item.autoRefreshPeriod >= 0) {
        properties.insert(QStringLiteral("AutoRefreshPeriod"),
                          QString::number(item.autoRefreshPeriod));
    }
    if (!item.period.isEmpty())
        properties.insert(QStringLiteral("Period"), item.period);
    if (item.tableChoiceFolders >= 0 && item.tableChoiceFolders <= 2) {
        static const char *const names[] = {"Items", "Folders", "FoldersAndItems"};
        properties.insert(QStringLiteral("ChoiceFoldersAndItems"),
                          QString::fromLatin1(names[item.tableChoiceFolders]));
    }
    if (item.restoreCurrentRow >= 0) {
        properties.insert(QStringLiteral("RestoreCurrentRow"),
                          boolText(item.restoreCurrentRow != 0));
    }
    if (item.showRoot >= 0)
        properties.insert(QStringLiteral("ShowRoot"), boolText(item.showRoot != 0));
    if (item.allowRootChoice >= 0) {
        properties.insert(QStringLiteral("AllowRootChoice"),
                          boolText(item.allowRootChoice != 0));
    }
    if (item.hasRowFilter)
        properties.insert(QStringLiteral("RowFilter"), QString());
    if (item.updateOnDataChange == 0)
        properties.insert(QStringLiteral("UpdateOnDataChange"), QStringLiteral("Auto"));
    if (item.hasTopLevelParent)
        properties.insert(QStringLiteral("TopLevelParent"), item.topLevelParent);

    QJsonArray columns;
    const FormItem *tableBar = nullptr;
    // Имя табличной части — последний сегмент пути к данным таблицы.
    QString tableName;
    {
        const QStringList segments = item.dataPathText.split(QLatin1Char('.'));
        if (!segments.isEmpty())
            tableName = segments.last();
    }
    const bool mainList = !tableName.isEmpty()
        && tableName.compare(mainAttributeName(document), Qt::CaseInsensitive) == 0;
    for (const FormItem &child : item.children) {
        if (!isRenderable(child))
            continue;
        if (child.type == FormItem::Type::Group) {
            if (!tableBar && child.groupKind == kGroupCommandBar) {
                tableBar = &child;
                continue;
            }
            if (child.groupKind == kGroupColumnGroup) {
                columns.append(makeColumnGroup(child, document, tableName));
            }
            continue;
        }
        if (child.type == FormItem::Type::AutoCommandBar)
            continue;
        // Дополнения поиска самой таблицы платформа не описывает в форме: они
        // появляются, только если вынесены в панель (тогда их и разбираем как
        // элементы панели). В таблице они — служебные: в выгрузке Form.xml их
        // нет, поэтому и модель их не получает.
        if (child.type == FormItem::Type::Search)
            continue;
        if (child.type == FormItem::Type::Field) {
            columns.append(makeField(child, document, true, tableName));
            continue;
        }
        appendChildren({child}, document, &columns);
    }

    // Свою командную панель таблица показывает только если она описана в форме.
    // Иначе платформа кладёт команды таблицы в общую панель формы, и отдельная
    // панель вроде «Создать» у списка была бы лишней. У основной таблицы списка
    // («Список») своей панели нет — команды живут в панели формы.
    if (mainList)
        properties.insert(QStringLiteral("CommandBarLocation"), QStringLiteral("None"));

    addStyleProperties(item, &properties);

    QJsonObject json;
    json.insert(QStringLiteral("tag"), QStringLiteral("Table"));
    json.insert(QStringLiteral("name"), item.name);
    json.insert(QStringLiteral("id"), QString::number(item.id));
    json.insert(QStringLiteral("properties"), properties);
    json.insert(QStringLiteral("authoredProperties"), properties);
    json.insert(QStringLiteral("childItems"), columns);
    json.insert(QStringLiteral("events"), QJsonArray());
    if (tableBar && !mainList) {
        json.insert(QStringLiteral("autoCommandBar"),
                    buildCommandBarNode(tableBar, document, QJsonArray(), true,
                                        item.name + QStringLiteral("КоманднаяПанель")));
    }
    return json;
}

QJsonObject makeItem(const FormItem &item, const FormDocument &document)
{
    switch (item.type) {
    case FormItem::Type::Button:
        return makeButton(item, document);
    case FormItem::Type::Field:
        return makeField(item, document, false);
    case FormItem::Type::Table:
        return makeTable(item, document);
    case FormItem::Type::Group:
        return makeGroup(item, document);
    // Дополнение панели (строка поиска, состояние просмотра, управление
    // поиском) — самостоятельный элемент панели, платформа подставляет его
    // данные из таблицы, на которую оно ссылается (AdditionSource).
    case FormItem::Type::Search: {
        QJsonObject properties;
        const QString title = item.titleText();
        if (!title.isEmpty())
            properties.insert(QStringLiteral("Title"), title);
        const QString table = mainAttributeName(document);
        if (!table.isEmpty()) {
            const QString source = elementTag(item) == QLatin1String("ViewStatusAddition")
                ? QStringLiteral("ViewStatus")
                : (elementTag(item) == QLatin1String("SearchControlAddition")
                       ? QStringLiteral("SearchControl")
                       : QStringLiteral("SearchStringRepresentation"));
            properties.insert(QStringLiteral("AdditionSource"),
                              QStringLiteral("%1, %2").arg(table, source));
        }
        if (!item.visible)
            properties.insert(QStringLiteral("Visible"), boolText(false));
        QJsonObject json;
        json.insert(QStringLiteral("tag"), elementTag(item));
        json.insert(QStringLiteral("name"), item.name);
        json.insert(QStringLiteral("id"), QString::number(item.id));
        json.insert(QStringLiteral("properties"), properties);
        json.insert(QStringLiteral("authoredProperties"), properties);
        json.insert(QStringLiteral("childItems"), QJsonArray());
        json.insert(QStringLiteral("events"), QJsonArray());
        return json;
    }
    case FormItem::Type::Decoration: {
        QJsonObject properties;
        const QString text = item.titleText();
        if (!text.isEmpty())
            properties.insert(QStringLiteral("Title"), text);
        // Признак форматированного заголовка: платформа пишет его только при
        // явно заданном значении, а рендерер по нему решает, разбирать ли
        // разметку заголовка (шрифт, цвет, ссылки).
        if (item.hasTitleFormatted)
            properties.insert(QStringLiteral("TitleFormatted"), boolText(item.titleFormatted));
        if (!item.tooltip.isEmpty())
            properties.insert(QStringLiteral("ToolTip"), item.tooltip);
        if (!item.visible)
            properties.insert(QStringLiteral("Visible"), boolText(false));
        if (item.hyperlink)
            properties.insert(QStringLiteral("Hyperlink"), boolText(true));
        // Выравнивание декорации внутри группы: платформа пишет
        // GroupHorizontalAlign и GroupVerticalAlign только при отличии от Auto.
        if (const QString align = alignText(item.groupHorizontalAlign); !align.isEmpty())
            properties.insert(QStringLiteral("GroupHorizontalAlign"), align);
        if (const QString align = verticalAlignText(item.groupVerticalAlign);
            !align.isEmpty()) {
            properties.insert(QStringLiteral("GroupVerticalAlign"), align);
        }
        if (item.widthUnits > 0)
            properties.insert(QStringLiteral("Width"), QString::number(item.widthUnits));
        if (item.heightUnits > 0)
            properties.insert(QStringLiteral("Height"), QString::number(item.heightUnits));
        // Растяжение декорации: платформа пишет только false и true, Auto (2) —
        // значение по умолчанию. Сверено с Form.xml по 73 надписям и 39
        // картинкам корпуса.
        if (item.horizontalStretch == 0)
            properties.insert(QStringLiteral("HorizontalStretch"), QStringLiteral("false"));
        else if (item.horizontalStretch == 1)
            properties.insert(QStringLiteral("HorizontalStretch"), QStringLiteral("true"));
        if (item.verticalStretch == 0)
            properties.insert(QStringLiteral("VerticalStretch"), QStringLiteral("false"));
        else if (item.verticalStretch == 1)
            properties.insert(QStringLiteral("VerticalStretch"), QStringLiteral("true"));
        // AutoMaxWidth надписи: платформа пишет только false (по умолчанию true).
        if (!item.pictureDecoration && !item.autoMaxWidth)
            properties.insert(QStringLiteral("AutoMaxWidth"), boolText(false));
        // Перетаскивание картинки файлом: 0 — AsFile (в XML пишется только оно).
        if (item.fileDragMode == 0)
            properties.insert(QStringLiteral("FileDragMode"), QStringLiteral("AsFile"));
        // Картинка декорации: имя общей картинки конфигурации
        // («CommonPicture.Имя») или стандартной картинки платформы
        // («StdPicture.Имя»); сами данные общих картинок уходят в узел
        // commonPictures модели.
        if (isPictureDecoration(item) && !item.pictureRef.isEmpty())
            properties.insert(QStringLiteral("Picture"), item.pictureRef);
        addStyleProperties(item, &properties);
        QJsonObject json;
        json.insert(QStringLiteral("tag"),
                    isPictureDecoration(item) ? QStringLiteral("PictureDecoration")
                                              : QStringLiteral("LabelDecoration"));
        json.insert(QStringLiteral("name"), item.name);
        json.insert(QStringLiteral("id"), QString::number(item.id));
        json.insert(QStringLiteral("properties"), properties);
        json.insert(QStringLiteral("authoredProperties"), properties);
        json.insert(QStringLiteral("childItems"), QJsonArray());
        json.insert(QStringLiteral("events"), QJsonArray());
        return json;
    }
    default:
        break;
    }
    return QJsonObject();
}

QJsonObject makeStandardButton(const QString &name, const QString &title,
                               const QString &commandName, bool defaultButton = false)
{
    QJsonObject properties;
    properties.insert(QStringLiteral("Type"), QStringLiteral("CommandBarButton"));
    properties.insert(QStringLiteral("Title"), title);
    properties.insert(QStringLiteral("Picture"), QString());
    properties.insert(QStringLiteral("Representation"), QStringLiteral("Text"));
    properties.insert(QStringLiteral("CommandName"), commandName);
    if (defaultButton)
        properties.insert(QStringLiteral("DefaultButton"), QStringLiteral("true"));
    QJsonObject json;
    json.insert(QStringLiteral("tag"), QStringLiteral("Button"));
    json.insert(QStringLiteral("name"), name);
    json.insert(QStringLiteral("id"), QString());
    json.insert(QStringLiteral("properties"), properties);
    json.insert(QStringLiteral("authoredProperties"), properties);
    json.insert(QStringLiteral("childItems"), QJsonArray());
    json.insert(QStringLiteral("events"), QJsonArray());
    return json;
}

QJsonObject makePopup(const QString &name, const QString &title,
                      const QJsonArray &children = QJsonArray())
{
    QJsonObject properties;
    properties.insert(QStringLiteral("Title"), title);
    properties.insert(QStringLiteral("Representation"), QStringLiteral("Text"));
    QJsonObject json;
    json.insert(QStringLiteral("tag"), QStringLiteral("Popup"));
    json.insert(QStringLiteral("name"), name);
    json.insert(QStringLiteral("id"), QString());
    json.insert(QStringLiteral("properties"), properties);
    json.insert(QStringLiteral("authoredProperties"), properties);
    json.insert(QStringLiteral("childItems"), children);
    json.insert(QStringLiteral("events"), QJsonArray());
    return json;
}

bool hasStandardButton(const QVector<FormItem> &items, const QString &title)
{
    for (const FormItem &item : items) {
        if (item.type == FormItem::Type::Button && item.titleText() == title)
            return true;
        if (hasStandardButton(item.children, title))
            return true;
    }
    return false;
}

// Узел командной панели: собственные кнопки формы плюс (для корневой панели)
// стандартные команды платформы, которые 1С добавляет сама.
QJsonObject buildCommandBarNode(const FormItem *bar, const FormDocument &document,
                                const QJsonArray &leading, bool autofill,
                                const QString &defaultName)
{
    QJsonArray buttons = leading;
    if (bar) {
        for (const FormItem &child : bar->children) {
            if (!isRenderable(child) || child.type == FormItem::Type::AutoCommandBar)
                continue;
            buttons.append(makeItem(child, document));
        }
    }
    QJsonObject properties;
    properties.insert(QStringLiteral("Autofill"), boolText(autofill));
    QJsonObject json;
    json.insert(QStringLiteral("tag"), QStringLiteral("AutoCommandBar"));
    json.insert(QStringLiteral("name"),
                bar && !bar->name.isEmpty() ? bar->name : defaultName);
    json.insert(QStringLiteral("id"), QString());
    json.insert(QStringLiteral("properties"), properties);
    json.insert(QStringLiteral("authoredProperties"), properties);
    json.insert(QStringLiteral("childItems"), buttons);
    json.insert(QStringLiteral("events"), QJsonArray());
    return json;
}

// Группа важных команд в панели формы: платформа ставит «Создать на
// основании» не вплотную к «Провести», а после этой группы, поэтому
// «Движения документа» оказывается левее подменю создания.
bool isImportantCommandsGroup(const QJsonObject &group)
{
    if (group.value(QStringLiteral("tag")).toString() != QLatin1String("ButtonGroup"))
        return false;
    const QString title = group.value(QStringLiteral("properties")).toObject()
                              .value(QStringLiteral("Title")).toString();
    const QString name = group.value(QStringLiteral("name")).toString();
    return title.contains(QStringLiteral("Важн"), Qt::CaseInsensitive)
        || name.contains(QStringLiteral("Важн"), Qt::CaseInsensitive)
        || name.contains(QStringLiteral("Important"), Qt::CaseInsensitive);
}

// Вставляет подменю создания после группы важных команд, где бы внутри
// панели она ни лежала (обычно — внутри «Кнопки командной панели»).
bool insertAfterImportantGroup(QJsonArray *groups, const QJsonObject &basedOn)
{
    for (int i = 0; i < groups->size(); ++i) {
        const QJsonObject group = groups->at(i).toObject();
        if (isImportantCommandsGroup(group)) {
            groups->insert(i + 1, basedOn);
            return true;
        }
    }
    for (int i = 0; i < groups->size(); ++i) {
        QJsonObject group = groups->at(i).toObject();
        QJsonArray children = group.value(QStringLiteral("childItems")).toArray();
        if (children.isEmpty())
            continue;
        if (!insertAfterImportantGroup(&children, basedOn))
            continue;
        group.insert(QStringLiteral("childItems"), children);
        (*groups)[i] = group;
        return true;
    }
    return false;
}

void moveCreateBasedOnAfterImportantCommands(QJsonObject *bar)
{
    QJsonArray buttons = bar->value(QStringLiteral("childItems")).toArray();
    QJsonObject basedOn;
    int basedOnIndex = -1;
    for (int i = 0; i < buttons.size(); ++i) {
        const QJsonObject button = buttons.at(i).toObject();
        if (button.value(QStringLiteral("name")).toString()
            == QLatin1String("_global_create_based_on")) {
            basedOn = button;
            basedOnIndex = i;
            break;
        }
    }
    if (basedOnIndex < 0)
        return;
    QJsonArray rest;
    for (int i = 0; i < buttons.size(); ++i) {
        if (i != basedOnIndex)
            rest.append(buttons.at(i));
    }
    if (!insertAfterImportantGroup(&rest, basedOn))
        return;
    bar->insert(QStringLiteral("childItems"), rest);
}

QJsonObject buildAutoCommandBar(const FormDocument &document,
                                const FormItem *autoBar, bool authoredBarPresent)
{
    QJsonArray standard;
    // Если в форме есть своя командная панель (группа CommandBar), платформа
    // показывает именно её: кнопки записи и проведения заданы в ней, а панель
    // автозаполнения остаётся пустой.
    if (authoredBarPresent) {
        QJsonObject bar = buildCommandBarNode(autoBar, document, standard, false,
                                              QStringLiteral("ФормаКоманднаяПанель"));
        return bar;
    }
    if (document.runtimeDocumentCommands) {
        // Порядок кнопок как в клиенте 1С: «Провести и закрыть» (по умолчанию),
        // «Записать», «Провести».
        standard.append(makeStandardButton(
            QStringLiteral("_stdPostAndClose"),
            QStringLiteral("Провести и закрыть"),
            QStringLiteral("Form.StandardCommand.PostAndClose"), true));
        if (!hasStandardButton(document.items, QStringLiteral("Записать"))) {
            standard.append(makeStandardButton(
                QStringLiteral("_stdWrite"), QStringLiteral("Записать"),
                QStringLiteral("Form.StandardCommand.Write")));
        }
        standard.append(makeStandardButton(
            QStringLiteral("_stdPost"), QStringLiteral("Провести"),
            QStringLiteral("Form.StandardCommand.Post")));
    } else if (document.runtimeCatalogCommands) {
        // Форма элемента справочника: панель начинается с «Записать».
        standard.append(makeStandardButton(
            QStringLiteral("_stdWrite"), QStringLiteral("Записать"),
            QStringLiteral("Form.StandardCommand.Write")));
    }
    // «Создать на основании» платформа добавляет только документам: в формах
    // справочников подменю создания на основании нет, а у отчётов и обработок
    // нет и создания.
    const QString kind = kindTagFor(document.ownerTitle);
    if (kind == QLatin1String("Document")) {
        standard.append(makePopup(QStringLiteral("_global_create_based_on"),
                                  QStringLiteral("Создать на основании"), QJsonArray()));
    }
    QJsonObject bar = buildCommandBarNode(autoBar, document, standard, false,
                                          QStringLiteral("ФормаКоманднаяПанель"));
    moveCreateBasedOnAfterImportantCommands(&bar);
    return bar;
}

QString kindTagFor(const QString &ownerTitle)
{
    const QString tag = ownerKindTag(ownerTitle);
    if (!tag.isEmpty())
        return tag;
    return QStringLiteral("Document");
}

QString kindLower(const QString &kindTag)
{
    QString kind = kindTag;
    if (kind == QLatin1String("Document"))
        return QStringLiteral("document");
    if (kind == QLatin1String("Catalog"))
        return QStringLiteral("catalog");
    if (kind == QLatin1String("DataProcessor"))
        return QStringLiteral("dataprocessor");
    if (kind == QLatin1String("Report"))
        return QStringLiteral("report");
    if (kind == QLatin1String("ChartOfCharacteristicTypes"))
        return QStringLiteral("characteristicstypes");
    if (kind == QLatin1String("ChartOfAccounts"))
        return QStringLiteral("chartofaccounts");
    if (kind == QLatin1String("ExchangePlan"))
        return QStringLiteral("exchangeplan");
    if (kind == QLatin1String("Task"))
        return QStringLiteral("task");
    if (kind == QLatin1String("BusinessProcess"))
        return QStringLiteral("businessprocess");
    if (kind == QLatin1String("InformationRegister"))
        return QStringLiteral("informationregister");
    if (kind == QLatin1String("AccumulationRegister"))
        return QStringLiteral("accumulationregister");
    return QStringLiteral("document");
}

QJsonObject standardCaptions(const QString &kindTag)
{
    QJsonObject captions;
    captions.insert(QStringLiteral("Ссылка"), QStringLiteral("Ссылка"));
    captions.insert(QStringLiteral("Ref"), QStringLiteral("Ссылка"));
    captions.insert(QStringLiteral("ПометкаУдаления"), QStringLiteral("Пометка удаления"));
    captions.insert(QStringLiteral("DeletionMark"), QStringLiteral("Пометка удаления"));
    captions.insert(QStringLiteral("ВерсияДанных"), QStringLiteral("Версия данных"));
    captions.insert(QStringLiteral("DataVersion"), QStringLiteral("Версия данных"));
    if (kindTag == QLatin1String("Document")) {
        captions.insert(QStringLiteral("Номер"), QStringLiteral("Номер"));
        captions.insert(QStringLiteral("Number"), QStringLiteral("Номер"));
        captions.insert(QStringLiteral("Дата"), QStringLiteral("Дата"));
        captions.insert(QStringLiteral("Date"), QStringLiteral("Дата"));
        captions.insert(QStringLiteral("Проведен"), QStringLiteral("Проведен"));
        captions.insert(QStringLiteral("Posted"), QStringLiteral("Проведен"));
    } else if (kindTag == QLatin1String("Catalog")
               || kindTag == QLatin1String("ChartOfCharacteristicTypes")) {
        captions.insert(QStringLiteral("Код"), QStringLiteral("Код"));
        captions.insert(QStringLiteral("Code"), QStringLiteral("Код"));
        captions.insert(QStringLiteral("Наименование"), QStringLiteral("Наименование"));
        captions.insert(QStringLiteral("Description"), QStringLiteral("Наименование"));
        captions.insert(QStringLiteral("ПометкаУдаления"), QStringLiteral("Пометка удаления"));
        captions.insert(QStringLiteral("ЭтоГруппа"), QStringLiteral("Это группа"));
        captions.insert(QStringLiteral("IsFolder"), QStringLiteral("Это группа"));
        captions.insert(QStringLiteral("Родитель"), QStringLiteral("Родитель"));
        captions.insert(QStringLiteral("Parent"), QStringLiteral("Родитель"));
    }
    return captions;
}

QString ownerName(const FormDocument &document)
{
    const int dot = document.ownerTitle.indexOf(QLatin1Char('.'));
    if (dot >= 0 && dot + 1 < document.ownerTitle.size())
        return document.ownerTitle.mid(dot + 1);
    return document.ownerTitle;
}

QJsonObject buildObjectMeta(const FormDocument &document)
{
    const FormOwnerMeta &owner = document.ownerMeta;
    const QString kindTag = owner.kindTag.isEmpty() ? kindTagFor(document.ownerTitle)
                                                    : owner.kindTag;
    const QString name = owner.name.isEmpty() ? ownerName(document) : owner.name;
    QJsonObject captions;
    for (auto it = document.titles.constBegin(); it != document.titles.constEnd(); ++it)
        captions.insert(it.key(), it.value());
    for (auto it = owner.captions.constBegin(); it != owner.captions.constEnd(); ++it)
        captions.insert(it.key(), it.value());

    auto objectOf = [](const QHash<QString, QString> &source) {
        QJsonObject result;
        for (auto it = source.constBegin(); it != source.constEnd(); ++it)
            result.insert(it.key(), it.value());
        return result;
    };
    auto numberQualifiers = [&owner]() {
        QJsonObject result;
        for (auto it = owner.numberQ.constBegin(); it != owner.numberQ.constEnd(); ++it) {
            const QStringList parts = it.value().split(QLatin1Char(':'));
            if (parts.size() != 2)
                continue;
            QJsonObject qualifier;
            qualifier.insert(QStringLiteral("digits"), parts.at(0).toInt());
            qualifier.insert(QStringLiteral("fractionDigits"), parts.at(1).toInt());
            qualifier.insert(QStringLiteral("allowedSign"),
                             QStringLiteral("Nonnegative"));
            result.insert(it.key(), qualifier);
        }
        return result;
    };
    auto stringLengths = [&owner]() {
        QJsonObject result;
        for (auto it = owner.stringLen.constBegin(); it != owner.stringLen.constEnd(); ++it)
            result.insert(it.key(), it.value().toInt());
        return result;
    };

    QJsonObject meta;
    meta.insert(QStringLiteral("definedType"), QJsonValue::Null);
    meta.insert(QStringLiteral("hasFolders"), false);
    meta.insert(QStringLiteral("hierarchical"), false);
    meta.insert(QStringLiteral("kindTag"), kindTag);
    meta.insert(QStringLiteral("stdCaptions"), standardCaptions(kindTag));
    meta.insert(QStringLiteral("name"), name);
    meta.insert(QStringLiteral("synonym"),
                owner.synonym.isEmpty() ? document.ownerSynonym : owner.synonym);
    meta.insert(QStringLiteral("listPresentation"),
                owner.listPresentation.isEmpty() ? document.ownerSynonym
                                                 : owner.listPresentation);
    meta.insert(QStringLiteral("captions"), captions);
    meta.insert(QStringLiteral("stringLen"), stringLengths());
    meta.insert(QStringLiteral("types"), objectOf(owner.types));
    meta.insert(QStringLiteral("fillChecking"), QJsonObject());
    meta.insert(QStringLiteral("numberQ"), numberQualifiers());
    meta.insert(QStringLiteral("dateFraction"), objectOf(owner.dateFraction));
    meta.insert(QStringLiteral("multiLine"), QJsonObject());
    meta.insert(QStringLiteral("kind"), kindLower(kindTag));
    meta.insert(QStringLiteral("metaClass"), kindTag);
    meta.insert(QStringLiteral("objectBelonging"), QString());
    meta.insert(QStringLiteral("autonumbering"), QString());
    meta.insert(QStringLiteral("useStandardCommands"), true);
    meta.insert(QStringLiteral("hasHelp"), false);
    meta.insert(QStringLiteral("descriptionLength"), 0);
    meta.insert(QStringLiteral("codeLength"), 0);
    meta.insert(QStringLiteral("numberLength"), 0);
    meta.insert(QStringLiteral("codeType"), QString());
    return meta;
}

QJsonObject buildAttribute(const FormAttribute &attribute, const QString &typeHint,
                           bool mainAttribute = false,
                           const QString &mainTable = QString())
{
    QJsonObject properties;
    QString type = typeHint;
    if (!typeHint.startsWith(QLatin1String("cfg:"))) {
        const QString mapped = rendererTypeFor(typeHint);
        if (!mapped.isEmpty())
            type = mapped;
    }
    properties.insert(QStringLiteral("Type"), type);
    if (mainAttribute)
        properties.insert(QStringLiteral("MainAttribute"), QStringLiteral("true"));
    if (!attribute.synonym.isEmpty())
        properties.insert(QStringLiteral("Title"), attribute.synonym);

    QJsonObject json;
    json.insert(QStringLiteral("name"), attribute.name);
    json.insert(QStringLiteral("id"), QString());
    json.insert(QStringLiteral("properties"), properties);
    json.insert(QStringLiteral("typeDescription"), QJsonValue::Null);
    json.insert(QStringLiteral("typeRefs"), type);
    json.insert(QStringLiteral("columns"), QJsonArray());
    json.insert(QStringLiteral("stringLen"), 0);
    json.insert(QStringLiteral("numberQ"), QJsonValue::Null);
    json.insert(QStringLiteral("dateFraction"), QString());
    // Настройки динамического списка: рендерер выводит из них заголовки
    // колонок для полей запроса, которых нет в метаданных объекта.
    if (attribute.dynamicList) {
        QJsonObject settings;
        settings.insert(QStringLiteral("type"), QStringLiteral("DynamicList"));
        settings.insert(QStringLiteral("mainTable"), mainTable);
        settings.insert(QStringLiteral("manualQuery"), attribute.listManualQuery);
        settings.insert(QStringLiteral("queryText"), attribute.listQuery);
        settings.insert(QStringLiteral("fieldTitles"), QJsonObject());
        json.insert(QStringLiteral("settings"), settings);
    }
    return json;
}

QJsonObject buildCommand(const FormCommand &command)
{
    QJsonObject properties;
    properties.insert(QStringLiteral("Title"),
                      command.synonym.isEmpty() ? command.name : command.synonym);
    properties.insert(QStringLiteral("Action"), command.name);

    QJsonObject json;
    json.insert(QStringLiteral("name"), command.name);
    json.insert(QStringLiteral("id"), QString());
    json.insert(QStringLiteral("properties"), properties);
    json.insert(QStringLiteral("typeDescription"), QJsonValue::Null);
    json.insert(QStringLiteral("typeRefs"), QString());
    json.insert(QStringLiteral("columns"), QJsonArray());
    json.insert(QStringLiteral("stringLen"), 0);
    json.insert(QStringLiteral("numberQ"), QJsonValue::Null);
    json.insert(QStringLiteral("dateFraction"), QString());
    return json;
}

} // namespace

QByteArray formWebModelJson(const FormDocument &document)
{
    QJsonObject model;
    model.insert(QStringLiteral("tag"), QStringLiteral("Form"));
    model.insert(QStringLiteral("name"), document.name);

    QString caption = document.ownerSynonym;
    if (caption.isEmpty())
        caption = document.synonym;
    if (caption.isEmpty())
        caption = document.name;

    QJsonObject properties;
    properties.insert(QStringLiteral("Title"), caption);
    properties.insert(QStringLiteral("AutoTitle"), QStringLiteral("false"));
    if (document.runtimeDocumentCommands) {
        properties.insert(QStringLiteral("UsePostingMode"), QStringLiteral("Auto"));
        properties.insert(QStringLiteral("RepostOnWrite"), QStringLiteral("true"));
    }
    model.insert(QStringLiteral("properties"), properties);

    // Корневые элементы формы: панель, которую заполняет платформа
    // (AutoCommandBar, код группы 9), отдаём отдельным узлом autoCommandBar —
    // так её ожидает рендерер. Собственная панель формы (CommandBar, код 0)
    // остаётся обычным элементом дерева: платформа тоже хранит её отдельно от
    // авто-панели, и в форме списка кнопки живут именно в ней.
    const FormItem *autoBar = nullptr;
    for (const FormItem &item : document.items) {
        if (item.type == FormItem::Type::Group
            && item.groupKind == kGroupAutoCommandBar) {
            autoBar = &item;
            break;
        }
    }
    // Своя командная панель может стоять не только в корне формы, но и внутри
    // группы («ГруппаВерхняяПанельНовости» в справочнике «Новости»): тогда
    // платформа показывает именно её, а панель автозаполнения остаётся пустой.
    std::function<bool(const QVector<FormItem> &)> hasAuthoredBar =
        [&hasAuthoredBar](const QVector<FormItem> &items) {
            for (const FormItem &item : items) {
                if (item.type == FormItem::Type::Group
                    && item.groupKind == kGroupCommandBar)
                    return true;
                if (hasAuthoredBar(item.children))
                    return true;
            }
            return false;
        };
    const bool authoredBarPresent = hasAuthoredBar(document.items);
    QJsonArray rootItems;
    for (const FormItem &item : document.items) {
        if (!isRenderable(item) || item.type == FormItem::Type::AutoCommandBar)
            continue;
        rootItems.append(makeItem(item, document));
    }
    linkFormattedDocumentBars(&rootItems);
    model.insert(QStringLiteral("childItemsRoot"), rootItems);
    model.insert(QStringLiteral("autoCommandBar"),
                 buildAutoCommandBar(document, autoBar, authoredBarPresent));

    QJsonArray attributes;
    // Основная таблица динамического списка — для настроек реквизита.
    const QString mainTable = document.ownerMeta.valid
        ? document.ownerMeta.kindTag + QLatin1Char('.') + document.ownerMeta.name
        : QString();
    const QString objectType = QStringLiteral("cfg:%1Object.%2")
        .arg(kindTagFor(document.ownerTitle), ownerName(document));
    auto objectAttribute = [&objectType](const QString &synonym) {
        FormAttribute attribute;
        attribute.name = QStringLiteral("Объект");
        attribute.synonym = synonym;
        attribute.typeHint = objectType;
        return attribute;
    };
    // Главный реквизит формы («Объект»/«Список») сама форма уже объявляет:
    // дублировать его нельзя — от порядка реквизитов зависят пути к данным.
    bool mainAttributeFound = false;
    for (const FormAttribute &attribute : document.attributes) {
        const bool main = attribute.name.compare(QStringLiteral("Объект"),
                                                 Qt::CaseInsensitive) == 0
            || attribute.name.compare(QStringLiteral("Object"), Qt::CaseInsensitive) == 0;
        if (main && !mainAttributeFound) {
            mainAttributeFound = true;
            attributes.append(buildAttribute(objectAttribute(attribute.synonym),
                                             objectType, true));
            continue;
        }
        // Реквизит динамического списка тоже главный: по нему рендерер
        // подставляет заголовки стандартных полей списка («Ссылка», «Код»).
        attributes.append(buildAttribute(attribute, attribute.typeHint,
                                         attribute.dynamicList, mainTable));
    }
    if (!mainAttributeFound) {
        attributes.prepend(buildAttribute(objectAttribute(QString()), objectType, true));
    }
    model.insert(QStringLiteral("attributes"), attributes);
    model.insert(QStringLiteral("baseAttributes"), QJsonArray());

    QJsonArray commands;
    for (const FormCommand &command : document.commands)
        commands.append(buildCommand(command));
    model.insert(QStringLiteral("commands"), commands);

    model.insert(QStringLiteral("excludedCommands"), QJsonArray());
    model.insert(QStringLiteral("commandInterface"), QJsonArray());
    model.insert(QStringLiteral("commonCommands"), QJsonObject());
    // Общие картинки, на которые ссылается форма: имя -> данные ZIP-архива
    // с вариантами PNG (рендерер разбирает архив сам).
    QJsonObject commonPictures;
    for (auto it = document.commonPictures.constBegin();
         it != document.commonPictures.constEnd(); ++it) {
        QJsonObject entry;
        QString mime = document.commonPictureMimes.value(it.key());
        if (mime.isEmpty())
            mime = QStringLiteral("application/zip");
        entry.insert(QStringLiteral("mime"), mime);
        entry.insert(QStringLiteral("data"), it.value());
        commonPictures.insert(it.key(), entry);
    }
    model.insert(QStringLiteral("commonPictures"), commonPictures);
    model.insert(QStringLiteral("commandGroups"), QJsonObject());
    model.insert(QStringLiteral("version"), QStringLiteral("2.20"));
    model.insert(QStringLiteral("adoptedForm"), false);
    model.insert(QStringLiteral("interfaceMode"), QStringLiteral("Any"));
    model.insert(QStringLiteral("configInterfaceMode"),
                 QStringLiteral("TaxiEnableVersion8_2"));
    model.insert(QStringLiteral("objectMeta"), buildObjectMeta(document));
    model.insert(QStringLiteral("refMeta"), QJsonObject());
    // Таблица стилей-цветов формы: имя стиля -> абсолютный цвет. Рендерер
    // сам разбирает ссылки «style:Имя» у элементов, но цвет берёт отсюда.
    QJsonObject styleItems;
    for (auto it = document.styleItems.constBegin();
         it != document.styleItems.constEnd(); ++it) {
        styleItems.insert(it.key(), it.value());
    }
    model.insert(QStringLiteral("styleItems"), styleItems);
    model.insert(QStringLiteral("events"), QJsonArray());

    return QJsonDocument(model).toJson(QJsonDocument::Compact);
}

} // namespace v8reader
