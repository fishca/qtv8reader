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
