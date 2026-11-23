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

// Шрифт элемента: ссылка на стиль и явно заданные атрибуты. В записи шрифта
// атрибуты отмечены битовой маской: 2 — высота, 4 — жирный, 8 — курсив,
// 16 — подчёркивание, 32 — зачёркивание, 512 — масштаб. Признак −1 значит,
// что атрибут в форме не задан (в Form.xml его нет): рендерер берёт значение
// из стиля; 0/1 — явно выключен или включён.
struct FormFont
{
    QString reference;   // «style:Имя»; пусто — шрифт не задан
    int height = 0;      // пункты
    int scale = 0;       // проценты
    int bold = -1;
    int italic = -1;
    int underline = -1;
    int strikeout = -1;

    bool isEmpty() const { return reference.isEmpty(); }
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
    // Режим ориентации группы из узла макета {29,…}: 0 — вертикально,
    // 1 — горизонтально, 2 — горизонтально, если помещается (платформа
    // в этом случае не пишет Group в Form.xml), 3 — всегда горизонтально.
    int groupMode = -1;
    // Автозаполнение командной панели (Autofill). У панели (вид группы 9) нет
    // узла макета {29,…}: её служебные поля лежат прямо в записи, и признак
    // стоит через 14 полей после имени — это список из трёх чисел, третье
    // равно нулю, когда разработчик снял автозаполнение (Form.xml пишет
    // Autofill=false), и единице при значении по умолчанию. Сверено с Form.xml
    // по 80 панелям корпуса: расхождений нет. −1 — признак не прочитан.
    int autoFillFlag = -1;
    // Ориентация страницы (тег Page) из её записи макета {18,…}: 0 —
    // вертикальная (значение по умолчанию, Group в Form.xml не пишется),
    // 1 — Horizontal, 2 — HorizontalIfPossible, 3 — AlwaysHorizontal.
    // У страницы нет узла {29,…} обычной группы, поэтому код читается отдельно.
    int pageMode = -1;
    // Путь к данным заголовка (TitleDataPath): вместо текста в заголовке
    // страницы или группы может стоять значение реквизита — типовая
    // конфигурация так выводит количество строк табличной части
    // («Объект.Товары.RowsCount»). В записи страницы путь лежит четвёртым
    // полем блока {18,…}; пустой путь записан как {0}. Сверено с Form.xml по
    // 10 страницам корпуса (и по 329 случаям во всей конфигурации: 300
    // страниц и 29 обычных групп).
    QVector<FormPathSegment> titleDataPath;
    QString titleDataPathText;  // «Объект.Товары.RowsCount» — разобрано по метаданным
    // Группировка группы колонок таблицы из записи {2, режим, …}: 0 —
    // Horizontal, 2 — InCell (в ячейке); 1 — значение по умолчанию, платформа
    // Group в Form.xml не пишет. Заголовок группы (ShowInHeader) и закрепление
    // (FixingInTable) берутся из той же записи. Сверено с Form.xml по 75
    // группам колонок корпуса и 33 группам ещё 24 форм.
    int columnGroupMode = -1;
    // Компактное отображение группы кнопок (ButtonGroup.Representation=Compact):
    // платформа рисует кнопки значки без подписей. В записи группы стоит блок
    // {2, …, 2, вид}; четвёртое поле равно 2 при компактном отображении и нулю
    // при обычном. Сверено с Form.xml по 62 группам кнопок корпуса (11
    // компактных совпали).
    bool compactButtonGroup = false;
    // Разрешать изменение содержимого (EnableContentChange): платформа пишет
    // свойство только значением true. Лежит третьим полем после имени у всех
    // видов групп — обычной, всплывающей, группы кнопок, командной панели,
    // страниц и группы колонок; у отдельной страницы этого поля нет. Сверено с
    // Form.xml по 237 группам корпуса: у 18 со свойством поле равно единице,
    // у остальных 219 — нулю.
    bool enableContentChange = false;
    int id = 0;
    int widthUnits = -1;       // ширина поля в единицах формы (×10 px); -1 — авто
    int heightUnits = -1;      // высота в единицах формы
    // Представление страниц группы-страниц (PagesRepresentation): 0 — None,
    // 1 — TabsOnTop (значение по умолчанию), 2 — TabsOnBottom; -1 — не прочитано.
    int pagesRepresentation = -1;
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
    // Свойства поля и колонки таблицы, которые платформа пишет в Form.xml
    // только при отличии от значения по умолчанию (поэтому простой признак
    // «слот равен 1» недостаточен: у ShowInHeader/ShowInFooter значение по
    // умолчанию — true). Коды ниже — как в бинарной форме, −1 значит
    // «в форме не задано». Сверено с Form.xml по 372 колонкам и 233 полям
    // корпуса: расхождений по этим свойствам нет.
    bool readOnly = false;          // ReadOnly
    bool showInHeader = true;       // ShowInHeader
    bool showInFooter = true;       // ShowInFooter
    bool cellHyperlink = false;     // CellHyperlink
    bool autoCellHeight = false;    // AutoCellHeight
    int skipOnInput = -1;           // 0 — false, 1 — true; -1 — не задано
    bool defaultItem = false;       // DefaultItem
    int horizontalAlign = -1;       // 0 — Left, 1 — Center, 2 — Right, 3 — Auto
    int headerHorizontalAlign = -1; // то же (заголовок колонки)
    int footerHorizontalAlign = -1; // то же (подвал колонки)
    int verticalAlign = -1;         // 0 — Top, 1 — Center, 2 — Bottom, 3 — Auto
    int editMode = -1;              // 0 — Directly, 1 — Auto, 2 — EnterOnInput
    int fixingInTable = -1;         // 0 — None, 1 — Left, 2 — Right
    int toolTipRepresentation = -1; // 3 — Button, 5 — ShowTop, 7 — ShowBottom
    QString inputHint;              // текст-подсказка ввода (InputHint)
    // Свойства поля из записи макета ({36,…} у поля ввода, {10,…} у
    // поля-картинки). Платформа пишет их в Form.xml только при отличии от
    // значения по умолчанию, поэтому у признаков хранится значение формы
    // (−1 — не задано), а умолчания стоят в инициализаторах. Сверено с
    // Form.xml по 326 полям ввода и 56 полям-картинкам корпуса.
    bool autoMaxWidth = true;      // AutoMaxWidth (по умолчанию true)
    bool autoMaxHeight = true;     // AutoMaxHeight (по умолчанию true)
    int maxWidth = 0;              // MaxWidth (0 — не задана)
    bool wrap = true;              // Wrap (по умолчанию true)
    int choiceButton = -1;         // 0 — false, 1 — true, 2 — по умолчанию
    int clearButton = -1;          // то же
    int spinButton = -1;           // то же
    int openButton = -1;           // то же
    int dropListButton = -1;       // то же
    int choiceListButton = -1;     // то же
    bool chooseType = true;        // ChooseType (по умолчанию true)
    bool textEdit = true;          // TextEdit (по умолчанию true)
    bool listChoiceMode = false;   // ListChoiceMode
    int autoMarkIncomplete = -1;   // AutoMarkIncomplete (2 — по умолчанию)
    int autoChoiceIncomplete = -1; // AutoChoiceIncomplete (2 — по умолчанию)
    int choiceHistoryOnInput = -1; // 0 — Auto, 1 — DontUse
    int choiceButtonRepresentation = -1; // 0 — по умолчанию, 2/3 — явно
    // Вид переключателя (RadioButtonType): в записи переключателя есть блок
    // {8, {3, вид, "", …}}, которого нет ни у одного другого вида поля.
    // Второе поле внутренней записи: 1 — Auto (платформа пишет явно),
    // 2 — Tumbler; 0 (RadioButton) в корпусе не встречается. −1 — не задано.
    // Сверено с Form.xml по 9 переключателям корпуса.
    int radioButtonType = -1;
    int choiceFoldersAndItems = -1; // 0 — Items, 1 — Folders, 2 — FoldersAndItems
    // Формат поля и предел значения из записи поля ввода {36,…}: двадцать
    // девятое и тридцатое поля — строки формата, шестнадцатое и семнадцатое —
    // пределы MinValue и MaxValue, шестьдесят шестое — признак расширенного
    // редактирования нескольких значений. Сверено с Form.xml по 7 форматам,
    // 6 форматам редактирования, 5 пределам и 34 полям с расширенным
    // редактированием корпуса.
    QString format;                  // Format
    QString editFormat;              // EditFormat
    QString minValue;                // MinValue
    QString maxValue;                // MaxValue
    bool extendedEditMultipleValues = false; // ExtendedEditMultipleValues
    // Вид флажка (CheckBoxType) из записи надписи/флажка {11,…}: двенадцатое
    // поле равно 3 у переключателя, 0 — вид по умолчанию (платформа пишет в
    // Form.xml Auto). Сверено с Form.xml по 3 переключателям корпуса (все три
    // совпали) и 55 флажкам с явным Auto.
    int checkBoxType = 0;
    int fileDragMode = -1;         // поле-картинка: 0 — AsFile, 1 — UseAsFile
    // Таблица: настройки, которые платформа пишет в Form.xml только для
    // отличающегося значения. Сверено с Form.xml по 48 таблицам корпуса.
    // Отображение таблицы (Representation): 0 — List, 1 — Table (по
    // умолчанию, в XML не пишется), 2 — Tree; −1 — не задано. Код лежит
    // третьим полем записи, сразу после ссылки на имя. Сверено с Form.xml по
    // 54 таблицам корпуса: 25 List, 15 умолчаний и 4 Tree совпали.
    int tableRepresentation = -1;
    bool useAlternationRowColor = false; // UseAlternationRowColor
    bool autoInsertNewRow = false;       // AutoInsertNewRow
    bool enableStartDrag = false;        // EnableStartDrag
    bool enableDrag = false;             // EnableDrag
    int commandBarLocation = -1;         // 0 — None, 1 — Auto, 2 — Top, 3 — Bottom
    // Положение заголовка таблицы: 0 — None (по умолчанию, в XML не пишется),
    // 1 — Auto, 2 — Left, 3 — Top, 4 — Right; −1 — не задано.
    int tableTitleLocation = -1;
    // Оформление таблицы: линии сетки (HorizontalLines и VerticalLines) —
    // двадцать седьмое и двадцать восьмое поля записи таблицы (0 — false,
    // 1 — умолчание, в Form.xml не пишется), начальное раскрытие дерева
    // (InitialTreeView) — тридцать четвёртое поле (0 — Default, 1 —
    // ExpandTopLevel, 2 — ExpandAllLevels). Сверено с Form.xml по 54 таблицам
    // корпуса и по формам, где выключена только одна из сеток:
    // «ВидыКонтактнойИнформации/ФормаСписка» (только HorizontalLines=false) и
    // «ПрограммыЭлектроннойПодписиИШифрования/ОшибкаОбращенияКПрограмме»
    // (только VerticalLines=false). −1 — поле не прочитано.
    int horizontalLines = -1;
    int verticalLines = -1;
    int initialTreeView = -1;
    // Путь картинки строки (RowPictureDataPath): тридцать девятое поле записи
    // таблицы — [0] (свойство не задано) или ["1", [номер поля]]. Номер
    // 10000000 — стандартное поле DefaultPicture динамического списка, иначе
    // это номер колонки таблицы значений или поля списка. Сверено с Form.xml
    // по 54 таблицам корпуса: у 12 таблиц с путём поле задано, у 42 — [0].
    int rowPictureFieldId = 0;
    // Заголовок таблицы (Header) и настройки динамического списка. Настройки
    // лежат в записи таблицы блоком «количество, код, значение, …»; в XML они
    // пишутся только у таблиц, связанных с динамическим списком. Сверено с
    // Form.xml по 87 таблицам динамических списков и 76 обычным таблицам.
    bool tableHeader = true;             // Header (по умолчанию true)
    // Разрешение менять состав и порядок строк таблицы: платформа пишет
    // ChangeRowSet и ChangeRowOrder только значением false, когда строки
    // неизменяемы. Оба кода лежат рядом с DefaultItem (0 — false, 1 —
    // умолчание). Сверено с Form.xml по 9 таблицам корпуса, у которых
    // свойства заданы, и по 45 таблицам без них.
    int changeRowSet = -1;
    int changeRowOrder = -1;
    // Команды, исключённые из командной панели таблицы (ExcludedCommands):
    // в записи таблицы это список [количество, GUID, …] стандартных команд
    // элемента. Платформа пишет его в Form.xml отсортированным по имени.
    QStringList excludedCommands;
    // Отступы внутри группы (VerticalSpacing и HorizontalSpacing): пятнадцатое
    // и шестнадцатое поля записи макета {29,…}. 0 — Auto (в Form.xml не
    // пишется), 1 — None, 2 — Half, 3 — Single, 4 — OneAndHalf, 5 — Double.
    // Сверено с Form.xml по 8 группам с VerticalSpacing и 3 с HorizontalSpacing.
    int verticalSpacing = 0;
    int horizontalSpacing = 0;
    // Ширина дочерних элементов группы (ChildItemsWidth) — второе поле записи
    // макета {29,…}: 1 — Equal, 2 — LeftWide, 3 — LeftWidest, 4 — LeftNarrow,
    // 5 — LeftNarrowest; 0 — значение по умолчанию, в Form.xml не пишется.
    // Там же: девятнадцатое поле — сквозное выравнивание (ThroughAlign), 0 —
    // Use (2 — умолчание), и двадцать первое — объединение группы (United),
    // 0 — false (1 — умолчание). Сверено с Form.xml по 237 обычным группам
    // корпуса (5 ширин, 11 United=false и 2 ThroughAlign=Use) и по формам
    // «ВнешниеПользователи» (LeftNarrowest) и «ЗаявкаНаОткрытиеЛицевыхСчетов
    // Сотрудников» (LeftNarrow).
    int childItemsWidth = 0;
    int throughAlign = -1;
    bool united = true;
    int autoRefresh = -1;                // AutoRefresh
    int autoRefreshPeriod = -1;          // AutoRefreshPeriod
    QString period;                      // Period («Custom, начало, конец»)
    int tableChoiceFolders = -1;         // ChoiceFoldersAndItems
    int restoreCurrentRow = -1;          // RestoreCurrentRow
    int showRoot = -1;                   // ShowRoot
    int allowRootChoice = -1;            // AllowRootChoice
    bool hasRowFilter = false;           // RowFilter (пустая структура отбора)
    int updateOnDataChange = -1;         // UpdateOnDataChange: 0 — Auto
    int allowGettingCurrentRowUrl = -1;  // AllowGettingCurrentRowURL
    bool hasTopLevelParent = false;      // TopLevelParent (строка, обычно пустая)
    QString topLevelParent;
    // Кнопка: вид (Type), отображение (Representation), пропуск при вводе и
    // выравнивание в группе. −1 — в форме не задано.
    int buttonType = -1;                 // 0 — CommandBarButton, 1 — UsualButton, 2 — Hyperlink
    int buttonRepresentation = -1;       // 0 — Text, 1 — Picture, 2 — PictureAndText, 3 — Auto
    int buttonGroupHorizontalAlign = -1; // 3 — Auto (по умолчанию)
    int buttonGroupVerticalAlign = -1;   // 3 — Auto (по умолчанию)
    int displayImportance = -1;          // 3 — Usual
    // Декорация-картинка: размер (1 — Stretch, 2 — Proportionally).
    int pictureSize = -1;
    // Оформление элемента. Ссылка на общий стиль конфигурации записана в форме
    // GUID-ом, на стиль платформы — отрицательным кодом, абсолютный цвет —
    // числом RGB. После разбора значения хранятся так же, как их пишет
    // Form.xml: «style:Имя» или «#RRGGBB»; шрифт — ссылкой на стиль, её
    // рендерер разбирает сам (начертание и размер задаёт имя стиля).
    QString textColor;
    QString titleTextColor;
    QString backColor;
    QString borderColor;        // рамка поля, таблицы и её колонок
    // Картинки элемента: «CommonPicture.Имя» (общая картинка конфигурации,
    // в форме записана GUID-ом), «StdPicture.Имя» (стандартная картинка
    // платформы) или «Abs:<свойство>.bmp» — картинка, встроенная прямо в
    // форму (её байты лежат в записи элемента). ValuesPicture — картинка
    // значений поля, HeaderPicture — значок в заголовке колонки, Picture —
    // картинка декорации или кнопки.
    QString valuesPictureRef;
    QString headerPictureRef;
    QString pictureRef;
    // Данные встроенных картинок: суффикс («ValuesPicture.bmp»,
    // «HeaderPicture.bmp», «Picture.bmp») -> base64 BMP. Рендерер ищет их
    // в commonPictures по ключу «@item:<имя элемента>/<суффикс>».
    QHash<QString, QString> inlinePictures;
    FormFont font;              // FontSpec
    FormFont titleFont;         // TitleFontSpec
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
    // Поведение задано в форме явно: платформа пишет Behavior в Form.xml не
    // у всех групп, поэтому умолчание не подставляем.
    bool hasBehavior = false;
    // Выравнивание элемента внутри группы (GroupHorizontalAlign и
    // GroupVerticalAlign). Лежит в «хвосте» записи элемента, поэтому отсчёт
    // ведётся от конца записи: необязательные вложенные записи (контекстное
    // меню, расширенная подсказка, оформление) его не сдвигают. У декорации
    // горизонтальное выравнивание — четвёртый слот с конца, вертикальное —
    // третий; у группы — третий и второй. Коды: 0 — Left/Top, 1 — Center,
    // 2 — Right/Bottom; 3 — Auto (значение по умолчанию, платформа в Form.xml
    // не пишет); −1 — не задано. Сверено с Form.xml по 112 декорациям и 361
    // группе корпуса: расхождений нет.
    int groupHorizontalAlign = -1;
    int groupVerticalAlign = -1;
    // Выравнивание содержимого внутри элемента (HorizontalAlign и
    // VerticalAlign в Form.xml). У группы коды лежат в узле макета {29,…} —
    // 17-е и 18-е поля (0 — Left/Top, 1 — Center, 2 — Right/Bottom,
    // 3 — Auto, значение по умолчанию), у декорации — в записи оформления
    // {5,…} 2-е и 3-е поля (у горизонтального 0 — Left, значение по
    // умолчанию; у вертикального 0 — Top, а умолчание — 3).
    // −1 — не задано, в модель не идёт. Сверено с Form.xml по 524 группам и
    // 73 надписям корпуса.
    int contentHorizontalAlign = -1;
    int contentVerticalAlign = -1;

    QString guid;
    QString name;
    QString synonym;
    // Кнопка выводится на командную панель (LocationInCommandBar); иначе
    // она живёт только в подменю «Еще».
    bool onCommandBar = true;
    // Код LocationInCommandBar из записи кнопки: 1 — InAdditionalSubmenu,
    // 2 — InCommandBar, 3 — InCommandBarAndInAdditionalSubmenu; −1 — свойство
    // не задано (значение по умолчанию — на панели).
    int commandBarLocationCode = -1;
    // Ссылка кнопки на команду в бинарной форме: {номер, GUID}. Номер — это
    // идентификатор команды формы, 0 — стандартная команда платформы или
    // общая команда конфигурации, иное — идентификатор элемента-владельца
    // (таблицы, поля форматированного документа) для его стандартных команд.
    int commandRef = -1;
    QString commandGuid;
    // Готовое имя команды для модели: «Form.Command.Создать»,
    // «Form.StandardCommand.Write», «Form.Item.Товары.StandardCommand.Add».
    QString commandName;
    QVector<FormItem> children;

    // Путь к данным (DataPath) и его расшифровка по метаданным.
    QVector<FormPathSegment> dataPath;
    QString dataPathText;      // «Объект.Билеты.Сумма» — для панели свойств
    QString dataTitle;         // заголовок реквизита из метаданных
    // Форматированный заголовок декорации (TitleFormatted): в записи формы
    // рядом с обычным заголовком лежит его размеченный вариант (шрифт, цвет,
    // ссылки), а признак говорит, что заголовок задан именно им. Платформа
    // пишет TitleFormatted в Form.xml только при явном значении.
    bool titleFormatted = false;   // заголовок размечен как форматированный
    bool hasTitleFormatted = false; // признак задан в форме
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
    // Идентификатор реквизита в контейнере формы. На него ссылается первый
    // сегмент пути к данным (DataPath): {1, {3}} — реквизит с номером 3.
    // Номера не совпадают с порядком объявления (у «Авансового отчёта»
    // «Объект» — 1, «Состояние документа» — 8, «Итого предыдущий аванс» — 3),
    // поэтому путь расшифровывается только по номеру.
    int id = -1;
    // Реквизит-динамический список: в бинарной форме рядом с ним лежат
    // настройки списка (текст запроса и карта полей), по которым колонки
    // таблицы получают имена и заголовки.
    bool dynamicList = false;
    // Главная таблица списка не указана (MainTableCategory = 0): список
    // строится произвольным запросом, а не поверх таблицы объекта. Платформа
    // пишет путь картинки строки такого списка как «~Таблица.DefaultPicture»
    // (с «~»); 3 формы корпуса.
    bool customMainTable = false;
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
    // Идентификатор команды в коллекции формы: на него ссылаются кнопки
    // парой {идентификатор, GUID коллекции}. Идентификаторы не обязаны идти
    // подряд и в том же порядке, что записи в контейнере, поэтому ищем
    // команду по номеру, а не по позиции в списке.
    int id = 0;
    QString collectionGuid;
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
    // Значения стилей-цветов, которые использует форма: имя стиля -> «#RRGGBB».
    // Рендерер получает их отдельной таблицей styleItems: по одной ссылке
    // «style:Имя» цвет не восстановить, значение лежит в данных стиля.
    QHash<QString, QString> styleItems;
    // Общие картинки, на которые ссылается форма: имя -> base64 данных.
    // Рендерер разбирает их сам (узел commonPictures); MIME лежит отдельно:
    // общие картинки конфигурации — ZIP с вариантами PNG, встроенные в форму
    // (ключ «@item:<имя>/<свойство>.bmp») — обычный BMP.
    QHash<QString, QString> commonPictures;
    QHash<QString, QString> commonPictureMimes;
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
