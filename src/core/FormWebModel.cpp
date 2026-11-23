#include "core/FormWebModel.h"

#include "core/FormDocument.h"
#include "core/FormOwner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

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

// Коды вида группы (второй код в узле группы).
enum : int {
    kGroupPopup = 1,
    kGroupColumnGroup = 2,
    kGroupPages = 3,
    kGroupPage = 4,
    kGroupUsual = 5,
    kGroupButtons = 6,
    kGroupContextMenu = 8,
    kGroupCommandBar = 9
};

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString groupText(int orientation)
{
    return orientation == 1 ? QStringLiteral("Horizontal")
                            : QStringLiteral("Vertical");
}

QString representationText(FormGroupRepresentation representation)
{
    switch (representation) {
    case FormGroupRepresentation::Strong:
        return QStringLiteral("Strong");
    case FormGroupRepresentation::Weak:
        return QStringLiteral("Weak");
    case FormGroupRepresentation::Normal:
        return QStringLiteral("Normal");
    case FormGroupRepresentation::None:
        break;
    }
    return QStringLiteral("None");
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
    return item.name.startsWith(QStringLiteral("Картинка"))
        || item.name.startsWith(QStringLiteral("Picture"));
}

// Элемент, который имеет смысл отдавать рендереру.
bool isRenderable(const FormItem &item)
{
    switch (item.type) {
    case FormItem::Type::Group:
        return item.groupKind != kGroupContextMenu;
    case FormItem::Type::Field:
    case FormItem::Type::Button:
    case FormItem::Type::Table:
        return true;
    case FormItem::Type::Decoration:
        return !isServiceTooltip(item) && !isPictureDecoration(item);
    case FormItem::Type::Search:
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
        case kGroupUsual:
            return QStringLiteral("UsualGroup");
        default:
            break;
        }
        return insideTable ? QStringLiteral("ColumnGroup") : QStringLiteral("UsualGroup");
    case FormItem::Type::Field:
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
        return QStringLiteral("LabelDecoration");
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
}

// Человекочитаемый заголовок из имени элемента: «ПоказатьДвиженияДокумента» →
// «Показать движения документа». Нужен для кнопок общих команд, у которых в
// контейнере нет готового заголовка (он берётся из описания общей команды).
QString humanizeName(const QString &name)
{
    static const QStringList prefixes = {
        QStringLiteral("ФормаОбщаяКоманда"),
        QStringLiteral("ОбщаяКоманда"),
        QStringLiteral("ФормаКоманда"),
        QStringLiteral("Форма")
    };
    QString rest = name;
    for (const QString &prefix : prefixes) {
        if (rest.startsWith(prefix) && rest.size() > prefix.size()) {
            rest = rest.mid(prefix.size());
            break;
        }
    }
    QString text;
    text.reserve(rest.size());
    bool wordStart = true;
    bool firstWord = true;
    for (int i = 0; i < rest.size(); ++i) {
        QChar ch = rest.at(i);
        if (i > 0 && ch.isUpper() && rest.at(i - 1).isLower()) {
            text += QLatin1Char(' ');
            wordStart = true;
        }
        if (wordStart) {
            // В подписи 1С первое слово с заглавной, остальные — со строчной.
            text += firstWord ? ch.toUpper() : ch.toLower();
            firstWord = false;
            wordStart = false;
        } else {
            text += ch.toLower();
        }
    }
    if (text.isEmpty())
        return name;
    return text;
}

QString buttonTitle(const FormItem &item, const FormDocument &document)
{
    if (!item.synonym.isEmpty())
        return item.synonym;
    if (!item.dataTitle.isEmpty())
        return item.dataTitle;
    for (const FormCommand &command : document.commands) {
        if (command.name == item.name && !command.synonym.isEmpty())
            return command.synonym;
    }
    return humanizeName(item.name);
}

QJsonObject makeButton(const FormItem &item, const FormDocument &document)
{
    QJsonObject properties;
    properties.insert(QStringLiteral("Type"), QStringLiteral("CommandBarButton"));
    const QString commandName = commandNameForButton(item, document);
    if (!commandName.isEmpty())
        properties.insert(QStringLiteral("CommandName"), commandName);
    const QString title = buttonTitle(item, document);
    if (!title.isEmpty())
        properties.insert(QStringLiteral("Title"), title);
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
    QString title = item.synonym;
    if (title.isEmpty() && !insideTable)
        title = item.dataTitle;
    if (!title.isEmpty())
        properties.insert(QStringLiteral("Title"), title);
    if (item.valueKind == FormValueKind::Boolean) {
        properties.insert(QStringLiteral("CheckBoxType"), QStringLiteral("Auto"));
    } else if (item.valueKind != FormValueKind::Unknown) {
        properties.insert(QStringLiteral("EditMode"), QStringLiteral("EnterOnInput"));
    }
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
    if (item.horizontalStretch == 0)
        properties.insert(QStringLiteral("HorizontalStretch"), QStringLiteral("false"));
    else if (item.horizontalStretch == 1)
        properties.insert(QStringLiteral("HorizontalStretch"), QStringLiteral("true"));
    if (!item.visible)
        properties.insert(QStringLiteral("Visible"), QStringLiteral("false"));

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

    if (tag == QLatin1String("Popup")) {
        properties.insert(QStringLiteral("Representation"), QStringLiteral("Text"));
    } else if (tag == QLatin1String("Pages")) {
        properties.insert(QStringLiteral("PagesRepresentation"), QStringLiteral("TabsOnTop"));
    } else if (tag == QLatin1String("Page")) {
        // Заголовок страницы уже в Title.
    } else if (tag == QLatin1String("ButtonGroup")) {
        properties.insert(QStringLiteral("Representation"), QStringLiteral("None"));
    } else {
        properties.insert(QStringLiteral("Group"), groupText(item.orientation));
        properties.insert(QStringLiteral("Representation"),
                          representationText(item.representation));
        properties.insert(QStringLiteral("Behavior"), behaviorText(item.behavior));
        properties.insert(QStringLiteral("ShowTitle"), boolText(item.showTitle));
        if (item.horizontalStretch == 1)
            properties.insert(QStringLiteral("HorizontalStretch"), QStringLiteral("true"));
        else if (item.horizontalStretch == 2)
            properties.insert(QStringLiteral("HorizontalStretch"), QStringLiteral("Auto"));
        if (!item.visible)
            properties.insert(QStringLiteral("Visible"), QStringLiteral("false"));
    }

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
    if (item.widthUnits > 0)
        properties.insert(QStringLiteral("Width"), QString::number(item.widthUnits));
    if (item.horizontalStretch == 1)
        properties.insert(QStringLiteral("HorizontalStretch"), QStringLiteral("true"));
    if (!item.visible)
        properties.insert(QStringLiteral("Visible"), QStringLiteral("false"));

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

QJsonObject makeTable(const FormItem &item, const FormDocument &document)
{
    QJsonObject properties;
    if (!item.dataPathText.isEmpty() && !item.dataPathText.contains(QLatin1Char('?')))
        properties.insert(QStringLiteral("DataPath"), item.dataPathText);
    properties.insert(QStringLiteral("Representation"), QStringLiteral("List"));
    properties.insert(QStringLiteral("AutoInsertNewRow"), QStringLiteral("true"));
    const QString title = item.titleText();
    if (!title.isEmpty())
        properties.insert(QStringLiteral("Title"), title);
    if (item.widthUnits > 0)
        properties.insert(QStringLiteral("Width"), QString::number(item.widthUnits));
    if (!item.visible)
        properties.insert(QStringLiteral("Visible"), QStringLiteral("false"));

    QJsonArray columns;
    const FormItem *tableBar = nullptr;
    // Имя табличной части — последний сегмент пути к данным таблицы.
    QString tableName;
    {
        const QStringList segments = item.dataPathText.split(QLatin1Char('.'));
        if (segments.size() >= 2)
            tableName = segments.last();
    }
    for (const FormItem &child : item.children) {
        if (!isRenderable(child))
            continue;
        if (child.type == FormItem::Type::Group) {
            if (!tableBar
                && (child.groupKind == kGroupCommandBar
                    || child.name.contains(QStringLiteral("КоманднаяПанель"),
                                           Qt::CaseInsensitive))) {
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
        if (child.type == FormItem::Type::Field) {
            columns.append(makeField(child, document, true, tableName));
            continue;
        }
        appendChildren({child}, document, &columns);
    }

    QJsonObject json;
    json.insert(QStringLiteral("tag"), QStringLiteral("Table"));
    json.insert(QStringLiteral("name"), item.name);
    json.insert(QStringLiteral("id"), QString::number(item.id));
    json.insert(QStringLiteral("properties"), properties);
    json.insert(QStringLiteral("authoredProperties"), properties);
    json.insert(QStringLiteral("childItems"), columns);
    json.insert(QStringLiteral("events"), QJsonArray());
    // Свою командную панель таблица показывает только если она описана в форме.
    // Иначе платформа кладёт команды таблицы в общую панель формы, и отдельная
    // панель вроде «Создать» у списка была бы лишней. У основной таблицы списка
    // («Список») своей панели нет — команды живут в панели формы.
    const bool mainList = tableName.compare(QStringLiteral("Список"),
                                            Qt::CaseInsensitive) == 0;
    if (mainList)
        properties.insert(QStringLiteral("CommandBarLocation"), QStringLiteral("None"));
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
    case FormItem::Type::Decoration: {
        QJsonObject properties;
        const QString text = item.titleText();
        if (!text.isEmpty())
            properties.insert(QStringLiteral("Title"), text);
        if (!item.visible)
            properties.insert(QStringLiteral("Visible"), boolText(false));
        QJsonObject json;
        json.insert(QStringLiteral("tag"), QStringLiteral("LabelDecoration"));
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
                                const FormItem *authoredBar)
{
    QJsonArray standard;
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
    // «Создать на основании» платформа добавляет только документам
    // и справочникам; в формах отчётов и обработок этой команды нет.
    const QString kind = kindTagFor(document.ownerTitle);
    if (kind == QLatin1String("Document") || kind == QLatin1String("Catalog")) {
        standard.append(makePopup(QStringLiteral("_global_create_based_on"),
                                  QStringLiteral("Создать на основании"), QJsonArray()));
    }
    QJsonObject bar = buildCommandBarNode(authoredBar, document, standard, false,
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
                           bool mainAttribute = false)
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

    // Корневые элементы формы: командную панель формы отдаём отдельным узлом
    // autoCommandBar (так её ожидает рендерер), остальные — в childItemsRoot.
    // Панель узнаём и по имени: платформа восстанавливает CommandBar из группы
    // «ГруппаКоманднаяПанель», у которой в бинарной форме нет кода 9. Если
    // таких групп несколько (служебная «ФормаКоманднаяПанель» и настоящая
    // панель с кнопками), в панель берём ту, где больше содержимого.
    auto isCommandBarGroup = [](const FormItem &item) {
        return item.type == FormItem::Type::Group
            && (item.groupKind == kGroupCommandBar
                || item.name.contains(QStringLiteral("КоманднаяПанель"),
                                      Qt::CaseInsensitive));
    };
    const FormItem *authoredBar = nullptr;
    for (const FormItem &item : document.items) {
        if (!isCommandBarGroup(item))
            continue;
        if (!authoredBar || item.children.size() > authoredBar->children.size())
            authoredBar = &item;
    }
    QJsonArray rootItems;
    for (const FormItem &item : document.items) {
        if (isCommandBarGroup(item))
            continue;
        if (!isRenderable(item) || item.type == FormItem::Type::AutoCommandBar)
            continue;
        rootItems.append(makeItem(item, document));
    }
    model.insert(QStringLiteral("childItemsRoot"), rootItems);
    model.insert(QStringLiteral("autoCommandBar"),
                 buildAutoCommandBar(document, authoredBar));

    QJsonArray attributes;
    // Главный реквизит формы («Объект»/«Список») сама форма уже объявляет:
    // дублировать его нельзя — от порядка реквизитов зависят пути к данным.
    bool mainAttributeFound = false;
    for (const FormAttribute &attribute : document.attributes) {
        const bool main = attribute.name.compare(QStringLiteral("Объект"),
                                                 Qt::CaseInsensitive) == 0
            || attribute.name.compare(QStringLiteral("Object"), Qt::CaseInsensitive) == 0;
        if (main && !mainAttributeFound) {
            mainAttributeFound = true;
            attributes.append(buildAttribute(
                FormAttribute{QStringLiteral("Объект"),
                              attribute.synonym,
                              QStringLiteral("cfg:%1Object.%2")
                                  .arg(kindTagFor(document.ownerTitle),
                                       ownerName(document))},
                QStringLiteral("cfg:%1Object.%2")
                    .arg(kindTagFor(document.ownerTitle), ownerName(document)),
                true));
            continue;
        }
        attributes.append(buildAttribute(attribute, attribute.typeHint));
    }
    if (!mainAttributeFound) {
        attributes.prepend(buildAttribute(
            FormAttribute{QStringLiteral("Объект"), QString(), QString()},
            QStringLiteral("cfg:%1Object.%2")
                .arg(kindTagFor(document.ownerTitle), ownerName(document)),
            true));
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
    model.insert(QStringLiteral("commonPictures"), QJsonObject());
    model.insert(QStringLiteral("commandGroups"), QJsonObject());
    model.insert(QStringLiteral("version"), QStringLiteral("2.20"));
    model.insert(QStringLiteral("adoptedForm"), false);
    model.insert(QStringLiteral("interfaceMode"), QStringLiteral("Any"));
    model.insert(QStringLiteral("configInterfaceMode"),
                 QStringLiteral("TaxiEnableVersion8_2"));
    model.insert(QStringLiteral("objectMeta"), buildObjectMeta(document));
    model.insert(QStringLiteral("refMeta"), QJsonObject());
    model.insert(QStringLiteral("styleItems"), QJsonObject());
    model.insert(QStringLiteral("events"), QJsonArray());

    return QJsonDocument(model).toJson(QJsonDocument::Compact);
}

} // namespace v8reader
