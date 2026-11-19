#include "gui/FormView.h"

#include "gui/BslHighlighter.h"

#include <QComboBox>
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
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

// Палитра и метрики интерфейса 1С:Предприятие 8.3 («Такси»).
// Форма управляемого приложения рисуется на белом фоне; заголовки таблиц,
// поля и кнопки имеют собственные цвета платформы.
namespace FormStyle {

const QColor text(0, 0, 0);
const QColor secondaryText(90, 90, 90);
const QColor fieldBorder(160, 160, 160);
const QColor primaryTop(255, 250, 205);
const QColor primaryBottom(250, 237, 158);
const QColor primaryBorder(226, 203, 120);
const QColor tableBorder(183, 183, 183);
const QColor gridLine(199, 199, 199);
const QColor headerBackground(242, 242, 242);
const QColor alternateRow(250, 250, 250);
const QColor separator(208, 208, 208);

constexpr double buttonHeight = 24.0;
constexpr double tableHeaderHeight = 22.0;
constexpr double tableRowHeight = 20.0;

} // namespace FormStyle

enum class ButtonIcon {
    None,
    Create,
    CreateBasedOn,
    Copy,
    Change,
    Print,
    Hierarchy,
    Paperclip,
    List,
    Clock,
    Fill,
    Chart,
    Envelope,
    Help,
    News,
    Window
};

struct PreviewControl
{
    enum class Kind { Field, Button, Table, PageTab, Decoration, Search, SearchOptions, Separator, HorizontalLine };
    Kind kind = Kind::Field;
    ButtonIcon icon = ButtonIcon::None;
    bool iconOnly = false;
    QRectF rect;
    QString title;
    QStringList columns;
    QVector<double> columnWidths;
    double labelWidth = 0.0;
    double inputWidth = -1.0;
    bool selectedTab = false;
    bool checkBox = false;
    bool dropDown = false;
    bool primary = false;
    bool labelOnly = false;
    bool calendar = false;
};

bool looksTechnical(const QString &text)
{
    return text.contains(QLatin1Char('='))
        || text.startsWith(QStringLiteral("ДЛФ"))
        || text.contains(QLatin1Char('%'));
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
    if (item.name == QStringLiteral("Баннер"))
        return true;
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
    if (has(QStringLiteral("Создать")))
        return ButtonIcon::Create;
    if (has(QStringLiteral("Скопировать")))
        return ButtonIcon::Copy;
    if (has(QStringLiteral("Изменить")))
        return ButtonIcon::Change;
    if (has(QStringLiteral("Печать")))
        return ButtonIcon::Print;
    if (has(QStringLiteral("ПоказатьДвижения"))
        || has(QStringLiteral("СтруктураПодчиненности")))
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
          m_font(QStringLiteral("Arial"), 9), m_metrics(m_font)
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

        // Платформа сама добавляет в форму документа стандартные команды
        // «Провести и закрыть», «Записать», «Провести» в начало командной
        // панели; в бинарной форме их нет.
        if (m_document.runtimeDocumentCommands) {
            std::function<v8reader::FormItem *(v8reader::FormItem &)> findCommandGroup =
                [&](v8reader::FormItem &item) -> v8reader::FormItem * {
                    if (item.type == v8reader::FormItem::Type::Group
                        && item.groupKind == 6
                        && (item.name.contains(QStringLiteral("Командн"))
                            || item.synonym.contains(QStringLiteral("командн"),
                                                     Qt::CaseInsensitive))) {
                        return &item;
                    }
                    for (v8reader::FormItem &child : item.children) {
                        if (v8reader::FormItem *found = findCommandGroup(child))
                            return found;
                    }
                    return nullptr;
                };
            for (v8reader::FormItem &root : localItems) {
                v8reader::FormItem *group = findCommandGroup(root);
                if (!group)
                    continue;
                v8reader::FormItem close;
                close.type = v8reader::FormItem::Type::Button;
                close.typeCode = 31;
                close.name = QStringLiteral("ПровестиИЗакрыть");
                close.synonym = QStringLiteral("Провести и закрыть");
                v8reader::FormItem write;
                write.type = v8reader::FormItem::Type::Button;
                write.typeCode = 31;
                write.name = QStringLiteral("Записать");
                write.synonym = QStringLiteral("Записать");
                v8reader::FormItem post;
                post.type = v8reader::FormItem::Type::Button;
                post.typeCode = 31;
                post.name = QStringLiteral("Провести");
                post.synonym = QStringLiteral("Провести");
                group->children.insert(0, close);
                group->children.insert(1, write);
                group->children.insert(2, post);
                break;
            }
        }

        QVector<const v8reader::FormItem *> items;
        for (const v8reader::FormItem &item : localItems) {
            if (!isServiceItem(item))
                items.append(&item);
        }
        *contentHeight = layoutVertical(items, QRectF(0.0, 0.0, m_width, 100000.0), 0);

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

    double labelWidth(const v8reader::FormItem &item) const
    {
        if (!item.showTitle)
            return 0.0;
        const QString label = fieldLabel(item);
        if (label.isEmpty())
            return 0.0;
        return std::min(210.0, m_metrics.horizontalAdvance(label + QStringLiteral(":")) + 10.0);
    }

    double preferredWidth(const v8reader::FormItem &item) const
    {
        using Type = v8reader::FormItem::Type;
        switch (item.type) {
        case Type::Field: {
            if (item.widthUnits > 0 && item.horizontalStretch != 1)
                return labelWidth(item) + item.widthUnits * 10.0 + 8.0;
            return std::max(170.0, labelWidth(item) + 160.0);
        }
        case Type::Button: {
            const ButtonIcon icon = iconForButton(item);
            const bool iconOnly = icon != ButtonIcon::None
                && icon != ButtonIcon::Create && icon != ButtonIcon::CreateBasedOn
                && icon != ButtonIcon::Print;
            if (iconOnly)
                return 32.0;
            double width = m_metrics.horizontalAdvance(buttonTitle(item)) + 16.0;
            if (icon != ButtonIcon::None)
                width += 22.0;
            return std::min(240.0, width);
        }
        case Type::Search:
            if (item.name.contains(QStringLiteral("СтрокаПоиска")))
                return 220.0;
            if (item.name.contains(QStringLiteral("УправлениеПоиском")))
                return 20.0;
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
                    return 34.0;
                const QString title = item.synonym.isEmpty()
                    ? humanizeCommand(item.name) : item.synonym;
                return std::max(64.0,
                    m_metrics.horizontalAdvance(title) + 44.0);
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
            return 27.0;
        case Type::Decoration:
            if (item.synonym.isEmpty())
                return 0.0;
            return item.heightUnits > 0 ? item.heightUnits * 10.0 : 19.0;
        case Type::Table:
            // В форме списка таблица занимает основную часть формы.
            return m_document.runtimeListCommands ? 470.0 : 360.0;
        case Type::Group: {
            if (item.groupKind == 1)
                return 27.0;
            if (isPages(item))
                return 26.0 + pageContentHeight(item, width);
            QVector<const v8reader::FormItem *> children;
            visibleChildren(item, &children);
            if (children.isEmpty())
                return 0.0;
            if (isHorizontal(item))
                return horizontalHeight(children, width, isCommandBar(item));
            double height = 0.0;
            for (const v8reader::FormItem *child : children) {
                if (child->name.endsWith(QStringLiteral("Использование")))
                    continue;
                height += naturalHeight(*child, width) + 5.0;
            }
            return height;
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

        double y = rect.top();
        for (const v8reader::FormItem *item : items) {
            if (item->name.endsWith(QStringLiteral("Использование")))
                continue;
            const double height = naturalHeight(*item, rect.width());
            if (height <= 0.0)
                continue;
            if (item->type == v8reader::FormItem::Type::Field) {
                const QString usageName = item->name + QStringLiteral("Использование");
                const QString usageLabel = usage.value(usageName);
                layoutField(*item, QRectF(rect.left(), y, rect.width(), 27.0),
                            0.0, usageLabel, !usageLabel.isEmpty(), !usageLabel.isEmpty());
                y += 27.0 + 5.0;
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
            visible.append(&help);
        }

        auto estimatedWidth = [&](const v8reader::FormItem *item) {
            double width = preferredWidth(*item);
            const bool searchOptions = item->type == v8reader::FormItem::Type::Search
                && item->name.contains(QStringLiteral("УправлениеПоиском"));
            if (!searchOptions)
                width = std::max(34.0, width);
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

        QVector<QVector<int>> rows;
        if (!allowWrap) {
            QVector<int> row;
            for (int i = 0; i < visible.size(); ++i)
                row.append(i);
            rows.append(row);
        } else {
            QVector<int> currentRow;
            double currentWidth = 0.0;
            for (int i = 0; i < visible.size(); ++i) {
                const double gap = currentRow.isEmpty() ? 0.0 : 8.0;
                if (!currentRow.isEmpty()
                    && currentWidth + gap + widths.at(i) > rect.width()) {
                    rows.append(currentRow);
                    currentRow.clear();
                    currentWidth = 0.0;
                }
                currentRow.append(i);
                currentWidth += (currentRow.size() > 1 ? 8.0 : 0.0) + widths.at(i);
            }
            if (!currentRow.isEmpty())
                rows.append(currentRow);
        }

        double y = rect.top();
        for (const QVector<int> &row : rows) {
            QVector<double> rowWidths;
            double total = 0.0;
            for (int index : row) {
                rowWidths.append(widths.at(index));
                total += widths.at(index);
            }
            total += 8.0 * (row.size() - 1);

            // Если не помещается и перенос не разрешён — сжимаем пропорционально.
            if (!allowWrap && total > rect.width()) {
                const double available = rect.width() - 8.0 * (row.size() - 1);
                const double scale = available / (total - 8.0 * (row.size() - 1));
                for (int i = 0; i < rowWidths.size(); ++i)
                    rowWidths[i] = std::max(34.0, rowWidths.at(i) * scale);
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
            double rowUsed = 0.0;
            for (int c = 0; c < row.size(); ++c)
                rowUsed += rowWidths.at(c);
            if (row.size() > 1)
                rowUsed += 8.0 * (row.size() - 1);
            const double rightShift = (rightStart > 0 && rowUsed < rect.width())
                ? rect.width() - rowUsed : 0.0;

            double x = rect.left();
            for (int c = 0; c < row.size(); ++c) {
                const int index = row.at(c);
                const v8reader::FormItem *item = visible.at(index);
                if (c == rightStart)
                    x += rightShift;
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
                    layoutField(*item, QRectF(x, y, rowWidths.at(c), 27.0), 0.0,
                                usageLabel, !usageLabel.isEmpty(), !usageLabel.isEmpty());
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
                                      std::min(rect.width(), 20.0),
                                      FormStyle::buttonHeight);
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
                m_controls->append(control);
            }
            return;
        case Type::Group: {
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
            if (children.isEmpty())
                return;
            if (isHorizontal(item))
                layoutHorizontal(children, rect, depth, isCommandBar(item),
                                 isTopCommandBar(item));
            else
                layoutVertical(children, rect, depth);
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
        double labelW = forcedLabelWidth;
        if (labelW <= 0.0) {
            labelW = std::min(210.0,
                m_metrics.horizontalAdvance(label + QStringLiteral(":")) + 10.0);
        }
        labelW = std::min(labelW, rect.width() * 0.7);

        PreviewControl control;
        control.kind = PreviewControl::Kind::Field;
        control.rect = rect;
        control.title = label;
        control.labelWidth = labelW;
        control.checkBox = checkBox;
        control.dropDown = dropDown;
        control.labelOnly = item.labelOnly;
        control.calendar = !item.labelOnly
            && (item.name.contains(QStringLiteral("Дата"))
                || item.name.contains(QStringLiteral("Период")));
        if (item.widthUnits > 0 && item.horizontalStretch != 1)
            control.inputWidth = item.widthUnits * 10.0;
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
        m_controls->append(control);
    }

    void tableColumns(const v8reader::FormItem &item, const QString &tableName,
                      QStringList *columns, QVector<int> *widths) const
    {
        for (const v8reader::FormItem &child : item.children) {
            if (isServiceItem(child) || isServiceDecoration(child))
                continue;
            if (child.type == v8reader::FormItem::Type::Field) {
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
                QString title = m_document.titles.value(base);
                if (title.isEmpty())
                    title = m_document.titles.value(child.name);
                if (title.isEmpty())
                    title = humanizeIdentifier(base);
                columns->append(title);
                widths->append(child.widthUnits);
            } else if (child.type == v8reader::FormItem::Type::Group) {
                tableColumns(child, tableName, columns, widths);
            }
        }
    }

    void layoutTable(const v8reader::FormItem &item, const QRectF &rect)
    {
        PreviewControl control;
        control.kind = PreviewControl::Kind::Table;
        const double tableHeight = m_document.runtimeListCommands ? 470.0 : 360.0;
        control.rect = QRectF(rect.left(), rect.top(), rect.width(),
                              std::min(rect.height(), tableHeight));
        QVector<int> widthUnits;
        tableColumns(item, item.name, &control.columns, &widthUnits);

        // В списках 1С по умолчанию показывает ключевые колонки, а остальные
        // пользователь добавляет сам. Повторяем типовой набор, чтобы таблица
        // выглядела как список платформы, а не как выгрузка всех полей.
        if (m_document.runtimeListCommands && control.columns.size() > 6) {
            const QVector<QStringList> preferred = {
                {QStringLiteral("Дата"), QStringLiteral("Период")},
                {QStringLiteral("Номер")},
                {QStringLiteral("Подотчетное лицо"), QStringLiteral("Контрагент"),
                 QStringLiteral("Сотрудник"), QStringLiteral("Физическое лицо")},
                {QStringLiteral("Сумма")},
                {QStringLiteral("Вид операции"), QStringLiteral("Статус"),
                 QStringLiteral("Состояние")},
                {QStringLiteral("Комментарий")}
            };
            QVector<int> picked;
            for (const QStringList &keys : preferred) {
                for (int i = 0; i < control.columns.size(); ++i) {
                    if (picked.contains(i))
                        continue;
                    bool matches = false;
                    for (const QString &key : keys) {
                        if (control.columns.at(i).contains(key, Qt::CaseInsensitive)) {
                            matches = true;
                            break;
                        }
                    }
                    if (matches) {
                        picked.append(i);
                        break;
                    }
                }
            }
            if (picked.size() >= 4) {
                QStringList filteredColumns;
                QVector<int> filteredUnits;
                for (int index : picked) {
                    filteredColumns.append(control.columns.at(index));
                    if (index < widthUnits.size())
                        filteredUnits.append(widthUnits.at(index));
                    else
                        filteredUnits.append(-1);
                }
                control.columns = filteredColumns;
                widthUnits = filteredUnits;
            }
        }

        if (!control.columns.isEmpty()) {
            QFont headerFont = m_font;
            headerFont.setBold(true);
            QFontMetricsF headerMetrics(headerFont);
            double total = 0.0;
            for (int i = 0; i < control.columns.size(); ++i) {
                const QString &column = control.columns.at(i);
                const int units = i < widthUnits.size() ? widthUnits.at(i) : -1;
                const double width = std::max(70.0, units > 0
                    ? units * 10.0
                    : std::min(240.0, std::max(110.0,
                        headerMetrics.horizontalAdvance(column) + 24.0)));
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

void drawButtonIcon(QPainter &painter, ButtonIcon icon, const QRectF &rect)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPointF c = rect.center();
    const double s = rect.width();
    const double stroke = std::max(1.2, s * 0.08);

    switch (icon) {
    case ButtonIcon::Create:
    case ButtonIcon::CreateBasedOn: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(76, 175, 80));
        painter.drawEllipse(rect.adjusted(0, 0, 0, 0));
        QPen pen(Qt::white, stroke, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(QPointF(c.x() - s * 0.22, c.y()),
                         QPointF(c.x() + s * 0.22, c.y()));
        painter.drawLine(QPointF(c.x(), c.y() - s * 0.22),
                         QPointF(c.x(), c.y() + s * 0.22));
        if (icon == ButtonIcon::CreateBasedOn) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(76, 175, 80));
            painter.drawEllipse(QPointF(rect.right() - 2.0, rect.top() + 2.0), 3.5, 3.5);
            painter.setPen(QPen(Qt::white, 1.2));
            painter.drawLine(QPointF(rect.right() - 4.5, rect.top() + 2.0),
                             QPointF(rect.right() + 0.5, rect.top() + 2.0));
            painter.drawLine(QPointF(rect.right() - 2.0, rect.top() - 0.5),
                             QPointF(rect.right() - 2.0, rect.top() + 4.5));
        }
        break;
    }
    case ButtonIcon::Copy: {
        const QRectF back(rect.left() + s * 0.10, rect.top() + s * 0.25,
                          s * 0.58, s * 0.58);
        painter.setPen(QPen(QColor(100, 100, 100), stroke));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(back, 1.5, 1.5);
        const QRectF front(rect.left() + s * 0.28, rect.top() + s * 0.08,
                           s * 0.58, s * 0.58);
        painter.setBrush(Qt::white);
        painter.drawRoundedRect(front, 1.5, 1.5);
        painter.setBrush(Qt::NoBrush);
        break;
    }
    case ButtonIcon::Change: {
        QPen pen(QColor(245, 145, 30), stroke, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(QPointF(rect.left() + s * 0.2, rect.bottom() - s * 0.2),
                         QPointF(rect.right() - s * 0.2, rect.top() + s * 0.2));
        painter.setPen(QPen(QColor(90, 90, 90), stroke * 0.8));
        painter.drawLine(QPointF(rect.left() + s * 0.18, rect.bottom() - s * 0.18),
                         QPointF(rect.left() + s * 0.35, rect.bottom() - s * 0.35));
        break;
    }
    case ButtonIcon::Print: {
        painter.setPen(QPen(QColor(70, 70, 70), stroke));
        painter.setBrush(QColor(210, 210, 210));
        const QRectF body(rect.left() + s * 0.12, rect.top() + s * 0.35,
                          s * 0.76, s * 0.42);
        painter.drawRoundedRect(body, 1.5, 1.5);
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF(rect.left() + s * 0.25, rect.top() + s * 0.08,
                                s * 0.5, s * 0.3));
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF(rect.left() + s * 0.25, rect.top() + s * 0.62,
                                s * 0.5, s * 0.3));
        painter.setBrush(Qt::NoBrush);
        break;
    }
    case ButtonIcon::Hierarchy: {
        painter.setPen(QPen(QColor(80, 80, 80), stroke));
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF(rect.left() + s * 0.08, rect.top() + s * 0.12,
                                s * 0.34, s * 0.24));
        painter.drawRect(QRectF(rect.left() + s * 0.58, rect.top() + s * 0.12,
                                s * 0.34, s * 0.24));
        painter.drawRect(QRectF(rect.left() + s * 0.34, rect.top() + s * 0.62,
                                s * 0.34, s * 0.24));
        painter.drawLine(QPointF(c.x(), rect.top() + s * 0.36),
                         QPointF(c.x(), rect.top() + s * 0.56));
        painter.drawLine(QPointF(rect.left() + s * 0.25, rect.top() + s * 0.36),
                         QPointF(rect.left() + s * 0.25, rect.top() + s * 0.5));
        painter.drawLine(QPointF(rect.right() - s * 0.25, rect.top() + s * 0.36),
                         QPointF(rect.right() - s * 0.25, rect.top() + s * 0.5));
        break;
    }
    case ButtonIcon::Paperclip: {
        painter.setPen(QPen(QColor(90, 90, 90), stroke, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(rect.left() + s * 0.25, rect.top() + s * 0.7);
        path.cubicTo(rect.left() + s * 0.05, rect.top() + s * 0.45,
                     rect.left() + s * 0.25, rect.top() + s * 0.1,
                     rect.left() + s * 0.5, rect.top() + s * 0.25);
        path.cubicTo(rect.left() + s * 0.7, rect.top() + s * 0.35,
                     rect.left() + s * 0.55, rect.top() + s * 0.8,
                     rect.left() + s * 0.78, rect.top() + s * 0.7);
        painter.drawPath(path);
        break;
    }
    case ButtonIcon::List: {
        painter.setPen(QPen(QColor(80, 80, 80), stroke));
        for (int i = 0; i < 3; ++i) {
            const double y = rect.top() + s * (0.25 + i * 0.25);
            painter.drawLine(QPointF(rect.left() + s * 0.35, y),
                             QPointF(rect.right() - s * 0.1, y));
            painter.drawEllipse(QPointF(rect.left() + s * 0.15, y), s * 0.06, s * 0.06);
        }
        break;
    }
    case ButtonIcon::Clock: {
        painter.setPen(QPen(QColor(80, 80, 80), stroke));
        painter.setBrush(Qt::white);
        painter.drawEllipse(rect.adjusted(s * 0.12, s * 0.12, -s * 0.12, -s * 0.12));
        painter.drawLine(c, QPointF(c.x(), c.y() - s * 0.22));
        painter.drawLine(c, QPointF(c.x() + s * 0.18, c.y() + s * 0.08));
        break;
    }
    case ButtonIcon::Fill: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 193, 7));
        QPolygonF bolt;
        bolt << QPointF(c.x() + s * 0.08, rect.top() + s * 0.05)
             << QPointF(rect.left() + s * 0.2, c.y() + s * 0.02)
             << QPointF(c.x(), c.y() + s * 0.02)
             << QPointF(c.x() - s * 0.12, rect.bottom() - s * 0.05)
             << QPointF(rect.right() - s * 0.15, c.y() - s * 0.04)
             << QPointF(c.x() + s * 0.02, c.y() - s * 0.04);
        painter.drawPolygon(bolt);
        break;
    }
    case ButtonIcon::Chart: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(66, 133, 244));
        painter.drawRect(QRectF(rect.left() + s * 0.15, rect.top() + s * 0.5,
                                s * 0.16, s * 0.4));
        painter.setBrush(QColor(52, 168, 83));
        painter.drawRect(QRectF(rect.left() + s * 0.42, rect.top() + s * 0.28,
                                s * 0.16, s * 0.62));
        painter.setBrush(QColor(251, 188, 5));
        painter.drawRect(QRectF(rect.left() + s * 0.69, rect.top() + s * 0.12,
                                s * 0.16, s * 0.78));
        break;
    }
    case ButtonIcon::Envelope: {
        painter.setPen(QPen(QColor(80, 80, 80), stroke));
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF(rect.left() + s * 0.1, rect.top() + s * 0.22,
                                s * 0.8, s * 0.56));
        painter.drawLine(QPointF(rect.left() + s * 0.1, rect.top() + s * 0.22),
                         QPointF(c.x(), rect.top() + s * 0.56));
        painter.drawLine(QPointF(rect.right() - s * 0.1, rect.top() + s * 0.22),
                         QPointF(c.x(), rect.top() + s * 0.56));
        break;
    }
    case ButtonIcon::Help: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(52, 120, 200));
        painter.drawEllipse(rect.adjusted(s * 0.08, s * 0.08, -s * 0.08, -s * 0.08));
        painter.setPen(QColor(Qt::white));
        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(int(s * 0.7));
        painter.setFont(font);
        painter.drawText(rect, Qt::AlignCenter, QStringLiteral("?"));
        break;
    }
    case ButtonIcon::News: {
        painter.setPen(QPen(QColor(70, 110, 160), stroke));
        painter.setBrush(QColor(220, 235, 250));
        painter.drawEllipse(rect.adjusted(s * 0.1, s * 0.1, -s * 0.1, -s * 0.1));
        painter.drawLine(QPointF(c.x(), rect.top() + s * 0.1),
                         QPointF(c.x(), rect.bottom() - s * 0.1));
        painter.drawLine(QPointF(rect.left() + s * 0.1, c.y()),
                         QPointF(rect.right() - s * 0.1, c.y()));
        break;
    }
    case ButtonIcon::Window: {
        painter.setPen(QPen(QColor(80, 80, 80), stroke));
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF(rect.left() + s * 0.12, rect.top() + s * 0.22,
                                s * 0.5, s * 0.5));
        painter.setBrush(QColor(230, 230, 230));
        painter.drawRect(QRectF(rect.left() + s * 0.38, rect.top() + s * 0.38,
                                s * 0.5, s * 0.5));
        break;
    }
    case ButtonIcon::None:
        break;
    }
    painter.restore();
}

void paintControl(QPainter &painter, const PreviewControl &control,
                  const QFont &font, const QFontMetricsF &metrics)
{
    painter.setFont(font);
    const QRectF rect = control.rect;

    switch (control.kind) {
    case PreviewControl::Kind::Field: {
        const QString label = control.title;
        if (control.labelOnly) {
            if (label.isEmpty())
                break;
            const bool link = label.contains(QStringLiteral("Цен"), Qt::CaseInsensitive)
                || label.contains(QStringLiteral("НДС"), Qt::CaseInsensitive)
                || label.contains(QStringLiteral("Настрой"), Qt::CaseInsensitive)
                || label.contains(QStringLiteral("Изменить"), Qt::CaseInsensitive);
            painter.setPen(link ? QColor(0, 102, 204) : FormStyle::text);
            painter.drawText(rect.adjusted(2, 0, -2, 0),
                             Qt::AlignLeft | Qt::AlignVCenter, label);
            break;
        }
        double x = rect.left();
        if (!label.isEmpty()) {
            const double labelWidth = std::min(control.labelWidth > 0.0
                    ? control.labelWidth
                    : std::max(40.0, metrics.horizontalAdvance(label + QStringLiteral(":")) + 6.0),
                rect.width() * 0.7);
            const QRectF labelRect(rect.left(), rect.top(),
                                   std::max(0.0, labelWidth), rect.height());
            painter.setPen(FormStyle::text);
            if (control.checkBox) {
                painter.drawText(labelRect.adjusted(0, 0, -2, 0),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 label + QStringLiteral(":"));
                x = labelRect.right() + 2.0;
            } else {
                painter.drawText(labelRect.adjusted(0, 0, -4, 0),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 label + QStringLiteral(":"));
                x = labelRect.right() + 3.0;
            }
        }
        if (control.checkBox) {
            const QRectF box(x + 1.0, rect.top() + (rect.height() - 13.0) / 2.0,
                             13.0, 13.0);
            painter.setPen(FormStyle::fieldBorder);
            painter.setBrush(Qt::white);
            painter.drawRect(box);
            painter.setBrush(Qt::NoBrush);
            x += 19.0;
        }
        double editWidth = std::max(24.0, rect.right() - x - 4.0);
        if (control.inputWidth > 0.0)
            editWidth = std::min(editWidth, control.inputWidth);
        const QRectF edit(x, rect.top() + 3.0, editWidth, rect.height() - 6.0);
        painter.setPen(FormStyle::fieldBorder);
        painter.setBrush(Qt::white);
        painter.drawRect(edit);
        painter.setBrush(Qt::NoBrush);
        if (control.dropDown) {
            const QPointF p(edit.right() - 12.0, edit.center().y());
            painter.setPen(FormStyle::secondaryText);
            painter.drawLine(p + QPointF(-4, -2), p + QPointF(0, 2));
            painter.drawLine(p + QPointF(0, 2), p + QPointF(4, -2));
        }
        if (control.calendar && edit.width() > 44.0) {
            painter.setPen(FormStyle::secondaryText);
            painter.drawLine(QPointF(edit.right() - 20.0, edit.top() + 2.0),
                             QPointF(edit.right() - 20.0, edit.bottom() - 2.0));
            const QRectF cal(edit.right() - 14.0, edit.center().y() - 5.0, 9.0, 9.0);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(cal.left(), cal.top() + 2.0, cal.width(),
                                    cal.height() - 2.0));
            painter.drawLine(QPointF(cal.left(), cal.top() + 4.0),
                             QPointF(cal.right(), cal.top() + 4.0));
            painter.drawLine(QPointF(cal.left() + 2.0, cal.top()),
                             QPointF(cal.left() + 2.0, cal.top() + 3.0));
            painter.drawLine(QPointF(cal.right() - 2.0, cal.top()),
                             QPointF(cal.right() - 2.0, cal.top() + 3.0));
        }
        break;
    }
    case PreviewControl::Kind::Button: {
        // В современном интерфейсе 1С кнопки командной панели плоские:
        // фон и рамка есть только у кнопки по умолчанию (жёлтая заливка).
        if (control.primary) {
            QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
            gradient.setColorAt(0.0, FormStyle::primaryTop);
            gradient.setColorAt(1.0, FormStyle::primaryBottom);
            painter.setPen(FormStyle::primaryBorder);
            painter.setBrush(gradient);
            painter.drawRoundedRect(rect, 2.0, 2.0);
            painter.setBrush(Qt::NoBrush);
        } else {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::NoBrush);
        }
        painter.setPen(FormStyle::text);

        const double iconSize = std::min(16.0, rect.height() - 8.0);
        double textLeft = rect.left() + 6.0;
        double textRight = rect.right() - (control.dropDown ? 18.0 : 6.0);
        if (control.icon != ButtonIcon::None) {
            QRectF iconRect;
            if (control.iconOnly) {
                iconRect = QRectF(rect.center().x() - iconSize / 2.0,
                                  rect.center().y() - iconSize / 2.0,
                                  iconSize, iconSize);
            } else {
                iconRect = QRectF(rect.left() + 5.0,
                                  rect.center().y() - iconSize / 2.0,
                                  iconSize, iconSize);
                textLeft = iconRect.right() + 4.0;
            }
            drawButtonIcon(painter, control.icon, iconRect);
        }
        if (!control.iconOnly) {
            const QString text = metrics.elidedText(control.title, Qt::ElideRight,
                                                    int(std::max(10.0, textRight - textLeft)));
            painter.setPen(FormStyle::text);
            painter.drawText(QRectF(textLeft, rect.top(), textRight - textLeft,
                                    rect.height()),
                             Qt::AlignLeft | Qt::AlignVCenter, text);
        }
        if (control.dropDown) {
            const QPointF p(rect.right() - 10.0, rect.center().y());
            painter.setPen(FormStyle::secondaryText);
            painter.drawLine(p + QPointF(-3, -2), p + QPointF(0, 2));
            painter.drawLine(p + QPointF(0, 2), p + QPointF(3, -2));
        }
        break;
    }
    case PreviewControl::Kind::Search: {
        painter.setPen(FormStyle::fieldBorder);
        painter.setBrush(Qt::white);
        painter.drawRect(rect);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor(160, 160, 160));
        const double cy = rect.center().y();
        painter.drawText(rect.adjusted(8, 0, -26, 0),
                         Qt::AlignLeft | Qt::AlignVCenter, control.title);
        painter.setPen(QColor(130, 130, 130));
        // Очистка строки поиска.
        painter.drawLine(QPointF(rect.right() - 18.0, cy - 4.0),
                         QPointF(rect.right() - 10.0, cy + 4.0));
        painter.drawLine(QPointF(rect.right() - 18.0, cy + 4.0),
                         QPointF(rect.right() - 10.0, cy - 4.0));
        break;
    }
    case PreviewControl::Kind::SearchOptions: {
        painter.setPen(QColor(120, 120, 120));
        const double cy = rect.center().y();
        painter.drawEllipse(QPointF(rect.center().x() - 2.0, cy - 1.0), 4.0, 4.0);
        painter.drawLine(QPointF(rect.center().x() + 1.0, cy + 2.0),
                         QPointF(rect.center().x() + 5.0, cy + 6.0));
        painter.drawLine(QPointF(rect.right() - 6.0, cy - 2.0),
                         QPointF(rect.right() - 3.0, cy + 1.0));
        painter.drawLine(QPointF(rect.right() - 3.0, cy + 1.0),
                         QPointF(rect.right() - 0.5, cy - 2.0));
        break;
    }
    case PreviewControl::Kind::Table: {
        const QStringList columns = control.columns;
        if (columns.isEmpty())
            return;
        QVector<double> widths = control.columnWidths;
        if (widths.size() != columns.size()) {
            widths.clear();
            for (int i = 0; i < columns.size(); ++i)
                widths.append(rect.width() / columns.size());
        }
        const double headerHeight = FormStyle::tableHeaderHeight;
        const double rowHeight = FormStyle::tableRowHeight;
        const double verticalScrollWidth = 10.0;
        const double contentWidth = std::max(1.0, rect.width() - verticalScrollWidth);
        double columnsTotal = 0.0;
        for (double width : widths)
            columnsTotal += width;
        const bool horizontalScroll = columnsTotal > contentWidth + 0.5;
        const double scrollHeight = horizontalScroll ? 10.0 : 0.0;
        const QRectF header(rect.left(), rect.top(), rect.width(), headerHeight);
        painter.setPen(FormStyle::tableBorder);
        painter.setBrush(FormStyle::headerBackground);
        painter.drawRect(header);
        painter.setBrush(Qt::NoBrush);
        QFont headerFont = font;
        painter.setFont(headerFont);
        painter.setPen(FormStyle::text);
        double columnX = rect.left();
        for (int i = 0; i < columns.size()
             && columnX < rect.left() + contentWidth; ++i) {
            const double width = widths.at(i);
            const QRectF cell(columnX, rect.top(), width, headerHeight);
            painter.setPen(FormStyle::gridLine);
            if (columnX + width < rect.left() + contentWidth) {
                painter.drawLine(cell.topRight(), cell.bottomRight());
            }
            painter.setPen(FormStyle::tableBorder);
            painter.drawLine(cell.bottomLeft(), cell.bottomRight());
            painter.setPen(FormStyle::text);
            const QString text = metrics.elidedText(columns.at(i), Qt::ElideRight,
                                                    int(width) - 8);
            painter.drawText(cell.adjusted(8, 0, -6, 0),
                             Qt::AlignLeft | Qt::AlignVCenter, text);
            // Индикатор сортировки, как у списка 1С: по дате/периоду.
            if (i == 0 && (text == QStringLiteral("Дата")
                           || text == QStringLiteral("Период"))) {
                const double arrowX = cell.right() - 10.0;
                const double top = cell.top() + 6.0;
                const double bottom = cell.bottom() - 6.0;
                painter.setPen(QPen(FormStyle::secondaryText, 1.2));
                painter.drawLine(QPointF(arrowX, top), QPointF(arrowX, bottom));
                painter.drawLine(QPointF(arrowX, bottom),
                                 QPointF(arrowX - 3.0, bottom - 3.5));
                painter.drawLine(QPointF(arrowX, bottom),
                                 QPointF(arrowX + 3.0, bottom - 3.5));
                painter.setPen(FormStyle::text);
            }
            columnX += width;
        }
        painter.setFont(font);
        painter.setPen(FormStyle::tableBorder);
        painter.drawRect(header);

        const int rows = std::max(1,
            int((rect.height() - headerHeight - scrollHeight) / rowHeight));
        for (int row = 0; row < rows; ++row) {
            const QRectF rowRect(rect.left(),
                                 rect.top() + headerHeight + row * rowHeight,
                                 contentWidth, rowHeight);
            if (row % 2 == 1) {
                painter.fillRect(rowRect, FormStyle::alternateRow);
            }
            painter.setPen(QPen(FormStyle::gridLine, 1.0, Qt::DotLine));
            painter.drawLine(rowRect.bottomLeft(), rowRect.bottomRight());
            painter.setPen(QPen(QColor(216, 216, 216), 1.0));
            double x = rect.left();
            for (int i = 0; i < widths.size() && x < rowRect.right(); ++i) {
                painter.drawLine(QPointF(x, rowRect.top()), QPointF(x, rowRect.bottom()));
                x += widths.at(i);
            }
        }
        const double tableHeight = std::min(rect.height(),
            headerHeight + rows * rowHeight + scrollHeight);
        painter.setPen(FormStyle::tableBorder);
        painter.drawRect(QRectF(rect.left(), rect.top(), rect.width(), tableHeight));

        // Вертикальная полоса прокрутки списка.
        const QRectF verticalTrack(rect.right() - verticalScrollWidth + 1.0,
                                   rect.top() + headerHeight + 1.0,
                                   verticalScrollWidth - 2.0,
                                   std::max(1.0, tableHeight - headerHeight
                                       - scrollHeight - 2.0));
        painter.fillRect(verticalTrack, QColor(241, 241, 241));
        const double verticalThumb = std::max(24.0,
            verticalTrack.height() * 0.45);
        painter.fillRect(QRectF(verticalTrack.left(), verticalTrack.top(),
                                verticalTrack.width(),
                                std::min(verticalThumb, verticalTrack.height())),
                         QColor(193, 193, 193));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(120, 120, 120));
        const double arrowCenterX = verticalTrack.center().x();
        QPolygonF upArrow;
        upArrow << QPointF(arrowCenterX - 2.5, verticalTrack.top() + 4.0)
                << QPointF(arrowCenterX + 2.5, verticalTrack.top() + 4.0)
                << QPointF(arrowCenterX, verticalTrack.top() + 7.0);
        painter.drawPolygon(upArrow);
        QPolygonF downArrow;
        downArrow << QPointF(arrowCenterX - 2.5, verticalTrack.bottom() - 4.0)
                  << QPointF(arrowCenterX + 2.5, verticalTrack.bottom() - 4.0)
                  << QPointF(arrowCenterX, verticalTrack.bottom() - 7.0);
        painter.drawPolygon(downArrow);

        if (horizontalScroll) {
            // Уголок между полосами прокрутки.
            painter.fillRect(QRectF(rect.right() - verticalScrollWidth + 1.0,
                                    rect.bottom() - scrollHeight + 1.0,
                                    verticalScrollWidth - 2.0, scrollHeight - 2.0),
                             QColor(241, 241, 241));
            const QRectF track(rect.left() + 1.0,
                               rect.top() + tableHeight - scrollHeight + 1.0,
                               contentWidth - 2.0, scrollHeight - 2.0);
            painter.fillRect(track, QColor(241, 241, 241));
            const double thumb = std::max(20.0,
                track.width() * contentWidth / columnsTotal - 20.0);
            painter.fillRect(QRectF(track.left() + 10.0, track.top(),
                                    std::min(thumb, track.width() - 20.0),
                                    track.height()),
                             QColor(193, 193, 193));
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(120, 120, 120));
            const double arrowY = track.center().y();
            QPolygonF leftArrow;
            leftArrow << QPointF(track.left() + 7.0, arrowY - 3.0)
                      << QPointF(track.left() + 7.0, arrowY + 3.0)
                      << QPointF(track.left() + 3.0, arrowY);
            painter.drawPolygon(leftArrow);
            QPolygonF rightArrow;
            rightArrow << QPointF(track.right() - 7.0, arrowY - 3.0)
                       << QPointF(track.right() - 7.0, arrowY + 3.0)
                       << QPointF(track.right() - 3.0, arrowY);
            painter.drawPolygon(rightArrow);
        }
        break;
    }
    case PreviewControl::Kind::PageTab: {
        QPainterPath tab;
        tab.moveTo(rect.left(), rect.bottom());
        tab.lineTo(rect.left(), rect.top() + 3.0);
        tab.quadTo(rect.left(), rect.top(), rect.left() + 3.0, rect.top());
        tab.lineTo(rect.right() - 3.0, rect.top());
        tab.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + 3.0);
        tab.lineTo(rect.right(), rect.bottom());
        tab.closeSubpath();
        painter.setPen(FormStyle::fieldBorder);
        painter.setBrush(control.selectedTab ? QColor(255, 255, 255)
                                             : QColor(236, 236, 236));
        painter.drawPath(tab);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(FormStyle::text);
        const QString text = metrics.elidedText(control.title, Qt::ElideRight,
                                                int(rect.width()) - 8);
        painter.drawText(rect.adjusted(4, 0, -4, 0), Qt::AlignCenter, text);
        break;
    }
    case PreviewControl::Kind::Decoration: {
        painter.setPen(FormStyle::text);
        painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(control.title, Qt::ElideRight,
                                            int(rect.width())));
        break;
    }
    case PreviewControl::Kind::Separator: {
        painter.setPen(QPen(FormStyle::separator, 1.0));
        painter.drawLine(rect.topLeft(), rect.bottomLeft());
        break;
    }
    case PreviewControl::Kind::HorizontalLine: {
        painter.setPen(QPen(FormStyle::fieldBorder, 1.0));
        painter.drawLine(QPointF(rect.left(), rect.center().y()),
                         QPointF(rect.right(), rect.center().y()));
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

        const QFont titleFont(QStringLiteral("Arial"), 14, QFont::Bold);
        const QFont font(QStringLiteral("Arial"), 9);
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

        const double margin = 6.0;
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
            ? (viewportWidth - 1.0) / 0.75 : kFormWidth;
        const double logicalWidth = std::clamp(baseWidth, 700.0, 1600.0);
        const double contentWidth = logicalWidth - margin * 2.0;

        // Высота окна 1С: таблица списка должна занимать всё доступное место,
        // а не оставлять пустую полосу под собой.
        const double viewportLogical = viewportHeight > 0.0
            ? viewportHeight / m_scale : 0.0;
        const double targetContentHeight = std::max(0.0,
            viewportLogical - margin * 2.0 - 2.0);

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
            control.rect.translate(margin, margin);
        m_controls = controls;
        m_contentHeight = contentHeight;
        double logicalHeight = qBound(280.0, margin * 2.0 + contentHeight, 8000.0);
        if (targetContentHeight > 0.0) {
            logicalHeight = std::max(logicalHeight,
                margin * 2.0 + targetContentHeight);
        }
        setFixedSize(int(logicalWidth * m_scale), int(logicalHeight * m_scale));
        update();
    }

    static constexpr double kFormWidth = 880.0;

    const v8reader::FormDocument *m_document = nullptr;
    QVector<PreviewControl> m_controls;
    double m_contentHeight = 0.0;
    double m_scale = 0.75;
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
    m_scaleCombo->setCurrentIndex(1);
    toolbar->addWidget(m_scaleCombo);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    auto *splitter = new QSplitter(Qt::Horizontal, formPage);

    m_tree = new QTreeWidget(splitter);
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

    auto *scroll = new QScrollArea(splitter);
    scroll->setWidgetResizable(false);
    scroll->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_preview = new FormPreviewWidget(scroll);
    scroll->setWidget(m_preview);

    splitter->addWidget(m_tree);
    splitter->addWidget(scroll);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    // Дереву элементов достаточно узкой колонки: форма должна занимать
    // основную часть панели, как клиентское окно в 1С.
    splitter->setSizes({230, 970});
    layout->addWidget(splitter, 1);
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
                if (!m_preview)
                    return;
                const int percent = m_scaleCombo->itemData(index).toInt();
                m_preview->setScale(percent / 100.0);
            });

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

} // namespace v8reader
