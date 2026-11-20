#include "gui/FormView.h"

#include "controls/ButtonControl.h"
#include "controls/CommandBarControl.h"
#include "controls/FieldControl.h"
#include "controls/FormControl.h"
#include "controls/FormStyle.h"
#include "controls/SearchControl.h"
#include "controls/StaticControl.h"
#include "controls/TabControl.h"
#include "controls/TableControl.h"
#include "gui/BslHighlighter.h"
#include "gui/web/WebView2Host.h"

#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPalette>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSet>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

using controls::ButtonIcon;


struct PreviewControl
{
    enum class Kind { Field, Button, Table, PageTab, Decoration, Search, SearchOptions, Separator, HorizontalLine, FilterChip, GroupTitle };
    Kind kind = Kind::Field;
    ButtonIcon icon = ButtonIcon::None;
    bool iconOnly = false;
    QRectF rect;
    QString title;
    QStringList columns;
    QVector<double> columnWidths;
    double labelWidth = 0.0;
    double inputWidth = -1.0;
    int labelPlacement = 0;   // 0 — слева, 1 — сверху, 2 — справа, 3 — снизу
    bool selectedTab = false;
    bool actionColumn = false;
    bool checkBox = false;
    bool checkboxOnly = false;
    bool checkBoxLeading = false;
    bool dropDown = false;
    bool primary = false;
    bool labelOnly = false;
    bool calendar = false;
    bool bordered = false;
    bool choiceButton = false;
    bool banner = false;
    bool closeGlyph = false;
    // Заголовок группы-секции: режим выделения и способ раскрытия.
    int groupRepresentation = 0;
    bool groupCollapsible = false;
    bool groupPopup = false;
};

bool looksTechnical(const QString &text)
{
    return text.contains(QLatin1Char('='))
        || text.startsWith(QStringLiteral("ДЛФ"))
        || text.contains(QLatin1Char('%'));
}

QFont formFont()
{
    QFont font(QStringLiteral("Tahoma"));
    font.setPointSizeF(9.0);
    return font;
}

// Имена реквизитов, которые в 1С являются ссылочными: у их полей есть
// кнопка выбора (три точки) справа.
bool looksLikeReferenceField(const QString &name)
{
    static const QStringList parts = {
        QStringLiteral("Организация"), QStringLiteral("ФизЛицо"),
        QStringLiteral("Склад"), QStringLiteral("Подразделение"),
        QStringLiteral("Ответственный"), QStringLiteral("Валюта"),
        QStringLiteral("Контрагент"), QStringLiteral("Номенклатура"),
        QStringLiteral("Пользователь"), QStringLiteral("Документ"),
        QStringLiteral("Ссылка"), QStringLiteral("Счет"),
        QStringLiteral("Статья"), QStringLiteral("Договор"),
        QStringLiteral("Банк"), QStringLiteral("Касса"),
        QStringLiteral("ТипЦен"), QStringLiteral("Проект")
    };
    for (const QString &part : parts) {
        if (name.contains(part, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool isServiceItem(const v8reader::FormItem &item)
{
    using Type = v8reader::FormItem::Type;
    if (item.type == Type::AutoCommandBar)
        return true;
    if (item.type == Type::Group && item.groupKind == 8)
        return true;
    if (!item.visible)
        return true;
    // Баннеры платформа показывает по условию (сканер, чеки, билеты).
    // В статическом просмотре оставляем только «билеты» — он есть на форме.
    if (item.type == Type::Group
        && item.name.contains(QStringLiteral("Баннер"))
        && !item.name.contains(QStringLiteral("УчетБилетов"))) {
        return true;
    }
    if (item.name.contains(QStringLiteral("КонтекстноеМеню"))
        || item.name.endsWith(QStringLiteral("РасширеннаяПодсказка"))
        || item.name.endsWith(QStringLiteral("ExtendedTooltip")))
        return true;
    return false;
}

bool isServiceDecoration(const v8reader::FormItem &item)
{
    return item.type == v8reader::FormItem::Type::Decoration
        && item.synonym.isEmpty();
}

// Группа без видимого содержимого не должна занимать высоту: в форме
// списка такие служебные группы оставляли пустую полосу под панелью.
bool hasRenderableContent(const v8reader::FormItem &item)
{
    using Type = v8reader::FormItem::Type;
    if (!item.visible)
        return false;
    if (item.type == Type::Group) {
        for (const v8reader::FormItem &child : item.children) {
            if (isServiceItem(child) || isServiceDecoration(child))
                continue;
            if (hasRenderableContent(child))
                return true;
        }
        return false;
    }
    if (item.type == Type::Decoration)
        return !item.synonym.isEmpty();
    return item.type == Type::Field || item.type == Type::Button
        || item.type == Type::Table || item.type == Type::Search;
}

QString humanizeIdentifier(const QString &text)
{
    QString result;
    QString word;
    auto flush = [&]() {
        if (word.isEmpty())
            return;
        word[0] = word.at(0).toUpper();
        for (int i = 1; i < word.size(); ++i)
            word[i] = word.at(i).toLower();
        if (!result.isEmpty())
            result += QLatin1Char(' ');
        result += word;
        word.clear();
    };
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        const bool upper = ch.isUpper();
        const bool nextLower = i + 1 < text.size() && text.at(i + 1).isLower();
        if (upper && !word.isEmpty() && (word.at(word.size() - 1).isLower() || nextLower))
            flush();
        word += ch;
    }
    flush();
    return result;
}

ButtonIcon iconForButton(const v8reader::FormItem &item)
{
    const QString text = item.name + QLatin1Char(' ') + item.synonym;
    auto has = [&](const QString &part) {
        return text.contains(part, Qt::CaseInsensitive);
    };
    if (has(QStringLiteral("СоздатьНаОсновании")))
        return ButtonIcon::None;
    if (item.name == QStringLiteral("Создать2"))
        return ButtonIcon::None;
    if (has(QStringLiteral("Создать")))
        return ButtonIcon::Create;
    if (has(QStringLiteral("АтКт")))
        return ButtonIcon::AtKt;
    if (has(QStringLiteral("Скопировать")))
        return ButtonIcon::Copy;
    if (has(QStringLiteral("НастроитьСписок")))
        return ButtonIcon::ListSettings;
    if (has(QStringLiteral("Просмотреть")))
        return ButtonIcon::List;
    if (has(QStringLiteral("Изменить")))
        return ButtonIcon::Pencil;
    if (has(QStringLiteral("Печать")))
        return ButtonIcon::Print;
    if (has(QStringLiteral("ПоказатьДвижения")))
        return ButtonIcon::AtKt;
    if (has(QStringLiteral("СтруктураПодчиненности")))
        return ButtonIcon::Hierarchy;
    if (has(QStringLiteral("ПрисоединенныеФайлы")))
        return ButtonIcon::Paperclip;
    if (has(QStringLiteral("ДополнительныеСведения")))
        return ButtonIcon::List;
    if (has(QStringLiteral("ИсторияИзменений")))
        return ButtonIcon::Clock;
    if (has(QStringLiteral("Заполнить")))
        return ButtonIcon::Fill;
    if (has(QStringLiteral("Отчет")))
        return ButtonIcon::Chart;
    if (has(QStringLiteral("ЭДО")))
        return ButtonIcon::Envelope;
    if (has(QStringLiteral("Справка")))
        return ButtonIcon::Help;
    if (has(QStringLiteral("Новости")))
        return ButtonIcon::News;
    if (has(QStringLiteral("НовоеОкно")))
        return ButtonIcon::Window;
    return ButtonIcon::None;
}

QString humanizeCommand(const QString &name)
{
    QString text = name;
    static const QStringList prefixes = {
        QStringLiteral("ФормаОбщаяКоманда"),
        QStringLiteral("ОбщаяКоманда"),
        QStringLiteral("Форма"),
        QStringLiteral("Команда")
    };
    for (const QString &prefix : prefixes) {
        if (text.startsWith(prefix) && text.size() > prefix.size()) {
            text = text.mid(prefix.size());
            break;
        }
    }
    return humanizeIdentifier(text);
}

class LayoutEngine
{
public:
    LayoutEngine(const v8reader::FormDocument &document, double width)
        : m_document(document), m_width(width),
          m_font(formFont()), m_metrics(m_font)
    {
        for (const v8reader::FormCommand &command : document.commands) {
            if (!command.name.isEmpty() && !command.synonym.isEmpty())
                m_commandTitles.insert(command.name, command.synonym);
        }
    }

    void build(QVector<PreviewControl> *controls, double *contentHeight,
               double targetHeight = 0.0)
    {
        m_controls = controls;
        QVector<v8reader::FormItem> localItems = m_document.items;

        // Платформа сама добавляет в форму стандартные команды записи
        // в начало командной панели; в бинарной форме их нет.
        if (m_document.runtimeDocumentCommands || m_document.runtimeCatalogCommands) {
            std::function<v8reader::FormItem *(v8reader::FormItem &)> findCommandBar =
                [&](v8reader::FormItem &item) -> v8reader::FormItem * {
                    if (isTopCommandBar(item))
                        return &item;
                    for (v8reader::FormItem &child : item.children) {
                        if (v8reader::FormItem *found = findCommandBar(child))
                            return found;
                    }
                    return nullptr;
                };
            for (v8reader::FormItem &root : localItems) {
                v8reader::FormItem *group = findCommandBar(root);
                if (!group)
                    continue;
                const bool document = m_document.runtimeDocumentCommands;
                v8reader::FormItem close;
                close.type = v8reader::FormItem::Type::Button;
                close.typeCode = 31;
                close.name = document ? QStringLiteral("ПровестиИЗакрыть")
                                      : QStringLiteral("ЗаписатьИЗакрыть");
                close.synonym = document ? QStringLiteral("Провести и закрыть")
                                         : QStringLiteral("Записать и закрыть");
                v8reader::FormItem write;
                write.type = v8reader::FormItem::Type::Button;
                write.typeCode = 31;
                write.name = QStringLiteral("Записать");
                write.synonym = QStringLiteral("Записать");
                group->children.insert(0, close);
                group->children.insert(1, write);
                if (document) {
                    v8reader::FormItem post;
                    post.type = v8reader::FormItem::Type::Button;
                    post.typeCode = 31;
                    post.name = QStringLiteral("Провести");
                    post.synonym = QStringLiteral("Провести");
                    group->children.insert(2, post);
                }
                break;
            }
        }

        // Платформа прячет подменю и новости командной панели в «Еще»:
        // вычищаем их из дерева, иначе они занимают место на панели.
        if (m_document.runtimeDocumentCommands && !m_document.runtimeListCommands) {
            std::function<void(v8reader::FormItem &)> prune = [&](v8reader::FormItem &item) {
                QVector<v8reader::FormItem> kept;
                for (const v8reader::FormItem &child : item.children) {
                    const bool submenu = child.type == v8reader::FormItem::Type::Group
                        && child.groupKind == 1
                        && child.name != QStringLiteral("СоздатьНаОсновании");
                    const bool news = child.name.contains(
                        QStringLiteral("КонтекстныеНовости"));
                    if (submenu || news)
                        continue;
                    kept.append(child);
                }
                item.children = kept;
                for (v8reader::FormItem &child : item.children)
                    prune(child);
            };
            for (v8reader::FormItem &root : localItems)
                prune(root);
        }

        QVector<const v8reader::FormItem *> items;
        for (const v8reader::FormItem &item : localItems) {
            if (!isServiceItem(item) && hasRenderableContent(item))
                items.append(&item);
        }
        *contentHeight = layoutVertical(items, QRectF(0.0, 0.0, m_width, 100000.0), 0);

        // В конфигураторе над таблицей списка показывается строка отборов
        // с образцами значений. Добавляем её как отдельный контрол.
        if (m_document.runtimeListCommands) {
            PreviewControl *table = nullptr;
            double bestArea = 0.0;
            for (PreviewControl &control : *controls) {
                if (control.kind != PreviewControl::Kind::Table)
                    continue;
                const double area = control.rect.width() * control.rect.height();
                if (!table || area > bestArea) {
                    table = &control;
                    bestArea = area;
                }
            }
            if (table) {
                const double chipsTop = table->rect.top();
                const double chipsLeft = std::max(0.0, table->rect.left() - 6.0);
                const double chipHeight = 26.0;
                const double chipGap = 4.0;
                for (PreviewControl &control : *controls) {
                    if (control.rect.top() >= chipsTop - 0.1)
                        control.rect.moveTop(control.rect.top() + chipHeight + chipGap);
                }
                const QStringList chipTexts = {
                    QStringLiteral("Поле1: Значение1"),
                    QStringLiteral("Поле2: Значение2")
                };
                double chipX = chipsLeft;
                for (const QString &text : chipTexts) {
                    const double width = std::min(220.0, std::max(120.0,
                        m_metrics.horizontalAdvance(text) + 34.0));
                    PreviewControl chip;
                    chip.kind = PreviewControl::Kind::FilterChip;
                    chip.rect = QRectF(chipX, chipsTop, width, chipHeight);
                    chip.title = text;
                    m_controls->append(chip);
                    chipX += width + 8.0;
                }
                *contentHeight += chipHeight + chipGap;
            }
        }

        // В окне 1С таблица забирает свободную высоту, а элементы под ней
        // (подвал формы) прижимаются к низу. Повторяем это для предпросмотра.
        if (targetHeight > *contentHeight) {
            PreviewControl *table = nullptr;
            double bestArea = 0.0;
            for (PreviewControl &control : *controls) {
                if (control.kind != PreviewControl::Kind::Table)
                    continue;
                const double area = control.rect.width() * control.rect.height();
                if (!table || area > bestArea) {
                    table = &control;
                    bestArea = area;
                }
            }
            if (table) {
                const double delta = targetHeight - *contentHeight;
                const double oldBottom = table->rect.bottom();
                table->rect.setHeight(table->rect.height() + delta);
                for (PreviewControl &control : *controls) {
                    if (&control == table || control.rect.top() < oldBottom - 0.5)
                        continue;
                    control.rect.translate(0.0, delta);
                }
                *contentHeight = targetHeight;
            }
        }
    }

private:
    QString fieldLabel(const v8reader::FormItem &item) const
    {
        // Заголовок поля, заданный в самой форме, важнее синонима реквизита:
        // «НазначениеАванса» в форме подписано «Назначение», а не
        // «Назначение аванса».
        if (!item.synonym.isEmpty() && !looksTechnical(item.synonym)) {
            return item.synonym;
        }
        // Путь к данным расшифрован по метаданным: заголовок реквизита
        // совпадает с конфигуратором («Код / артикул», «Ставка НДС»).
        if (!item.dataTitle.isEmpty())
            return item.dataTitle;
        QString title = m_document.titles.value(item.name);
        if (title.isEmpty()) {
            const QString synonym = item.synonym;
            if (!synonym.isEmpty() && !looksTechnical(synonym))
                title = synonym;
        }
        if (title.isEmpty())
            title = humanizeIdentifier(item.name);
        return title;
    }

    QString buttonTitle(const v8reader::FormItem &item) const
    {
        if (!item.synonym.isEmpty() && !looksTechnical(item.synonym))
            return item.synonym;
        const QString command = m_commandTitles.value(item.name);
        if (!command.isEmpty())
            return command;
        return humanizeCommand(item.name);
    }

    // Заголовок группы-секции: «Расходы сотрудника», «Цены» и т. п.
    QString groupTitle(const v8reader::FormItem &item) const
    {
        if (!item.synonym.isEmpty() && !looksTechnical(item.synonym))
            return item.synonym;
        return QString();
    }

    int titlePlacement(const v8reader::FormItem &item) const
    {
        switch (item.titleLocation) {
        case v8reader::FormTitleLocation::Top:
            return 1;
        case v8reader::FormTitleLocation::Right:
            return 2;
        case v8reader::FormTitleLocation::Bottom:
            return 3;
        default:
            return 0;
        }
    }

    bool stackedTitle(const v8reader::FormItem &item) const
    {
        return item.type == v8reader::FormItem::Type::Field
            && item.showTitle && (titlePlacement(item) == 1 || titlePlacement(item) == 3);
    }

    // Группа рисует заголовок, только если задано выделение или особое
    // поведение: у обычных групп без выделения заголовка на форме нет.
    bool groupHasHeader(const v8reader::FormItem &item) const
    {
        using Type = v8reader::FormItem::Type;
        if (item.type != Type::Group)
            return false;
        if (item.groupKind == 1 || item.groupKind == 3 || item.groupKind == 6
            || item.groupKind == 9)
            return false;
        if (!item.showTitle || groupTitle(item).isEmpty())
            return false;
        return item.representation != v8reader::FormGroupRepresentation::None
            || item.behavior != v8reader::FormGroupBehavior::Usual;
    }

    double groupHeaderHeight(const v8reader::FormItem &item) const
    {
        if (!groupHasHeader(item))
            return 0.0;
        if (item.behavior == v8reader::FormGroupBehavior::PopUp)
            return FormStyle::buttonHeight + 4.0;
        if (item.representation == v8reader::FormGroupRepresentation::Strong)
            return 30.0;
        return 24.0;
    }

    double labelWidth(const v8reader::FormItem &item) const
    {
        if (!item.showTitle)
            return 0.0;
        // Заголовок сверху/снизу не занимает горизонтальную колонку.
        if (stackedTitle(item))
            return 0.0;
        const QString label = fieldLabel(item);
        if (label.isEmpty())
            return 0.0;
        return std::min(112.0,
            m_metrics.horizontalAdvance(label + QStringLiteral(":")) + 8.0);
    }

    double preferredWidth(const v8reader::FormItem &item) const
    {
        using Type = v8reader::FormItem::Type;
        switch (item.type) {
        case Type::Field: {
            if (item.widthUnits > 0 && item.horizontalStretch != 1)
                return labelWidth(item) + item.widthUnits * 10.0 + 8.0;
            // Ширина по типу реквизита: 8 px на символ плюс поле ввода.
            if (item.valueChars > 0 && item.horizontalStretch == 0)
                return labelWidth(item) + item.valueChars * 8.0 + 26.0;
            return std::max(170.0, labelWidth(item) + 160.0);
        }
        case Type::Button: {
            const ButtonIcon icon = iconForButton(item);
            const bool iconOnly = icon != ButtonIcon::None
                && icon != ButtonIcon::Create && icon != ButtonIcon::CreateBasedOn
                && icon != ButtonIcon::Print;
            if (iconOnly)
                return 30.0;
            // Кнопка по умолчанию рисуется полужирным — ширина считается по
            // полужирным метрикам, иначе подпись обрезается.
            const QString title = buttonTitle(item);
            QFont bold = m_font;
            bold.setBold(true);
            const QFontMetricsF boldMetrics(bold);
            const bool primaryLike = title.startsWith(QStringLiteral("Провести и закрыть"))
                || title == QStringLiteral("Создать");
            // Отступы кнопки 8.3: по 12 px с каждой стороны плюс рамка.
            double width = (primaryLike ? boldMetrics.horizontalAdvance(title)
                                        : m_metrics.horizontalAdvance(title)) + 26.0;
            if (icon != ButtonIcon::None)
                width += 21.0;
            if (item.name == QStringLiteral("Создать"))
                width += 16.0;
            return std::min(250.0, width);
        }
        case Type::Search:
            if (item.name.contains(QStringLiteral("СтрокаПоиска")))
                return 212.0;
            if (item.name.contains(QStringLiteral("УправлениеПоиском")))
                return 28.0;
            return 40.0;
        case Type::Table:
            return 340.0;
        case Type::Decoration:
            return std::max(40.0, m_metrics.horizontalAdvance(item.synonym) + 10.0);
        case Type::Group: {
            if (item.groupKind == 1) {
                const ButtonIcon icon = iconForButton(item);
                const bool iconOnly = icon != ButtonIcon::None
                    && icon != ButtonIcon::Create
                    && icon != ButtonIcon::CreateBasedOn
                    && icon != ButtonIcon::Print;
                if (iconOnly)
                    return 30.0;
                const QString title = item.synonym.isEmpty()
                    ? humanizeCommand(item.name) : item.synonym;
                if (item.name == QStringLiteral("Создать2"))
                    return 95.0;
                if (item.name == QStringLiteral("СоздатьНаОсновании"))
                    return 160.0;
                if (item.name == QStringLiteral("Еще"))
                    return 58.0;
                return std::max(64.0,
                    m_metrics.horizontalAdvance(title) + 26.0);
            }
            if (item.widthUnits > 0)
                return item.widthUnits * 10.0;
            if (isHorizontal(item)) {
                double width = 0.0;
                int count = 0;
                for (const v8reader::FormItem &child : item.children) {
                    if (isServiceItem(child) || isServiceDecoration(child)
                        || child.name.endsWith(QStringLiteral("Использование"))) {
                        continue;
                    }
                    width += preferredWidth(child) + 8.0;
                    ++count;
                }
                return count > 0 ? width : 100.0;
            }
            double width = 160.0;
            for (const v8reader::FormItem &child : item.children) {
                if (isServiceItem(child) || isServiceDecoration(child))
                    continue;
                width = std::max(width, preferredWidth(child));
            }
            return width;
        }
        default:
            return 80.0;
        }
    }

    // Высота командной панели с учётом переноса кнопок на вторую строку:
    // в форме «Варианты отчетов» кнопок больше, чем помещается в ширину.
    double commandBarHeight(const v8reader::FormItem &item, double width) const
    {
        QVector<double> widths;
        for (const v8reader::FormItem &child : item.children) {
            if (isServiceItem(child))
                continue;
            widths.append(std::max(30.0, preferredWidth(child)));
        }
        if (widths.isEmpty())
            return FormStyle::buttonHeight + 6.0;
        const QVector<QVector<int>> rows = controls::commandBarRows(widths, width, true);
        const int rowCount = static_cast<int>(std::max<qsizetype>(1, rows.size()));
        return rowCount * (FormStyle::buttonHeight + 6.0);
    }

    bool isTopCommandBar(const v8reader::FormItem &item) const
    {
        if (item.type != v8reader::FormItem::Type::Group)
            return false;
        if (item.groupKind == 0
            && (item.name.contains(QStringLiteral("КоманднаяПанель"))
                || item.synonym == QStringLiteral("Командная панель"))) {
            return true;
        }
        // В форме документа командная панель — группа вида 6 с именем
        // «ГруппаКнопкиКоманднойПанели»; внешняя группа вида 9 не в счёт.
        if (item.groupKind == 6
            && (item.name.contains(QStringLiteral("КоманднойПанели"))
                || item.synonym.contains(QStringLiteral("командн"),
                                         Qt::CaseInsensitive))) {
            return true;
        }
        // Формы элементов/справочников могут не иметь отдельной группы
        // кнопок: командная панель — внешняя группа вида 9.
        if (item.groupKind == 9
            && item.name.contains(QStringLiteral("ФормаКоманднаяПанель"))) {
            for (const v8reader::FormItem &child : item.children) {
                if (child.type != v8reader::FormItem::Type::Group)
                    continue;
                if ((child.groupKind == 0 || child.groupKind == 6)
                    && (child.name.contains(QStringLiteral("Командн"))
                        || child.synonym.contains(QStringLiteral("командн"),
                                                  Qt::CaseInsensitive))) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    bool isCommandBar(const v8reader::FormItem &item) const
    {
        if (item.type != v8reader::FormItem::Type::Group)
            return false;
        if (item.groupKind == 9 || item.groupKind == 6)
            return true;
        return item.groupKind == 0
            && (item.name.contains(QStringLiteral("КоманднаяПанель"))
                || item.synonym == QStringLiteral("Командная панель"));
    }

    bool isHorizontal(const v8reader::FormItem &item) const
    {
        if (item.type != v8reader::FormItem::Type::Group)
            return false;
        if (item.groupKind == 9 || item.groupKind == 6 || item.orientation == 1)
            return true;
        if (item.groupKind == 0
            && (item.name.contains(QStringLiteral("КоманднаяПанель"))
                || item.synonym == QStringLiteral("Командная панель"))) {
            return true;
        }
        return false;
    }

    bool isPages(const v8reader::FormItem &item) const
    {
        return item.type == v8reader::FormItem::Type::Group && item.groupKind == 3;
    }

    double naturalHeight(const v8reader::FormItem &item, double width) const
    {
        using Type = v8reader::FormItem::Type;
        switch (item.type) {
        case Type::Field:
        case Type::Button:
        case Type::Search:
            if (item.type == Type::Field && stackedTitle(item))
                return 42.0;
            return 27.0;
        case Type::Decoration:
            if (item.synonym.isEmpty())
                return 0.0;
            return item.heightUnits > 0 ? item.heightUnits * 10.0 : 19.0;
        case Type::Table:
            // В форме списка таблица занимает основную часть формы.
            return m_document.runtimeListCommands ? 492.0 : 360.0;
        case Type::Group: {
            if (item.groupKind == 1)
                return 27.0;
            if (isPages(item))
                return 26.0 + pageContentHeight(item, width);
            const double header = groupHeaderHeight(item);
            QVector<const v8reader::FormItem *> children;
            visibleChildren(item, &children);
            if (children.isEmpty())
                return header;
            if (isHorizontal(item))
                return header + horizontalHeight(children, width, isCommandBar(item));
            double height = 0.0;
            for (const v8reader::FormItem *child : children) {
                if (child->name.endsWith(QStringLiteral("Использование")))
                    continue;
                height += naturalHeight(*child, width) + 5.0;
            }
            return header + height;
        }
        default:
            return 0.0;
        }
    }

    double horizontalHeight(const QVector<const v8reader::FormItem *> &items,
                            double width, bool allowWrap) const
    {
        QVector<const v8reader::FormItem *> visible;
        QVector<double> widths;
        for (const v8reader::FormItem *item : items) {
            if (item->name.endsWith(QStringLiteral("Использование")))
                continue;
            visible.append(item);
            widths.append(std::max(34.0, preferredWidth(*item)));
        }
        if (visible.isEmpty())
            return 0.0;

        if (!allowWrap) {
            double total = 0.0;
            for (double w : widths)
                total += w;
            total += 8.0 * (visible.size() - 1);
            const double scale = total > width && total > 0.0
                ? (width - 8.0 * (visible.size() - 1)) / total : 1.0;
            double height = 0.0;
            for (int i = 0; i < visible.size(); ++i)
                height = std::max(height,
                    naturalHeight(*visible.at(i), widths.at(i) * scale));
            return height;
        }

        double x = 0.0;
        double rowHeight = 0.0;
        double total = 0.0;
        for (int i = 0; i < visible.size(); ++i) {
            const double w = widths.at(i);
            if (x > 0.0 && x + w > width) {
                total += rowHeight + 5.0;
                x = 0.0;
                rowHeight = 0.0;
            }
            rowHeight = std::max(rowHeight, naturalHeight(*visible.at(i), w));
            x += w + 8.0;
        }
        return total + rowHeight;
    }

    double pageContentHeight(const v8reader::FormItem &pages, double width) const
    {
        for (const v8reader::FormItem &child : pages.children) {
            if (child.type != v8reader::FormItem::Type::Group)
                continue;
            QVector<const v8reader::FormItem *> pageChildren;
            visibleChildren(child, &pageChildren);
            if (pageChildren.isEmpty())
                continue;
            double height = 0.0;
            for (const v8reader::FormItem *pageChild : pageChildren)
                height += naturalHeight(*pageChild, width) + 5.0;
            return height;
        }
        return 40.0;
    }

    void visibleChildren(const v8reader::FormItem &item,
                         QVector<const v8reader::FormItem *> *children) const
    {
        for (const v8reader::FormItem &child : item.children) {
            if (!isServiceItem(child) && !isServiceDecoration(child))
                children->append(&child);
        }
    }

    QHash<QString, QString> usageLabels(const QVector<const v8reader::FormItem *> &items) const
    {
        QHash<QString, QString> result;
        for (const v8reader::FormItem *item : items) {
            if (item->type != v8reader::FormItem::Type::Field)
                continue;
            if (!item->name.endsWith(QStringLiteral("Использование")))
                continue;
            result.insert(item->name, fieldLabel(*item));
        }
        return result;
    }

    double layoutVertical(const QVector<const v8reader::FormItem *> &items,
                          const QRectF &rect, int depth)
    {
        const QHash<QString, QString> usage = usageLabels(items);

        // В конфигураторе подписи полей вертикальной группы выровнены по
        // общей колонке: значения начинаются на одной вертикали.
        double sharedLabel = 0.0;
        for (const v8reader::FormItem *item : items) {
            if (item->type != v8reader::FormItem::Type::Field)
                continue;
            if (isServiceItem(*item) || isServiceDecoration(*item) || !item->showTitle)
                continue;
            if (titlePlacement(*item) != 0)
                continue;
            if (item->name.endsWith(QStringLiteral("Использование")))
                continue;
            const QString label = fieldLabel(*item);
            if (label.isEmpty())
                continue;
            sharedLabel = std::max(sharedLabel,
                std::min(112.0,
                    m_metrics.horizontalAdvance(label + QStringLiteral(":")) + 8.0));
        }

        double y = rect.top();
        for (const v8reader::FormItem *item : items) {
            if (item->name.endsWith(QStringLiteral("Использование")))
                continue;
            double height = naturalHeight(*item, rect.width());
            if (isTopCommandBar(*item))
                height = commandBarHeight(*item, rect.width());
            if (height <= 0.0)
                continue;
            if (item->type == v8reader::FormItem::Type::Field) {
                const QString usageName = item->name + QStringLiteral("Использование");
                const QString usageLabel = usage.value(usageName);
                const double fieldHeight = naturalHeight(*item, rect.width());
                layoutField(*item, QRectF(rect.left(), y, rect.width(), fieldHeight),
                            usageLabel.isEmpty() ? sharedLabel : 0.0,
                            usageLabel, !usageLabel.isEmpty(), !usageLabel.isEmpty());
                y += fieldHeight + 5.0;
            } else {
                layoutNode(*item, QRectF(rect.left(), y, rect.width(), height), depth + 1);
                y += height + 5.0;
            }
        }
        return y - rect.top();
    }

    bool stretchable(const v8reader::FormItem &item) const
    {
        using Type = v8reader::FormItem::Type;
        if (isTopCommandBar(item))
            return true;
        if (item.horizontalStretch == 0)
            return false;
        if (item.widthUnits > 0 && item.horizontalStretch != 1)
            return false;
        if (item.type == Type::Field || item.type == Type::Search)
            return true;
        if (item.type != Type::Group)
            return false;
        return item.groupKind != 1 && item.groupKind != 6 && item.groupKind != 9;
    }

    double layoutHorizontal(const QVector<const v8reader::FormItem *> &items,
                            const QRectF &rect, int depth, bool allowWrap,
                            bool runtimeCommands = false)
    {
        QVector<const v8reader::FormItem *> visible;
        for (const v8reader::FormItem *item : items) {
            if (item->name.endsWith(QStringLiteral("Использование")))
                continue;
            visible.append(item);
        }
        if (visible.isEmpty())
            return 0.0;

        // Кнопки с LocationInCommandBar «только в подменю» платформа
        // на панель не выводит — они живут в «Еще».
        if (allowWrap) {
            QVector<const v8reader::FormItem *> onBar;
            for (const v8reader::FormItem *item : visible) {
                if (item->type == v8reader::FormItem::Type::Button
                    && !item->onCommandBar) {
                    continue;
                }
                onBar.append(item);
            }
            if (!onBar.isEmpty())
                visible = onBar;
        }

        // Внутри командной панели подменю (Печать, Заполнить, Отчеты, ЭДО)
        // 1С в панель не выводит — они живут в «Еще».
        if (allowWrap && !runtimeCommands) {
            QVector<const v8reader::FormItem *> filtered;
            for (const v8reader::FormItem *item : visible) {
                if (item->type == v8reader::FormItem::Type::Group
                    && item->groupKind == 1) {
                    continue;
                }
                filtered.append(item);
            }
            if (!filtered.isEmpty())
                visible = filtered;
        }

        // 1С не показывает подменю отдельной кнопкой: если рядом есть кнопка
        // с тем же именем («ПодменюСоздать» и «Создать»), подменю становится
        // выпадающим списком этой кнопки.
        QSet<const v8reader::FormItem *> mergedMenus;
        QSet<const v8reader::FormItem *> dropDownButtons;
        for (const v8reader::FormItem *menu : visible) {
            if (menu->type != v8reader::FormItem::Type::Group
                || menu->groupKind != 1) {
                continue;
            }
            QString base = menu->name;
            const QString prefix = QStringLiteral("Подменю");
            if (base.startsWith(prefix) && base.size() > prefix.size())
                base = base.mid(prefix.size());
            if (base.isEmpty())
                continue;
            for (const v8reader::FormItem *button : visible) {
                if (button->type != v8reader::FormItem::Type::Button)
                    continue;
                if (button->name != base && button->synonym != menu->synonym)
                    continue;
                if (mergedMenus.contains(menu))
                    continue;
                mergedMenus.insert(menu);
                dropDownButtons.insert(button);
            }
        }
        if (!mergedMenus.isEmpty()) {
            QVector<const v8reader::FormItem *> filtered;
            for (const v8reader::FormItem *item : visible) {
                if (!mergedMenus.contains(item))
                    filtered.append(item);
            }
            if (filtered.isEmpty())
                return 0.0;
            visible = filtered;
        }

        const QHash<QString, QString> usage = usageLabels(items);

        // 1С добавляет в форму команды, которых нет в модели:
        // «Создать на основании» для формы списка и документа,
        // «Еще» и «?» — для любой формы.
        const bool listBar = runtimeCommands && m_document.runtimeListCommands;
        const bool documentBar = runtimeCommands && m_document.runtimeDocumentCommands;
        v8reader::FormItem createBasedOn;
        v8reader::FormItem more;
        v8reader::FormItem help;
        if (runtimeCommands) {
            if (listBar || documentBar) {
                createBasedOn.type = v8reader::FormItem::Type::Group;
                createBasedOn.groupKind = 1;
                createBasedOn.name = QStringLiteral("СоздатьНаОсновании");
                createBasedOn.synonym = QStringLiteral("Создать на основании");
            }
            more.type = v8reader::FormItem::Type::Group;
            more.groupKind = 1;
            more.name = QStringLiteral("Еще");
            more.synonym = QStringLiteral("Еще");
            help.type = v8reader::FormItem::Type::Button;
            help.typeCode = 31;
            help.name = QStringLiteral("Справка");
            help.synonym = QStringLiteral("?");

            if (listBar || documentBar) {
                int insertAt = 0;
                for (int i = 0; i < visible.size(); ++i) {
                    if (visible.at(i)->name == QStringLiteral("Создать")) {
                        insertAt = i + 1;
                        break;
                    }
                }
                // «Создать на основании» в 1С стоит после кнопок важных команд.
                for (int i = insertAt; i < visible.size(); ++i) {
                    const v8reader::FormItem *item = visible.at(i);
                    if (item->type == v8reader::FormItem::Type::Group
                        && item->groupKind == 6) {
                        insertAt = i + 1;
                        break;
                    }
                }
                visible.insert(insertAt, &createBasedOn);
            }
            visible.append(&more);
            // Если в форме уже есть кнопка «Справка» (?), вторую не добавляем.
            bool hasHelp = false;
            for (const v8reader::FormItem *item : visible) {
                if (item->type == v8reader::FormItem::Type::Button
                    && (item->name.contains(QStringLiteral("Справка"))
                        || item->synonym == QStringLiteral("?"))) {
                    hasHelp = true;
                    break;
                }
            }
            if (!hasHelp)
                visible.append(&help);
        }

        // В форме документа подменю (Печать, Заполнить, Отчеты, ЭДО) и новости
        // платформа прячет в «Еще»: на панели остаются только кнопки.
        if (documentBar) {
            QVector<const v8reader::FormItem *> filtered;
            for (const v8reader::FormItem *item : visible) {
                const bool submenu = item->type == v8reader::FormItem::Type::Group
                    && item->groupKind == 1
                    && item->name != QStringLiteral("СоздатьНаОсновании")
                    && item->name != QStringLiteral("Еще");
                if (submenu)
                    continue;
                if (item->name.contains(QStringLiteral("КонтекстныеНовости")))
                    continue;
                filtered.append(item);
            }
            visible = filtered;
        }

        // В конфигураторе у формы списка своя панель: две «Создать»,
        // группа служебных иконок и «Создать на основании». Повторяем её.
        v8reader::FormItem createPrimary;
        v8reader::FormItem create2;
        v8reader::FormItem copyButton;
        v8reader::FormItem atKt;
        v8reader::FormItem listSettings;
        v8reader::FormItem paperclip;
        v8reader::FormItem listDoc;
        v8reader::FormItem pencil;
        if (listBar) {
            const v8reader::FormItem *create = nullptr;
            const v8reader::FormItem *search = nullptr;
            const v8reader::FormItem *searchOptions = nullptr;
            for (const v8reader::FormItem *item : visible) {
                if (item->name == QStringLiteral("Создать"))
                    create = item;
                else if (item->name.contains(QStringLiteral("СтрокаПоиска")))
                    search = item;
                else if (item->name.contains(QStringLiteral("УправлениеПоиском")))
                    searchOptions = item;
            }
            if (create && search && searchOptions) {
                createPrimary.type = v8reader::FormItem::Type::Button;
                createPrimary.typeCode = 31;
                createPrimary.name = QStringLiteral("Создать");
                createPrimary.synonym = QStringLiteral("Создать");

                create2.type = v8reader::FormItem::Type::Group;
                create2.groupKind = 1;
                create2.name = QStringLiteral("Создать2");
                create2.synonym = QStringLiteral("Создать");

                copyButton.type = v8reader::FormItem::Type::Button;
                copyButton.typeCode = 31;
                copyButton.name = QStringLiteral("Скопировать");

                atKt.type = v8reader::FormItem::Type::Button;
                atKt.typeCode = 31;
                atKt.name = QStringLiteral("АтКт");

                listSettings.type = v8reader::FormItem::Type::Button;
                listSettings.typeCode = 31;
                listSettings.name = QStringLiteral("НастроитьСписок");

                paperclip.type = v8reader::FormItem::Type::Button;
                paperclip.typeCode = 31;
                paperclip.name = QStringLiteral("ПрисоединенныеФайлы");

                listDoc.type = v8reader::FormItem::Type::Button;
                listDoc.typeCode = 31;
                listDoc.name = QStringLiteral("Просмотреть");

                pencil.type = v8reader::FormItem::Type::Button;
                pencil.typeCode = 31;
                pencil.name = QStringLiteral("Изменить");

                visible = {&createPrimary, &create2, &copyButton, &atKt,
                           &createBasedOn, &listSettings, &paperclip,
                           &listDoc, &pencil, search, searchOptions,
                           &more, &help};
                dropDownButtons.insert(&createPrimary);
            }
        }

        auto estimatedWidth = [&](const v8reader::FormItem *item) {
            double width = preferredWidth(*item);
            const bool searchOptions = item->type == v8reader::FormItem::Type::Search
                && item->name.contains(QStringLiteral("УправлениеПоиском"));
            if (!searchOptions)
                width = std::max(30.0, width);
            if (dropDownButtons.contains(item))
                width += 12.0;
            return width;
        };
        auto totalWidth = [&](const QVector<const v8reader::FormItem *> &items) {
            double total = 0.0;
            for (const v8reader::FormItem *item : items)
                total += estimatedWidth(item);
            if (items.size() > 1)
                total += 8.0 * (items.size() - 1);
            return total;
        };

        // Как и 1С, при нехватке ширины сворачиваем глобальные команды и
        // служебные кнопки в «Еще», вместо переноса панели на вторую строку.
        if (runtimeCommands && totalWidth(visible) > rect.width()) {
            QVector<const v8reader::FormItem *> filtered;
            for (const v8reader::FormItem *item : visible) {
                const bool globalGroup = item->type == v8reader::FormItem::Type::Group
                    && item->groupKind == 6
                    && item->name.contains(QStringLiteral("ГлобальныеКоманды"));
                const bool minorButton = item->type == v8reader::FormItem::Type::Button
                    && (item->name.contains(QStringLiteral("КонтекстныеНовости"))
                        || item->name.contains(QStringLiteral("ОткрытьНовоеОкно")));
                if (globalGroup || minorButton)
                    continue;
                filtered.append(item);
            }
            if (!filtered.isEmpty())
                visible = filtered;
        }

        // На увеличенном масштабе панель не переносится: второстепенные
        // команды убираются, как в 1С, чтобы поиск и «Еще» оставались видны.
        if (runtimeCommands) {
            const QStringList droppable = {
                QStringLiteral("НастроитьСписок"),
                QStringLiteral("ПрисоединенныеФайлы"),
                QStringLiteral("Просмотреть"),
                QStringLiteral("Изменить"),
                QStringLiteral("Скопировать"),
                QStringLiteral("АтКт"),
                QStringLiteral("СоздатьНаОсновании")
            };
            for (const QString &name : droppable) {
                if (totalWidth(visible) <= rect.width())
                    break;
                for (int i = 0; i < visible.size(); ++i) {
                    if (visible.at(i)->name == name) {
                        visible.removeAt(i);
                        break;
                    }
                }
            }
        }

        // Кнопка по умолчанию в 1С подсвечена жёлтым. Она выбирается после
        // раскладки: «Создать» для формы списка и «Провести и закрыть»
        // для формы документа.

        QVector<double> widths;
        for (int i = 0; i < visible.size(); ++i)
            widths.append(estimatedWidth(visible.at(i)));

        // В командной панели 1С разделяет группы команд вертикальными линиями.
        QVector<bool> separatorBefore(visible.size(), false);
        for (int i = 1; i < visible.size(); ++i) {
            const v8reader::FormItem *item = visible.at(i);
            if (item->type == v8reader::FormItem::Type::Group
                && item->groupKind == 6) {
                separatorBefore[i] = true;
            } else if (item->type == v8reader::FormItem::Type::Search
                       && item->name.contains(QStringLiteral("СтрокаПоиска"))) {
                separatorBefore[i] = true;
            }
        }

        const QVector<QVector<int>> rows =
            controls::commandBarRows(widths, rect.width(), allowWrap);

        double y = rect.top();
        for (const QVector<int> &row : rows) {
            QVector<double> rowWidths;
            for (int index : row)
                rowWidths.append(widths.at(index));
            const double total = controls::commandBarTotal(rowWidths);

            // Если не помещается и перенос не разрешён — сжимаем пропорционально.
            if (!allowWrap && total > rect.width()) {
                const double available = rect.width() - 8.0 * (row.size() - 1);
                const double scale = available / (total - 8.0 * (row.size() - 1));
                for (int i = 0; i < rowWidths.size(); ++i) {
                    const v8reader::FormItem *item = visible.at(row.at(i));
                    // Баннер не сжимаем: иначе текст на плашке обрезается.
                    if (item->name.contains(QStringLiteral("ТекстБаннера"))
                        || item->name.contains(QStringLiteral("УчетБилетов"))) {
                        continue;
                    }
                    rowWidths[i] = std::max(34.0, rowWidths.at(i) * scale);
                }
            } else if (total < rect.width()) {
                int count = 0;
                for (int index : row) {
                    if (stretchable(*visible.at(index)))
                        ++count;
                }
                if (count > 0) {
                    const double extra = (rect.width() - total) / count;
                    for (int i = 0; i < row.size(); ++i) {
                        if (stretchable(*visible.at(row.at(i))))
                            rowWidths[i] += extra;
                    }
                }
            }

            double rowHeight = 0.0;
            for (int i = 0; i < row.size(); ++i)
                rowHeight = std::max(rowHeight,
                    naturalHeight(*visible.at(row.at(i)), rowWidths.at(i)));

            // Две строки быстрого отбора делят ширину поровну, как в
            // конфигураторе; иначе первое поле съедает почти всю строку.
            if (row.size() == 2) {
                bool bothFilters = true;
                for (int index : row) {
                    const v8reader::FormItem *item = visible.at(index);
                    const QString usageName = item->name + QStringLiteral("Использование");
                    if (item->type != v8reader::FormItem::Type::Field
                        || usage.value(usageName).isEmpty()) {
                        bothFilters = false;
                        break;
                    }
                }
                if (bothFilters) {
                    const double each = (rect.width() - 8.0) / 2.0;
                    rowWidths[0] = each;
                    rowWidths[1] = each;
                }
            }

            // Поиск, «Еще» и «?» в 1С прижаты к правому краю командной панели.
            int rightStart = -1;
            if (runtimeCommands) {
                for (int c = 0; c < row.size(); ++c) {
                    const v8reader::FormItem *item = visible.at(row.at(c));
                    const bool search = item->type == v8reader::FormItem::Type::Search
                        && item->name.contains(QStringLiteral("СтрокаПоиска"));
                    if (search || item->name == QStringLiteral("Еще")
                        || item->name == QStringLiteral("Справка")) {
                        rightStart = c;
                        break;
                    }
                }
            }
            const double rightShift =
                controls::commandBarRightShift(rowWidths, rect.width(), rightStart);

            double x = rect.left();
            for (int c = 0; c < row.size(); ++c) {
                const int index = row.at(c);
                const v8reader::FormItem *item = visible.at(index);
                if (c == rightStart)
                    x += rightShift;
                // В 1С «Создать на основании» отделено от кнопок заметным отступом.
                if (item == &createBasedOn)
                    x += 30.0;
                if (separatorBefore.at(index) && c > 0) {
                    PreviewControl line;
                    line.kind = PreviewControl::Kind::Separator;
                    line.rect = QRectF(x - 5.0, y + 4.0, 1.0,
                                       std::max(12.0, rowHeight - 8.0));
                    m_controls->append(line);
                }
                const QString usageName = item->name + QStringLiteral("Использование");
                const QString usageLabel = usage.value(usageName);
                if (item->type == v8reader::FormItem::Type::Field) {
                    // Поля с заголовком сверху выше остальных: в строке их
                    // поля ввода выравниваются по нижнему краю, как в 1С.
                    const double fieldHeight = naturalHeight(*item, rowWidths.at(c));
                    const double fieldTop = y + std::max(0.0, rowHeight - fieldHeight);
                    layoutField(*item, QRectF(x, fieldTop, rowWidths.at(c), fieldHeight),
                                0.0, usageLabel, !usageLabel.isEmpty(),
                                !usageLabel.isEmpty());
                } else {
                    layoutNode(*item, QRectF(x, y, rowWidths.at(c), rowHeight), depth + 1,
                               QString(), false, false,
                               dropDownButtons.contains(item));
                }
                x += rowWidths.at(c) + 8.0;
            }
            y += rowHeight + 5.0;
        }
        return y - rect.top();
    }

    void layoutNode(const v8reader::FormItem &item, const QRectF &rect, int depth,
                    const QString &usageLabel = QString(), bool usageCheck = false,
                    bool usageDropDown = false, bool buttonDropDown = false)
    {
        using Type = v8reader::FormItem::Type;
        if (depth > 40)
            return;
        const double buttonY = rect.top() + std::max(0.0,
            (rect.height() - FormStyle::buttonHeight) / 2.0);

        switch (item.type) {
        case Type::Field:
            layoutField(item, rect, 0.0, usageLabel, usageCheck, usageDropDown);
            return;
        case Type::Button:
            layoutButton(item, QRectF(rect.left(), buttonY,
                                      std::min(rect.width(), preferredWidth(item)),
                                      FormStyle::buttonHeight), buttonDropDown);
            return;
        case Type::Search:
            if (item.name.contains(QStringLiteral("СтрокаПоиска"))) {
                PreviewControl control;
                control.kind = PreviewControl::Kind::Search;
                control.rect = QRectF(rect.left(), buttonY,
                                      std::min(rect.width(), 220.0),
                                      FormStyle::buttonHeight);
                control.title = QStringLiteral("Поиск (Ctrl+F)");
                m_controls->append(control);
            } else if (item.name.contains(QStringLiteral("УправлениеПоиском"))) {
                PreviewControl control;
                control.kind = PreviewControl::Kind::SearchOptions;
                control.rect = QRectF(rect.left(), buttonY,
                                      std::min(rect.width(), 30.0),
                                      FormStyle::buttonHeight);
                control.bordered = true;
                m_controls->append(control);
            }
            return;
        case Type::Table:
            layoutTable(item, rect);
            return;
        case Type::Decoration:
            if (!item.synonym.isEmpty()) {
                PreviewControl control;
                control.kind = PreviewControl::Kind::Decoration;
                const double height = item.heightUnits > 0 ? item.heightUnits * 10.0 : 19.0;
                control.rect = QRectF(rect.left(), rect.top(), rect.width(), height);
                control.title = item.synonym;
                control.banner = item.name.contains(QStringLiteral("ТекстБаннера"));
                control.closeGlyph = item.synonym == QStringLiteral("Х");
                m_controls->append(control);
            }
            return;
        case Type::Group: {
            // Внешняя группа командной панели (вид 9) в 1С содержит внутреннюю
            // панель кнопок и служебные кнопки, которые платформа прячет в «Еще».
            if (item.groupKind == 9) {
                for (const v8reader::FormItem &child : item.children) {
                    if (child.type == v8reader::FormItem::Type::Group
                        && isTopCommandBar(child)) {
                        layoutNode(child, rect, depth + 1);
                        return;
                    }
                }
            }
            if (item.groupKind == 1) {
                layoutButton(item, QRectF(rect.left(), buttonY,
                                          std::min(rect.width(), preferredWidth(item)),
                                          FormStyle::buttonHeight), true);
                return;
            }
            if (isPages(item)) {
                layoutPages(item, rect);
                return;
            }
            QVector<const v8reader::FormItem *> children;
            visibleChildren(item, &children);
            const double header = groupHeaderHeight(item);
            if (header > 0.0) {
                PreviewControl title;
                title.kind = PreviewControl::Kind::GroupTitle;
                title.rect = QRectF(rect.left(), rect.top(), rect.width(), header);
                title.title = groupTitle(item);
                title.groupRepresentation = static_cast<int>(item.representation);
                title.groupCollapsible = item.behavior == v8reader::FormGroupBehavior::Collapsible;
                title.groupPopup = item.behavior == v8reader::FormGroupBehavior::PopUp;
                m_controls->append(title);
            }
            if (children.isEmpty())
                return;
            const QRectF inner = rect.adjusted(0.0, header, 0.0, 0.0);
            if (isHorizontal(item))
                layoutHorizontal(children, inner, depth, isCommandBar(item),
                                 isTopCommandBar(item));
            else
                layoutVertical(children, inner, depth);
            return;
        }
        default:
            return;
        }
    }

    void layoutField(const v8reader::FormItem &item, const QRectF &rect,
                     double forcedLabelWidth, const QString &labelOverride = QString(),
                     bool checkBox = false, bool dropDown = false)
    {
        const QString label = !labelOverride.isEmpty()
            ? labelOverride
            : (item.showTitle ? fieldLabel(item) : QString());

        bool booleanField = item.valueKind == v8reader::FormValueKind::Boolean;
        if (!checkBox && !booleanField) {
            for (const v8reader::FormAttribute &attribute : m_document.attributes) {
                if (attribute.name == item.name
                    && attribute.typeHint == QStringLiteral("Булево")) {
                    booleanField = true;
                    break;
                }
            }
        }

        // Положение заголовка из формы: 1 — сверху, 2 — справа, 3 — снизу.
        // У флажков и полей-надписей подпись всегда стоит рядом.
        const int placement = (checkBox || booleanField || item.labelOnly)
            ? 0 : titlePlacement(item);
        double labelW = forcedLabelWidth;
        if (placement == 1 || placement == 3) {
            labelW = 0.0;
        } else if (placement == 2) {
            // Подпись справа — своя ширина, общая колонка подписей не нужна.
            labelW = std::min(112.0,
                m_metrics.horizontalAdvance(label + QStringLiteral(":")) + 8.0);
        } else if (labelW <= 0.0) {
            labelW = std::min(112.0,
                m_metrics.horizontalAdvance(label + QStringLiteral(":")) + 8.0);
        }
        labelW = std::min(labelW, rect.width() * 0.7);

        // Тип реквизита из метаданных: ссылка — кнопка выбора, дата —
        // календарь, булево — флажок. Неизвестный тип остаётся на прежних
        // эвристиках по имени поля.
        const bool knownKind = item.valueKind != v8reader::FormValueKind::Unknown;
        const bool referenceField = item.valueKind == v8reader::FormValueKind::Reference
            || item.valueKind == v8reader::FormValueKind::Enum
            || item.valueKind == v8reader::FormValueKind::Composite;

        PreviewControl control;
        control.kind = PreviewControl::Kind::Field;
        control.rect = rect;
        control.title = label;
        control.labelWidth = labelW;
        control.labelPlacement = placement;
        control.checkBox = checkBox || booleanField;
        control.checkboxOnly = booleanField;
        control.checkBoxLeading = booleanField;
        control.dropDown = dropDown;
        control.labelOnly = item.labelOnly && !booleanField;
        control.calendar = !item.labelOnly
            && (knownKind
                    ? item.valueKind == v8reader::FormValueKind::Date
                    : (item.name.contains(QStringLiteral("Дата"))
                       || item.name.contains(QStringLiteral("Период"))));
        control.choiceButton = !item.labelOnly && !control.calendar && !dropDown
            && !booleanField
            && (knownKind ? referenceField : looksLikeReferenceField(item.name));
        if (item.widthUnits > 0 && item.horizontalStretch != 1)
            control.inputWidth = item.widthUnits * 10.0;
        else if (!knownKind && checkBox && m_document.runtimeListCommands)
            control.inputWidth = 325.0;
        else if (item.valueChars > 0 && item.horizontalStretch == 0)
            control.inputWidth = item.valueChars * 8.0 + 18.0;
        m_controls->append(control);
    }

    void layoutButton(const v8reader::FormItem &item, const QRectF &rect, bool dropDown)
    {
        PreviewControl control;
        control.kind = PreviewControl::Kind::Button;
        control.rect = rect;
        control.title = buttonTitle(item);
        control.icon = iconForButton(item);
        control.iconOnly = control.icon != ButtonIcon::None
            && control.icon != ButtonIcon::Create
            && control.icon != ButtonIcon::CreateBasedOn
            && control.icon != ButtonIcon::Print;
        control.dropDown = dropDown;
        // Кнопка «Справка» в 1С — обычная кнопка с «?», а не иконка.
        if (item.name == QStringLiteral("Справка")) {
            control.icon = ButtonIcon::None;
            control.iconOnly = false;
            control.title = QStringLiteral("?");
            control.bordered = true;
        }
        control.bordered = item.name == QStringLiteral("Еще")
            || item.name == QStringLiteral("Справка")
            || item.name == QStringLiteral("Создать2")
            || item.name == QStringLiteral("СоздатьНаОсновании")
            || item.name == QStringLiteral("Скопировать")
            || item.name == QStringLiteral("АтКт")
            || item.name == QStringLiteral("НастроитьСписок")
            || item.name == QStringLiteral("ПрисоединенныеФайлы")
            || item.name == QStringLiteral("Просмотреть")
            || item.name == QStringLiteral("Изменить");
        m_controls->append(control);
    }

    void tableColumns(const v8reader::FormItem &item, const QString &tableName,
                      QStringList *columns, QVector<int> *widths,
                      QVector<int> *chars) const
    {
        for (const v8reader::FormItem &child : item.children) {
            if (isServiceItem(child) || isServiceDecoration(child))
                continue;
            if (child.type == v8reader::FormItem::Type::Field) {
                // Скрытые в форме колонки (в т.ч. «Валюта») 1С не показывает.
                if (!child.visible)
                    continue;
                // Служебные колонки, которые платформа не показывает в списке.
                if (child.name == QStringLiteral("ЕстьФайлы")
                    || child.name == QStringLiteral("Ссылка")) {
                    continue;
                }
                QString base = child.name;
                if (!tableName.isEmpty() && base.startsWith(tableName)
                    && base.size() > tableName.size()) {
                    base = base.mid(tableName.size());
                }
                // Заголовок колонки — прежде всего синоним самого поля формы
                // («Аванс», «Зачтено (руб.)»), затем синоним реквизита из
                // метаданных, и только потом имя.
                QString title;
                if (!child.synonym.isEmpty() && !looksTechnical(child.synonym))
                    title = child.synonym;
                if (title.isEmpty())
                    title = child.dataTitle;
                if (title.isEmpty())
                    title = m_document.titles.value(base);
                if (title.isEmpty())
                    title = m_document.titles.value(child.name);
                if (title.isEmpty())
                    title = humanizeIdentifier(base);
                // Колонка-отступ без заголовка: ячейка остаётся пустой.
                if (!child.showTitle)
                    title.clear();
                // В таблицах 1С служебная колонка номера строки называется
                // коротко «N», а не «Номер строки».
                if (title.startsWith(QStringLiteral("Номер строки"),
                                     Qt::CaseInsensitive)) {
                    title = QStringLiteral("N");
                }
                columns->append(title);
                widths->append(child.widthUnits);
                chars->append(child.valueChars);
            } else if (child.type == v8reader::FormItem::Type::Group) {
                tableColumns(child, tableName, columns, widths, chars);
            }
        }
    }

    void layoutTable(const v8reader::FormItem &item, const QRectF &rect)
    {
        PreviewControl control;
        control.kind = PreviewControl::Kind::Table;
        control.actionColumn = m_document.runtimeListCommands
            || m_document.runtimeCatalogListCommands;
        double tableTop = rect.top();
        double tableHeight = m_document.runtimeListCommands ? 492.0 : 360.0;
        if (m_document.runtimeDocumentCommands && !m_document.runtimeListCommands) {
            // Командная панель табличной части документа: «Добавить» и «Еще».
            PreviewControl add;
            add.kind = PreviewControl::Kind::Button;
            add.rect = QRectF(rect.left(), tableTop + 3.0, 74.0,
                              FormStyle::buttonHeight);
            add.title = QStringLiteral("Добавить");
            add.bordered = true;
            m_controls->append(add);

            PreviewControl up;
            up.kind = PreviewControl::Kind::Button;
            up.rect = QRectF(rect.left() + 82.0, tableTop + 3.0, 24.0,
                             FormStyle::buttonHeight);
            up.icon = ButtonIcon::Up;
            up.iconOnly = true;
            up.bordered = true;
            m_controls->append(up);

            PreviewControl down;
            down.kind = PreviewControl::Kind::Button;
            down.rect = QRectF(rect.left() + 110.0, tableTop + 3.0, 24.0,
                               FormStyle::buttonHeight);
            down.icon = ButtonIcon::Down;
            down.iconOnly = true;
            down.bordered = true;
            m_controls->append(down);

            PreviewControl more;
            more.kind = PreviewControl::Kind::Button;
            more.rect = QRectF(rect.right() - 60.0, tableTop + 3.0, 56.0,
                               FormStyle::buttonHeight);
            more.title = QStringLiteral("Еще");
            more.dropDown = true;
            more.bordered = true;
            m_controls->append(more);

            tableTop += 30.0;
            tableHeight = std::max(80.0, tableHeight - 30.0);
        }
        if (m_document.runtimeListCommands)
            tableTop += 1.0;
        control.rect = QRectF(rect.left() - 4.0, tableTop,
                              std::max(1.0, rect.width() - 6.0),
                              std::min(std::max(0.0, rect.height() - (tableTop - rect.top())),
                                       tableHeight));
        QVector<int> widthUnits;
        QVector<int> valueChars;
        tableColumns(item, item.name, &control.columns, &widthUnits, &valueChars);

        if (!control.columns.isEmpty()) {
            QFont headerFont = m_font;
            headerFont.setBold(false);
            QFontMetricsF headerMetrics(headerFont);
            double total = 0.0;
            for (int i = 0; i < control.columns.size(); ++i) {
                const QString &column = control.columns.at(i);
                const int units = i < widthUnits.size() ? widthUnits.at(i) : -1;
                const int chars = i < valueChars.size() ? valueChars.at(i) : 0;
                double width = 0.0;
                if (column.isEmpty()) {
                    width = 40.0;
                } else if (column == QStringLiteral("N")) {
                    // Служебная колонка номера строки узкая.
                    width = 44.0;
                } else {
                    // Как в конфигураторе: ширина по типу реквизита
                    // (10 px на символ + 17 px на ячейку), но не уже
                    // заголовка колонки.
                    const double dataWidth = chars > 0 ? chars * 10.0 + 17.0 : 0.0;
                    const double headerWidth =
                        headerMetrics.horizontalAdvance(column) + 24.0;
                    width = std::max(90.0, std::max(headerWidth, dataWidth));
                    if (units > 0)
                        width = std::max(width, units * 10.0);
                    width = std::min(width, 340.0);
                }
                control.columnWidths.append(width);
                total += width;
            }
            // Если колонки уже помещаются — растягиваем их на всю таблицу;
            // иначе оставляем ширину по содержимому и показываем прокрутку.
            const double availableWidth = std::max(60.0, control.rect.width());
            if (total < availableWidth) {
                const double extra = (availableWidth - total) / control.columnWidths.size();
                for (double &width : control.columnWidths)
                    width += extra;
            }
        }
        m_controls->append(control);
    }

    void layoutPages(const v8reader::FormItem &pages, const QRectF &rect)
    {
        QVector<const v8reader::FormItem *> pageItems;
        visibleChildren(pages, &pageItems);

        double x = rect.left();
        double selectedTabRight = rect.left();
        for (int i = 0; i < pageItems.size(); ++i) {
            const v8reader::FormItem *page = pageItems.at(i);
            if (page->type != v8reader::FormItem::Type::Group)
                continue;
            PreviewControl tab;
            tab.kind = PreviewControl::Kind::PageTab;
            const QString title = page->synonym.isEmpty()
                ? humanizeCommand(page->name) : page->synonym;
            tab.title = title;
            tab.selectedTab = (i == 0);
            const double width = std::min(200.0,
                m_metrics.horizontalAdvance(title) + 28.0);
            tab.rect = QRectF(x, rect.top(), width, 24.0);
            m_controls->append(tab);
            if (tab.selectedTab)
                selectedTabRight = tab.rect.right();
            x += width + 2.0;
        }
        if (selectedTabRight < rect.right()) {
            PreviewControl line;
            line.kind = PreviewControl::Kind::HorizontalLine;
            line.rect = QRectF(selectedTabRight, rect.top() + 24.0,
                               rect.right() - selectedTabRight, 1.0);
            m_controls->append(line);
        }

        const v8reader::FormItem *firstPage = nullptr;
        for (const v8reader::FormItem *page : pageItems) {
            if (page->type == v8reader::FormItem::Type::Group) {
                firstPage = page;
                break;
            }
        }
        if (!firstPage)
            return;
        QVector<const v8reader::FormItem *> children;
        visibleChildren(*firstPage, &children);
        layoutVertical(children,
                       QRectF(rect.left(), rect.top() + 28.0,
                              rect.width(), naturalHeight(*firstPage, rect.width())),
                       0);
    }

    const v8reader::FormDocument &m_document;
    double m_width;
    QFont m_font;
    QFontMetricsF m_metrics;
    QHash<QString, QString> m_commandTitles;
    QVector<PreviewControl> *m_controls = nullptr;
};

void paintControl(QPainter &painter, const PreviewControl &control,
                  const QFont &font, const QFontMetricsF &metrics)
{
    painter.setFont(font);
    const QRectF rect = control.rect;
    struct ClipGuard {
        QPainter &painter;
        ClipGuard(QPainter &p, const QRectF &r) : painter(p)
        {
            painter.save();
            painter.setClipRect(r);
        }
        ~ClipGuard() { painter.restore(); }
    } clip(painter, rect.adjusted(-1.0, -1.0, 1.0, 1.0));

    switch (control.kind) {
    case PreviewControl::Kind::Field: {
        controls::FieldVisual visual;
        visual.rect = rect;
        visual.label = control.title;
        visual.labelWidth = control.labelWidth;
        visual.inputWidth = control.inputWidth;
        visual.labelPlacement = control.labelPlacement;
        visual.labelOnly = control.labelOnly;
        visual.checkBox = control.checkBox;
        visual.checkboxOnly = control.checkboxOnly;
        visual.checkBoxLeading = control.checkBoxLeading;
        visual.dropDown = control.dropDown;
        visual.calendar = control.calendar;
        visual.choiceButton = control.choiceButton;
        controls::paintField(painter, visual, metrics);
        break;
    }
    case PreviewControl::Kind::Button: {
        controls::ButtonVisual visual;
        visual.rect = rect;
        visual.text = control.title;
        visual.icon = control.icon;
        visual.primary = control.primary;
        visual.bordered = control.bordered;
        visual.iconOnly = control.iconOnly;
        visual.dropDown = control.dropDown;
        controls::paintButton(painter, visual, metrics);
        break;
    }
    case PreviewControl::Kind::Search: {
        controls::paintSearch(painter, controls::SearchVisual{rect, control.title});
        break;
    }
    case PreviewControl::Kind::SearchOptions: {
        controls::paintSearchOptions(painter, rect);
        break;
    }
    case PreviewControl::Kind::Table: {
        controls::TableVisual visual;
        visual.rect = rect;
        visual.columns = control.columns;
        visual.widths = control.columnWidths;
        visual.actionColumn = control.actionColumn;
        controls::paintTable(painter, visual, font, metrics);
        break;
    }
    case PreviewControl::Kind::PageTab: {
        controls::TabVisual visual;
        visual.rect = rect;
        visual.title = control.title;
        visual.selected = control.selectedTab;
        controls::paintTab(painter, visual, metrics);
        break;
    }
    case PreviewControl::Kind::FilterChip: {
        controls::paintFilterChip(painter, controls::ChipVisual{rect, control.title});
        break;
    }
    case PreviewControl::Kind::Decoration: {
        controls::TextVisual visual{rect, control.title};
        visual.banner = control.banner;
        visual.closeGlyph = control.closeGlyph;
        controls::paintText(painter, visual, metrics);
        break;
    }
    case PreviewControl::Kind::GroupTitle: {
        // Подпись секции: обычное выделение — жирный заголовок, сильное —
        // крупный обычный, раскрываемая группа — треугольник, всплывающая —
        // кнопка-заголовок.
        QFont titleFont = font;
        switch (control.groupRepresentation) {
        case 1: // Strong
            titleFont.setPointSizeF(font.pointSizeF() + 3.0);
            break;
        case 3: // Normal
            titleFont.setPointSizeF(font.pointSizeF() + 1.5);
            titleFont.setBold(true);
            break;
        default: // Weak
            titleFont.setPointSizeF(font.pointSizeF() + 0.5);
            titleFont.setBold(true);
            break;
        }
        painter.setFont(titleFont);
        const QFontMetricsF titleMetrics(titleFont);
        if (control.groupPopup) {
            QRectF button(rect.left(), rect.top() + 1.0, rect.width(),
                          std::max(20.0, rect.height() - 4.0));
            QLinearGradient gradient(button.topLeft(), button.bottomLeft());
            gradient.setColorAt(0.0, QColor(252, 252, 252));
            gradient.setColorAt(1.0, QColor(238, 238, 238));
            painter.setPen(FormStyle::fieldBorder);
            painter.setBrush(gradient);
            painter.drawRect(button);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(FormStyle::text);
            painter.drawText(button.adjusted(8.0, 0.0, -18.0, 0.0),
                             Qt::AlignLeft | Qt::AlignVCenter, control.title);
            const QPointF center(button.right() - 10.0, button.center().y());
            painter.setPen(QColor(80, 80, 80));
            painter.drawLine(center + QPointF(-3.0, -1.5),
                             center + QPointF(0.0, 1.5));
            painter.drawLine(center + QPointF(0.0, 1.5),
                             center + QPointF(3.0, -1.5));
            break;
        }
        double textLeft = rect.left();
        if (control.groupCollapsible) {
            const QPointF center(rect.left() + 6.0, rect.center().y());
            QPainterPath arrow;
            arrow.moveTo(center + QPointF(-4.0, -2.5));
            arrow.lineTo(center + QPointF(4.0, -2.5));
            arrow.lineTo(center + QPointF(0.0, 2.5));
            arrow.closeSubpath();
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(80, 80, 80));
            painter.drawPath(arrow);
            painter.setBrush(Qt::NoBrush);
            textLeft += 14.0;
        }
        painter.setPen(FormStyle::text);
        painter.drawText(QRectF(textLeft, rect.top(), rect.right() - textLeft, rect.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, control.title);
        break;
    }
    case PreviewControl::Kind::Separator: {
        controls::paintSeparator(painter, rect);
        break;
    }
    case PreviewControl::Kind::HorizontalLine: {
        controls::paintHorizontalLine(painter, rect);
        break;
    }
    }
}

} // namespace

// Схематичное представление формы, повторяющее базовую раскладку 1С:
// вертикальные/горизонтальные группы, выравнивание подписей, страницы,
// таблицы, командные панели и быстрые отборы.
class FormPreviewWidget : public QWidget
{
public:
    explicit FormPreviewWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("formPreview"));
        setMinimumHeight(260);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setAutoFillBackground(true);
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, Qt::white);
        setPalette(palette);
    }

    void setDocument(const v8reader::FormDocument *document)
    {
        m_document = document;
        rebuild();
    }

    void setScale(double scale)
    {
        m_scale = std::max(0.25, std::min(3.0, scale));
        rebuild();
    }

    void refreshLayout() { rebuild(); }

    void clear()
    {
        m_document = nullptr;
        m_controls.clear();
        m_contentHeight = 0.0;
        setFixedSize(int(kFormWidth * m_scale), 260);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), Qt::white);
        painter.scale(m_scale, m_scale);

        const double logicalW = width() / m_scale;
        const double logicalH = height() / m_scale;
        const double titleHeight = 18.0;

        // Рамка и полоса заголовка окна формы, как у окна конфигуратора.
        QLinearGradient titleGradient(0.0, 0.0, 0.0, titleHeight);
        titleGradient.setColorAt(0.0, QColor(163, 196, 228));
        titleGradient.setColorAt(1.0, QColor(141, 176, 208));
        painter.fillRect(QRectF(0.0, 0.0, logicalW, titleHeight), titleGradient);
        painter.setPen(QColor(255, 255, 255));
        painter.drawLine(QPointF(0.5, 0.5), QPointF(logicalW - 0.5, 0.5));
        painter.drawLine(QPointF(0.5, 0.5), QPointF(0.5, logicalH - 0.5));
        painter.setPen(QColor(0, 16, 38));
        painter.drawRect(QRectF(0.5, 0.5, logicalW - 1.0, logicalH - 1.0));
        painter.setPen(QColor(69, 139, 201));
        painter.drawRect(QRectF(1.5, 1.5, logicalW - 3.0, logicalH - 3.0));
        painter.setPen(QColor(226, 255, 255));
        painter.drawRect(QRectF(2.5, 2.5, logicalW - 5.0, logicalH - 5.0));
        painter.setPen(QColor(120, 150, 180));
        painter.drawLine(QPointF(0.0, titleHeight), QPointF(logicalW, titleHeight));

        // Заголовок окна формы: «Документ.АвансовыйОтчет.ФормаДокумента».
        if (m_document) {
            QString caption = m_document->name;
            if (!m_document->ownerTitle.isEmpty()) {
                caption = QStringLiteral("%1.%2")
                              .arg(m_document->ownerTitle, m_document->name);
            }
            if (!caption.isEmpty()) {
                QFont captionFont(QStringLiteral("Arial"), 8, QFont::Bold);
                painter.setFont(captionFont);
                painter.setPen(QColor(16, 32, 52));
                painter.drawText(QRectF(6.0, 0.0, logicalW - 12.0, titleHeight),
                                 Qt::AlignLeft | Qt::AlignVCenter, caption);
                painter.setFont(formFont());
            }
        }

        // Клиентская область окна 1С — светло-серая (#f2f2f2), как в Такси;
        // поля и таблица рисуются поверх белым.
        painter.fillRect(QRectF(3.0, titleHeight + 1.0, logicalW - 6.0,
                                logicalH - titleHeight - 4.0),
                         FormStyle::formBackground);

        const QFont titleFont(QStringLiteral("Arial"), 14, QFont::Bold);
        const QFont font = formFont();
        const QFontMetricsF metrics(font);

        if (!m_document) {
            painter.setPen(QColor(140, 140, 140));
            painter.setFont(titleFont);
            painter.drawText(rect().adjusted(20, 40, -20, -20),
                             Qt::AlignHCenter | Qt::AlignTop,
                             QStringLiteral("Форма не выбрана"));
            return;
        }

        painter.setFont(font);
        for (const PreviewControl &control : m_controls)
            paintControl(painter, control, font, metrics);

        // Тонкая рамка клиентской области под заголовком.
        painter.setPen(QPen(QColor(150, 160, 170), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(3.0, titleHeight, logicalW - 6.0,
                                logicalH - titleHeight - 3.0));

        // Белая внешняя кромка окна.
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF(0.0, 0.0, logicalW, 2.0));
        painter.drawRect(QRectF(0.0, 0.0, 2.0, logicalH));
        painter.drawRect(QRectF(logicalW - 2.0, 0.0, 2.0, logicalH));
        painter.drawRect(QRectF(0.0, logicalH - 2.0, logicalW, 2.0));
    }

private:
    void rebuild()
    {
        m_controls.clear();
        if (!m_document || m_document->items.isEmpty()) {
            m_contentHeight = 0.0;
            setFixedSize(int(kFormWidth * m_scale), 260);
            update();
            return;
        }

        const double margin = 10.0;
        double viewportWidth = 0.0;
        double viewportHeight = 0.0;
        if (QWidget *viewport = parentWidget()) {
            if (QScrollArea *area = qobject_cast<QScrollArea *>(viewport->parentWidget())) {
                viewportWidth = area->viewport()->width();
                viewportHeight = area->viewport()->height();
            }
        }
        // Ширина формы следует за шириной панели, как окно 1С: при масштабе
        // 75% форма занимает панель целиком, при увеличении — прокручивается.
        const double baseWidth = viewportWidth > 2.0
            ? (viewportWidth - 1.0) / m_scale : kFormWidth;
        const double logicalWidth = std::clamp(baseWidth, 700.0, 1000.0);
        const double contentWidth = logicalWidth - margin * 2.0;

        // Высота окна 1С: таблица списка должна занимать всё доступное место,
        // а не оставлять пустую полосу под собой.
        const double viewportLogical = viewportHeight > 0.0
            ? viewportHeight / m_scale : 0.0;
        const double titleBarHeight = 18.0;
        const double targetContentHeight = std::max(0.0,
            viewportLogical - titleBarHeight - 87.0);

        LayoutEngine engine(*m_document, contentWidth);
        double contentHeight = 0.0;
        QVector<PreviewControl> controls;
        engine.build(&controls, &contentHeight, targetContentHeight);

        // Кнопка по умолчанию (жёлтая) — «Создать» в форме списка и
        // «Провести и закрыть» в форме документа.
        for (PreviewControl &control : controls)
            control.primary = false;
        auto markPrimary = [&controls](const std::function<bool(const PreviewControl &)> &predicate) {
            for (PreviewControl &control : controls) {
                if (control.kind != PreviewControl::Kind::Button || control.iconOnly)
                    continue;
                if (predicate(control)) {
                    control.primary = true;
                    return true;
                }
            }
            return false;
        };
        if (!markPrimary([](const PreviewControl &control) {
                return control.title == QStringLiteral("Создать")
                    || control.title.startsWith(QStringLiteral("Провести и закрыть"));
            })) {
            markPrimary([](const PreviewControl &) { return true; });
        }

        for (PreviewControl &control : controls)
            control.rect.translate(margin, titleBarHeight + 6.0);
        // Отладочный дамп контролов: QTV8READER_DEBUG_FORM=<путь к файлу>.
        const QString debugPath = qEnvironmentVariable("QTV8READER_DEBUG_FORM");
        if (!debugPath.isEmpty()) {
            QFile file(debugPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QTextStream stream(&file);
                for (const PreviewControl &control : controls) {
                    stream << int(control.kind) << '\t' << control.title
                           << '\t' << control.rect.left() << ',' << control.rect.top()
                           << ' ' << control.rect.width() << 'x' << control.rect.height()
                           << "\ticonOnly=" << control.iconOnly
                           << " bordered=" << control.bordered
                           << " icon=" << int(control.icon)
                           << " cb=" << control.checkBox
                           << " cal=" << control.calendar
                           << " choice=" << control.choiceButton
                           << " labelOnly=" << control.labelOnly
                           << " inputW=" << control.inputWidth
                           << " labelW=" << control.labelWidth << '\n';
                }
            }
        }
        m_controls = controls;
        m_contentHeight = contentHeight;
        double logicalHeight = qBound(280.0,
            margin * 2.0 + contentHeight + titleBarHeight, 8000.0);
        if (targetContentHeight > 0.0) {
            logicalHeight = std::max(logicalHeight,
                margin * 2.0 + targetContentHeight + titleBarHeight);
        }
        if (viewportLogical > 0.0 && m_document->runtimeListCommands)
            logicalHeight = std::min(logicalHeight, viewportLogical);
        setFixedSize(int(logicalWidth * m_scale), int(logicalHeight * m_scale));
        update();
    }

    static constexpr double kFormWidth = 880.0;

    const v8reader::FormDocument *m_document = nullptr;
    QVector<PreviewControl> m_controls;
    double m_contentHeight = 0.0;
    double m_scale = 1.0;
};

namespace v8reader {

FormView::FormView(QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("formTabs"));
    outerLayout->addWidget(m_tabs);

    auto *formPage = new QWidget(m_tabs);
    auto *layout = new QVBoxLayout(formPage);
    layout->setContentsMargins(0, 4, 0, 0);

    m_hint = new QLabel(QStringLiteral("Выберите форму в дереве метаданных."), formPage);
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint);

    auto *toolbar = new QHBoxLayout();
    toolbar->addWidget(new QLabel(QStringLiteral("Масштаб:"), formPage));
    m_scaleCombo = new QComboBox(formPage);
    m_scaleCombo->setObjectName(QStringLiteral("formScale"));
    const QVector<int> scales = {50, 75, 100, 125, 150};
    for (int percent : scales)
        m_scaleCombo->addItem(QStringLiteral("%1%").arg(percent), percent);
    m_scaleCombo->setCurrentIndex(2);
    toolbar->addWidget(m_scaleCombo);
    toolbar->addWidget(new QLabel(QStringLiteral("Вид:"), formPage));
    m_modeCombo = new QComboBox(formPage);
    m_modeCombo->setObjectName(QStringLiteral("formViewMode"));
    m_modeCombo->addItem(QStringLiteral("Упрощённый"), false);
    m_modeCombo->addItem(QStringLiteral("Точный (как в 1С)"), true);
    m_modeCombo->setToolTip(QStringLiteral(
        "Упрощённый вид — собственный рендерер; точный вид — рендерер "
        "конфигуратора 1С поверх Microsoft Edge WebView2"));
    toolbar->addWidget(m_modeCombo);
    m_treeToggle = new QToolButton(formPage);
    m_treeToggle->setObjectName(QStringLiteral("formTreeToggle"));
    m_treeToggle->setText(QStringLiteral("Дерево"));
    m_treeToggle->setCheckable(true);
    m_treeToggle->setChecked(false);
    m_treeToggle->setToolTip(QStringLiteral("Показать дерево элементов формы"));
    toolbar->addWidget(m_treeToggle);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    m_splitter = new QSplitter(Qt::Horizontal, formPage);

    m_tree = new QTreeWidget(m_splitter);
    m_tree->setObjectName(QStringLiteral("formTree"));
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({QStringLiteral("Элемент"), QStringLiteral("Тип"),
                             QStringLiteral("Имя"), QStringLiteral("Синоним")});
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_tree->setColumnWidth(2, 220);
    m_tree->setColumnWidth(3, 200);

    auto *scroll = new QScrollArea(m_splitter);
    scroll->setWidgetResizable(false);
    scroll->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_preview = new FormPreviewWidget(scroll);
    scroll->setWidget(m_preview);

    m_views = new QStackedWidget(m_splitter);
    m_views->setObjectName(QStringLiteral("formViews"));
    m_views->addWidget(scroll);

    m_exactPage = new QWidget(m_views);
    auto *exactLayout = new QVBoxLayout(m_exactPage);
    exactLayout->setContentsMargins(0, 0, 0, 0);
    m_exactHost = new WebView2Host(m_exactPage);
    m_exactHost->setObjectName(QStringLiteral("formExactView"));
    m_exactHost->setResourceProvider(
        [this](const QString &path, QByteArray *data, QString *mime) {
            return provideExactResource(path, data, mime);
        });
    m_exactHost->setStartUrl(exactStartUrl());
    connect(m_exactHost, &WebView2Host::loadFailed, this,
            [this](const QString &reason) { showExactViewMessage(reason); });
    exactLayout->addWidget(m_exactHost, 1);
    m_exactMessage = new QLabel(m_exactPage);
    m_exactMessage->setObjectName(QStringLiteral("formExactMessage"));
    m_exactMessage->setWordWrap(true);
    m_exactMessage->setContentsMargins(12, 8, 12, 8);
    m_exactMessage->hide();
    exactLayout->addWidget(m_exactMessage);
    m_views->addWidget(m_exactPage);

    m_splitter->addWidget(m_tree);
    m_splitter->addWidget(m_views);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    // По умолчанию форма занимает всю ширину вкладки, как в конфигураторе;
    // дерево элементов открывается кнопкой «Дерево».
    m_splitter->setSizes({0, 10000});
    connect(m_treeToggle, &QToolButton::toggled, this, [this](bool visible) {
        const QList<int> sizes = m_splitter->sizes();
        const int total = sizes.value(0) + sizes.value(1);
        if (visible)
            m_splitter->setSizes({230, std::max(1, total - 230)});
        else
            m_splitter->setSizes({0, std::max(1, total)});
    });
    QTimer::singleShot(0, this, [this] {
        if (!m_treeToggle->isChecked())
            m_splitter->setSizes({0, std::max(1, m_splitter->width())});
    });
    layout->addWidget(m_splitter, 1);
    m_tabs->addTab(formPage, QStringLiteral("Форма"));

    auto *modulePage = new QWidget(m_tabs);
    auto *moduleLayout = new QVBoxLayout(modulePage);
    moduleLayout->setContentsMargins(0, 4, 0, 0);
    m_moduleEdit = new QPlainTextEdit(modulePage);
    m_moduleEdit->setObjectName(QStringLiteral("formModuleEdit"));
    m_moduleEdit->setReadOnly(true);
    m_moduleEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_moduleEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_moduleEdit->setPlaceholderText(QStringLiteral("Модуль формы пуст."));
    moduleLayout->addWidget(m_moduleEdit, 1);
    m_highlighter = new BslHighlighter(m_moduleEdit->document());
    m_tabs->addTab(modulePage, QStringLiteral("Модуль"));

    connect(m_scaleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                const int percent = m_scaleCombo->itemData(index).toInt();
                if (m_preview)
                    m_preview->setScale(percent / 100.0);
                if (m_exactHost)
                    m_exactHost->setZoomFactor(percent / 100.0);
            });
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                setExactViewEnabled(m_modeCombo->itemData(index).toBool());
                Q_EMIT exactViewModeChanged(m_exactEnabled);
            });

    QString exactReason;
    const bool exactAvailable = WebView2Host::isSupported(&exactReason);
    setExactViewAvailability(exactAvailable, exactReason);

    clear();
}

void FormView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // Размер области просмотра известен только после раскладки: повторяем
    // макет, чтобы таблица списка заняла всю высоту, как в окне 1С.
    if (m_preview) {
        QTimer::singleShot(0, this, [this] {
            if (m_preview)
                m_preview->refreshLayout();
        });
    }
}

void FormView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_preview) {
        QTimer::singleShot(50, this, [this] {
            if (m_preview)
                m_preview->refreshLayout();
        });
    }
}

void FormView::setDocument(const FormDocument &document)
{
    m_document = document;
    m_hasDocument = true;

    QString title = document.synonym;
    if (title.isEmpty())
        title = document.name;
    m_hint->setText(QStringLiteral("Форма «%1»: элементов — %2, полей — %3, "
                                   "атрибутов — %4, команд — %5, модуль — %6 символов.")
                        .arg(title)
                        .arg(document.itemCount())
                        .arg(document.fieldCount())
                        .arg(document.attributes.size())
                        .arg(document.commands.size())
                        .arg(document.moduleText.size()));

    fillTree();
    m_preview->setDocument(&m_document);
    m_moduleEdit->setPlainText(document.moduleText);
}

void FormView::setModuleHighlightEnabled(bool enabled)
{
    if (m_highlighter)
        m_highlighter->setEnabled(enabled);
}

void FormView::setHighlightSettings(const BslHighlightSettings &settings)
{
    if (m_highlighter)
        m_highlighter->setSettings(settings);
}

void FormView::setDarkMode(bool dark)
{
    if (m_highlighter)
        m_highlighter->setDarkMode(dark);
}

void FormView::setScalePercent(int percent)
{
    if (percent <= 0)
        return;
    if (m_scaleCombo) {
        const int index = m_scaleCombo->findData(percent);
        if (index >= 0) {
            m_scaleCombo->setCurrentIndex(index);
            return;
        }
    }
    if (m_preview)
        m_preview->setScale(percent / 100.0);
}

void FormView::setMessage(const QString &message)
{
    m_document = FormDocument();
    m_hasDocument = false;
    m_exactModel.clear();
    m_exactCaption.clear();
    m_hint->setText(message);
    m_tree->clear();
    m_preview->clear();
    m_moduleEdit->clear();
}

void FormView::clear()
{
    setMessage(QStringLiteral("Выберите форму в дереве метаданных."));
}

void FormView::fillTree()
{
    m_tree->clear();

    auto *itemsRoot = new QTreeWidgetItem(m_tree);
    itemsRoot->setText(0, QStringLiteral("Элементы формы"));
    itemsRoot->setText(1, QStringLiteral("(%1)").arg(m_document.itemCount()));

    std::function<void(QTreeWidgetItem *, const FormItem &)> addItem =
        [&](QTreeWidgetItem *parent, const FormItem &item) {
            auto *node = new QTreeWidgetItem(parent);
            node->setText(0, item.title());
            node->setText(1, item.typeTitle());
            node->setText(2, item.name);
            node->setText(3, item.synonym);
            QStringList hints;
            if (item.type == FormItem::Type::Group && item.groupKind >= 0) {
                hints << QStringLiteral("вид группы: %1, ориентация: %2")
                    .arg(item.groupKind)
                    .arg(item.orientation == 1 ? QStringLiteral("горизонтальная")
                                               : QStringLiteral("вертикальная"));
            }
            if (item.widthUnits > 0)
                hints << QStringLiteral("ширина: %1 ед. (%2 px)").arg(item.widthUnits)
                    .arg(item.widthUnits * 10);
            if (item.heightUnits > 0)
                hints << QStringLiteral("высота: %1 ед. (%2 px)").arg(item.heightUnits)
                    .arg(item.heightUnits * 10);
            if (item.horizontalStretch >= 0)
                hints << QStringLiteral("растяжение: %1").arg(item.horizontalStretch == 1
                    ? QStringLiteral("да") : item.horizontalStretch == 2
                        ? QStringLiteral("авто") : QStringLiteral("нет"));
            if (!hints.isEmpty())
                node->setToolTip(0, hints.join(QStringLiteral("; ")));
            for (const FormItem &child : item.children)
                addItem(node, child);
        };
    for (const FormItem &item : m_document.items)
        addItem(itemsRoot, item);
    itemsRoot->setExpanded(true);
    for (int i = 0; i < itemsRoot->childCount() && i < 3; ++i)
        itemsRoot->child(i)->setExpanded(true);

    if (!m_document.attributes.isEmpty()) {
        auto *attributesRoot = new QTreeWidgetItem(m_tree);
        attributesRoot->setText(0, QStringLiteral("Атрибуты формы"));
        attributesRoot->setText(1, QStringLiteral("(%1)").arg(m_document.attributes.size()));
        for (const FormAttribute &attribute : m_document.attributes) {
            auto *node = new QTreeWidgetItem(attributesRoot);
            node->setText(0, attribute.synonym.isEmpty()
                             ? attribute.name : attribute.synonym);
            node->setText(1, QStringLiteral("Атрибут"));
            node->setText(2, attribute.name);
            node->setText(3, attribute.synonym);
            if (!attribute.typeHint.isEmpty())
                node->setToolTip(1, attribute.typeHint);
        }
    }

    if (!m_document.commands.isEmpty()) {
        auto *commandsRoot = new QTreeWidgetItem(m_tree);
        commandsRoot->setText(0, QStringLiteral("Команды формы"));
        commandsRoot->setText(1, QStringLiteral("(%1)").arg(m_document.commands.size()));
        for (const FormCommand &command : m_document.commands) {
            auto *node = new QTreeWidgetItem(commandsRoot);
            node->setText(0, command.synonym.isEmpty()
                             ? command.name : command.synonym);
            node->setText(1, QStringLiteral("Команда"));
            node->setText(2, command.name);
            node->setText(3, command.synonym);
        }
    }
}

void FormView::setExactViewAvailability(bool available, const QString &reason)
{
    m_exactAvailable = available;
    m_exactUnavailableReason = reason;
    if (m_modeCombo) {
        const int index = m_modeCombo->findData(true);
        if (index >= 0) {
            auto *model = qobject_cast<QStandardItemModel *>(m_modeCombo->model());
            if (model) {
                if (QStandardItem *item = model->item(index))
                    item->setEnabled(available);
            }
        }
        if (!available && m_modeCombo->currentIndex() == index)
            m_modeCombo->setCurrentIndex(0);
    }
    if (!available && m_exactEnabled) {
        m_exactEnabled = false;
        applyExactViewState();
    }
}

void FormView::setExactViewEnabled(bool enabled)
{
    const bool effective = enabled && m_exactAvailable;
    if (m_modeCombo) {
        const int index = m_modeCombo->findData(effective);
        if (index >= 0 && m_modeCombo->currentIndex() != index) {
            QSignalBlocker blocker(m_modeCombo);
            m_modeCombo->setCurrentIndex(index);
        }
    }
    if (m_exactEnabled == effective) {
        applyExactViewState();
        return;
    }
    m_exactEnabled = effective;
    applyExactViewState();
}

void FormView::applyExactViewState()
{
    if (m_views)
        m_views->setCurrentIndex(m_exactEnabled ? 1 : 0);
    if (m_exactEnabled) {
        if (!m_exactAvailable) {
            showExactViewMessage(
                m_exactUnavailableReason.isEmpty()
                    ? QStringLiteral("Точный вид недоступен на этой платформе.")
                    : m_exactUnavailableReason);
            return;
        }
        if (!m_hasDocument) {
            showExactViewMessage(QStringLiteral(
                "Выберите форму в дереве метаданных, чтобы увидеть её точный вид."));
            return;
        }
        if (m_exactModel.isEmpty()) {
            showExactViewMessage(QStringLiteral(
                "Точный вид для этой формы не подготовлен."));
            return;
        }
        if (m_exactMessage)
            m_exactMessage->hide();
        if (m_exactHost) {
            m_exactHost->setZoomFactor(
                m_scaleCombo ? m_scaleCombo->currentData().toInt() / 100.0 : 1.0);
            m_exactHost->load();
        }
    }
}

void FormView::setExactFormModel(const QByteArray &modelJson, const QString &caption)
{
    m_exactModel = modelJson;
    m_exactCaption = caption;
    if (m_exactHost) {
        m_exactHost->setStartUrl(exactStartUrl());
        if (m_exactEnabled)
            m_exactHost->load();
    }
    applyExactViewState();
}

void FormView::showExactViewMessage(const QString &message)
{
    if (!m_exactMessage)
        return;
    m_exactMessage->setText(message);
    m_exactMessage->show();
}

QString FormView::exactStartUrl() const
{
    // Страница рендерера конфигуратора 1С; модель формы она забирает из
    // model.json, который отдаёт приложение.
    return QStringLiteral("https://qv8.local/model.html");
}

bool FormView::provideExactResource(const QString &path, QByteArray *data,
                                    QString *mime) const
{
    if (!data)
        return false;
    if (path == QLatin1String("model.json")) {
        if (m_exactModel.isEmpty())
            return false;
        *data = m_exactModel;
        if (mime)
            *mime = QStringLiteral("application/json; charset=utf-8");
        return true;
    }

    QString clean = path;
    while (clean.startsWith(QLatin1Char('/')))
        clean.remove(0, 1);
    if (clean.isEmpty())
        clean = QStringLiteral("index.html");
    // Страница точного вида и её ресурсы лежат в qrc под /formpreview.
    QFile file(QStringLiteral(":/formpreview/") + clean);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    *data = file.readAll();
    if (mime) {
        const QString suffix = QFileInfo(clean).suffix().toLower();
        if (suffix == QLatin1String("html") || suffix == QLatin1String("htm"))
            *mime = QStringLiteral("text/html; charset=utf-8");
        else if (suffix == QLatin1String("js") || suffix == QLatin1String("mjs"))
            *mime = QStringLiteral("text/javascript; charset=utf-8");
        else if (suffix == QLatin1String("css"))
            *mime = QStringLiteral("text/css; charset=utf-8");
        else if (suffix == QLatin1String("json"))
            *mime = QStringLiteral("application/json; charset=utf-8");
        else if (suffix == QLatin1String("svg"))
            *mime = QStringLiteral("image/svg+xml");
        else if (suffix == QLatin1String("png"))
            *mime = QStringLiteral("image/png");
        else if (suffix == QLatin1String("gif"))
            *mime = QStringLiteral("image/gif");
        else if (suffix == QLatin1String("woff2"))
            *mime = QStringLiteral("font/woff2");
        else
            *mime = QStringLiteral("application/octet-stream");
    }
    return true;
}

} // namespace v8reader
