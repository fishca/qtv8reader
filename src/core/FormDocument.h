#ifndef QTV8READER_FORMDOCUMENT_H
#define QTV8READER_FORMDOCUMENT_H

#include <QHash>
#include <QString>
#include <QVector>

namespace v8reader {

// Положение заголовка элемента формы (TitleLocation в Form.xml).
// Числовые значения совпадают с сериализацией управляемой формы.
enum class FormTitleLocation {
    None = 0,
    Auto = 1,
    Left = 2,
    Top = 3,
    Right = 4,
    Bottom = 5
};

// Выделение группы (Representation в Form.xml): рамка-секция вокруг
// содержимого. Числовые значения сверены с Form.xml.
enum class FormGroupRepresentation {
    None = 0,
    Strong = 1,
    Weak = 2,
    Normal = 3
};

// Поведение группы (Behavior в Form.xml): обычная, раскрываемая
// с треугольником или всплывающая кнопка-заголовок.
enum class FormGroupBehavior {
    Usual = 0,
    Collapsible = 1,
    PopUp = 2
};

// Вид значения поля формы, выведенный из типа реквизита.
enum class FormValueKind {
    Unknown,
    String,
    Number,
    Boolean,
    Date,
    Reference,
    Enum,
    Composite,
    Picture
};

// Вид поля управляемой формы. В бинарной форме код вида записан целым числом
// непосредственно перед именем элемента; значения совпадают с порядком классов
// платформы. Сверено с Form.xml по 583 полям 26 форм (input, label, checkbox,
// picture, radio, spreadsheet, text document, calendar, progress bar, track
// bar, chart, graphical schema, HTML document, formatted document, planner,
// PDF document).
enum class FormFieldKind {
    Unknown = 0,
    Label = 1,               // LabelField — только текст, без поля ввода
    Input = 2,               // InputField
    CheckBox = 3,            // CheckBoxField
    Picture = 4,             // PictureField
    RadioButton = 5,         // RadioButtonField
    SpreadSheetDocument = 6, // SpreadSheetDocumentField
    TextDocument = 7,        // TextDocumentField
    Calendar = 8,            // CalendarField
    ProgressBar = 9,         // ProgressBarField
    TrackBar = 10,           // TrackBarField
    Chart = 11,              // ChartField
    GraphicalSchema = 14,    // GraphicalSchemaField
    HTMLDocument = 15,       // HTMLDocumentField
    FormattedDocument = 17,  // FormattedDocumentField
    Planner = 19,            // PlannerField
    PDFDocument = 20         // PDFDocumentField
};

// Код вида поля из бинарной формы → перечисление.
inline FormFieldKind formFieldKindFromCode(int code)
{
    switch (code) {
    case 1:
        return FormFieldKind::Label;
    case 2:
        return FormFieldKind::Input;
    case 3:
        return FormFieldKind::CheckBox;
    case 4:
        return FormFieldKind::Picture;
    case 5:
        return FormFieldKind::RadioButton;
    case 6:
        return FormFieldKind::SpreadSheetDocument;
    case 7:
        return FormFieldKind::TextDocument;
    case 8:
        return FormFieldKind::Calendar;
    case 9:
        return FormFieldKind::ProgressBar;
    case 10:
        return FormFieldKind::TrackBar;
    case 11:
        return FormFieldKind::Chart;
    case 14:
        return FormFieldKind::GraphicalSchema;
    case 15:
        return FormFieldKind::HTMLDocument;
    case 17:
        return FormFieldKind::FormattedDocument;
    case 19:
        return FormFieldKind::Planner;
    case 20:
        return FormFieldKind::PDFDocument;
    default:
        break;
    }
    return FormFieldKind::Unknown;
}

// Один сегмент пути к данным (DataPath) элемента формы. В бинарной форме
// путь записан не текстом, а ссылками на реквизиты метаданных:
// {длина, {индекс реквизита формы}, сегменты...}. Сегмент — это либо GUID
// реквизита объекта/табличной части, либо код стандартного реквизита
// (отрицательное число), либо индекс поля динамического списка.
struct FormPathSegment
{
    enum class Kind { Index, Attribute, StandardAttribute };

    Kind kind = Kind::Index;
    QString guid;   // Kind::Attribute
    int code = 0;   // Kind::StandardAttribute (отрицательный) или Kind::Index
};

// Элемент управляемой формы 1С (поле, группа, кнопка, таблица и т. д.).
struct FormItem
{
    enum class Type {
        Group,
        Field,
        Button,
        Table,
        Decoration,
        Column,
        Search,
        AutoCommandBar,
        Other
    };

    Type type = Type::Other;
    int typeCode = 0;      // код элемента в сериализации (22 — группа, 37 — поле, ...)
    int groupKind = -1;    // вид группы (9 — командная панель, 6 — группа кнопок, ...)
    int orientation = -1;  // 1 — горизонтально, 0 — вертикально (для групп)
    int id = 0;
    int widthUnits = -1;       // ширина поля в единицах формы (×10 px); -1 — авто
    int heightUnits = -1;      // высота в единицах формы
    int horizontalStretch = -1; // 0 — нет, 1 — да, 2 — авто
    int verticalStretch = -1;
    bool visible = true;
    bool showTitle = true;      // TitleLocation != None
    bool labelOnly = false;     // LabelField/PictureField без поля ввода
    // Вид поля из самой формы (LabelField, CheckBoxField, PictureField, ...).
    // Unknown — в сборке кода вида нет, тогда вид выводится из типа реквизита.
    FormFieldKind fieldKind = FormFieldKind::Unknown;
    int fieldKindCode = 0;      // исходный код вида из бинарной формы
    // Представления списка выбора, заданные в самой форме. Пустые строки —
    // значения, у которых представление берётся из метаданных.
    QStringList choiceItems;
    QString tooltip;            // подсказка элемента (ToolTip)
    // Декорация-картинка (PictureDecoration) отличается от надписи тем же
    // кодом вида перед именем, что и вид поля: 0 — надпись, 1 — картинка.
    // Сверено с Form.xml по 112 декорациям 26 форм.
    bool pictureDecoration = false;
    // Гиперссылка (Hyperlink): платформа рисует текст декорации ссылкой —
    // синим с подчёркиванием. Сверено с Form.xml по тем же 112 декорациям.
    bool hyperlink = false;
    // Картинка декорации: код стандартной картинки платформы (отрицательный)
    // либо GUID общей картинки конфигурации.
    int pictureStandardCode = 0;
    QString pictureGuid;
    // Положение заголовка поля: в конфигураторе подпись может стоять слева,
    // сверху, справа или снизу от поля ввода.
    FormTitleLocation titleLocation = FormTitleLocation::Auto;
    // Режим группы: выделение рамкой и поведение заголовка.
    FormGroupRepresentation representation = FormGroupRepresentation::None;
    FormGroupBehavior behavior = FormGroupBehavior::Usual;

    QString guid;
    QString name;
    QString synonym;
    // Кнопка выводится на командную панель (LocationInCommandBar); иначе
    // она живёт только в подменю «Еще».
    bool onCommandBar = true;
    QVector<FormItem> children;

    // Путь к данным (DataPath) и его расшифровка по метаданным.
    QVector<FormPathSegment> dataPath;
    QString dataPathText;      // «Объект.Билеты.Сумма» — для панели свойств
    QString dataTitle;         // заголовок реквизита из метаданных
    QString typeText;          // «Строка(50)», «СправочникСсылка.Организации»
    FormValueKind valueKind = FormValueKind::Unknown;
    int valueChars = 0;        // ширина поля в символах, выведенная из типа

    // Заголовок элемента из самой формы; если пуст — берётся dataTitle.
    QString titleText() const
    {
        return synonym.isEmpty() ? dataTitle : synonym;
    }

    QString title() const { return synonym.isEmpty() ? name : synonym; }
    QString typeTitle() const;
};

struct FormAttribute
{
    QString name;
    QString synonym;
    QString typeHint;
    // Реквизит-динамический список: в бинарной форме рядом с ним лежат
    // настройки списка (текст запроса и карта полей), по которым колонки
    // таблицы получают имена и заголовки.
    bool dynamicList = false;
    // Карта полей списка: номер поля в пути к данным (DataPath) → имя поля.
    // В бинарной форме путь колонки записан номером: «Список.#5», — а имена
    // лежат в настройках реквизита (FieldsMapItemIdN / FieldsMapItemNameN).
    QHash<int, QString> listFields;
    QString listQuery;         // текст запроса списка (QueryText)
    bool listManualQuery = false; // запрос задан вручную (ManualQuery)
    // Колонки таблицы значений, объявленной реквизитом формы: номер колонки →
    // имя, заголовок и тип. Путь колонки в бинарной форме записан номером
    // («ТаблицаТоваров.#1»), как и путь поля динамического списка.
    QHash<int, QString> columnNames;
    QHash<int, QString> columnTitles;
    QHash<int, QString> columnTypes;
};

struct FormCommand
{
    QString name;
    QString synonym;
};

// Метаданные владельца формы, которые рендерер «точного вида» ожидает
// в узле objectMeta: типы реквизитов с квалификаторами, заголовки, причём
// для колонок табличных частей — с префиксом «ТабличнаяЧасть.».
struct FormOwnerMeta
{
    bool valid = false;
    QString kindTag;   // «Document», «Catalog», …
    QString name;      // имя объекта метаданных («АвансовыйОтчет»)
    QString synonym;
    QString listPresentation;
    QHash<QString, QString> captions;    // «Товары.Сумма» → «Сумма»
    QHash<QString, QString> types;       // «Товары.Сумма» → «xs:decimal»
    QHash<QString, QString> numberQ;     // «Товары.Сумма» → «15:2»
    QHash<QString, QString> stringLen;   // «Комментарий» → «100»
    QHash<QString, QString> dateFraction; // «Дата» → «Date»
};

struct FormDocument
{
    bool valid = false;
    QString name;     // имя формы из метаданных
    QString synonym;  // синоним формы из метаданных
    // Владелец формы для заголовка окна: «Документ.АвансовыйОтчет».
    QString ownerTitle;
    // Синоним владельца: в окне 1С заголовок — «Авансовый отчет».
    QString ownerSynonym;
    // Типы и заголовки реквизитов владельца для рендерера.
    FormOwnerMeta ownerMeta;
    QString moduleText;
    QVector<FormItem> items;
    QVector<FormAttribute> attributes;
    QVector<FormCommand> commands;
    // Синонимы реквизитов объекта/формы: имя -> заголовок.
    QHash<QString, QString> titles;
    // Форма списка документа: платформа добавляет команды
    // «Создать на основании», «Еще», «?».
    bool runtimeListCommands = false;
    // Форма документа: платформа добавляет «Провести и закрыть»,
    // «Записать» и «Провести».
    bool runtimeDocumentCommands = false;
    // Форма элемента справочника: «Записать и закрыть» и «Записать».
    bool runtimeCatalogCommands = false;
    // Форма списка справочника: ключевые колонки по умолчанию.
    bool runtimeCatalogListCommands = false;

    int itemCount() const;
    int fieldCount() const;
};

} // namespace v8reader

#endif // QTV8READER_FORMDOCUMENT_H
