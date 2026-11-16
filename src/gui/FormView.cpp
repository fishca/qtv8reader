#include "gui/FormView.h"

#include <QComboBox>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QScrollArea>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

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
    enum class Kind { Field, Button, Table, PageTab, Decoration, Search };
    Kind kind = Kind::Field;
    ButtonIcon icon = ButtonIcon::None;
    bool iconOnly = false;
    QRectF rect;
    QString title;
    QStringList columns;
    double labelWidth = 0.0;
    bool selectedTab = false;
    bool checkBox = false;
    bool dropDown = false;
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
        return ButtonIcon::CreateBasedOn;
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

    void build(QVector<PreviewControl> *controls, double *contentHeight)
    {
        m_controls = controls;
        QVector<const v8reader::FormItem *> items;
        for (const v8reader::FormItem &item : m_document.items) {
            if (!isServiceItem(item))
                items.append(&item);
        }
        *contentHeight = layoutVertical(items, QRectF(0.0, 0.0, m_width, 100000.0), 0);
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
        const QString label = fieldLabel(item);
        if (label.isEmpty())
            return 0.0;
        return std::min(210.0, m_metrics.horizontalAdvance(label + QStringLiteral(":")) + 10.0);
    }

    double preferredWidth(const v8reader::FormItem &item) const
    {
        using Type = v8reader::FormItem::Type;
        switch (item.type) {
        case Type::Field:
            return std::max(170.0, labelWidth(item) + 160.0);
        case Type::Button: {
            const ButtonIcon icon = iconForButton(item);
            const bool iconOnly = icon != ButtonIcon::None
                && icon != ButtonIcon::Create && icon != ButtonIcon::CreateBasedOn;
            if (iconOnly)
                return 32.0;
            double width = m_metrics.horizontalAdvance(buttonTitle(item)) + 16.0;
            if (icon != ButtonIcon::None)
                width += 22.0;
            return std::min(240.0, width);
        }
        case Type::Search:
            return item.name.contains(QStringLiteral("СтрокаПоиска")) ? 220.0 : 40.0;
        case Type::Table:
            return 340.0;
        case Type::Decoration:
            return std::max(40.0, m_metrics.horizontalAdvance(item.synonym) + 10.0);
        case Type::Group: {
            if (item.groupKind == 1)
                return 110.0;
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
            return item.synonym.isEmpty() ? 0.0 : 19.0;
        case Type::Table:
            return 360.0;
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

        double commonLabel = 0.0;
        for (const v8reader::FormItem *item : items) {
            if (item->type != v8reader::FormItem::Type::Field)
                continue;
            if (item->name.endsWith(QStringLiteral("Использование")))
                continue;
            const QString usageLabel = usage.value(item->name + QStringLiteral("Использование"));
            commonLabel = std::max(commonLabel,
                usageLabel.isEmpty()
                    ? labelWidth(*item)
                    : std::min(210.0, m_metrics.horizontalAdvance(usageLabel + QStringLiteral(":")) + 10.0));
        }

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
                            commonLabel, usageLabel, !usageLabel.isEmpty(), !usageLabel.isEmpty());
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
        if (item.type == Type::Field || item.type == Type::Search)
            return true;
        if (item.type != Type::Group)
            return false;
        return item.groupKind != 1 && item.groupKind != 6 && item.groupKind != 9;
    }

    double layoutHorizontal(const QVector<const v8reader::FormItem *> &items,
                            const QRectF &rect, int depth, bool allowWrap)
    {
        QVector<const v8reader::FormItem *> visible;
        for (const v8reader::FormItem *item : items) {
            if (!item->name.endsWith(QStringLiteral("Использование")))
                visible.append(item);
        }
        if (visible.isEmpty())
            return 0.0;

        const QHash<QString, QString> usage = usageLabels(items);

        QVector<double> widths;
        for (int i = 0; i < visible.size(); ++i)
            widths.append(std::max(34.0, preferredWidth(*visible.at(i))));

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

            double x = rect.left();
            for (int c = 0; c < row.size(); ++c) {
                const v8reader::FormItem *item = visible.at(row.at(c));
                const QString usageName = item->name + QStringLiteral("Использование");
                const QString usageLabel = usage.value(usageName);
                if (item->type == v8reader::FormItem::Type::Field) {
                    layoutField(*item, QRectF(x, y, rowWidths.at(c), 27.0), 0.0,
                                usageLabel, !usageLabel.isEmpty(), !usageLabel.isEmpty());
                } else {
                    layoutNode(*item, QRectF(x, y, rowWidths.at(c), rowHeight), depth + 1);
                }
                x += rowWidths.at(c) + 8.0;
            }
            y += rowHeight + 5.0;
        }
        return y - rect.top();
    }

    void layoutNode(const v8reader::FormItem &item, const QRectF &rect, int depth,
                    const QString &usageLabel = QString(), bool usageCheck = false,
                    bool usageDropDown = false)
    {
        using Type = v8reader::FormItem::Type;
        if (depth > 40)
            return;

        switch (item.type) {
        case Type::Field:
            layoutField(item, rect, 0.0, usageLabel, usageCheck, usageDropDown);
            return;
        case Type::Button:
            layoutButton(item, QRectF(rect.left(), rect.top(),
                                      std::min(rect.width(), preferredWidth(item)),
                                      27.0), false);
            return;
        case Type::Search:
            if (!item.name.contains(QStringLiteral("СтрокаПоиска")))
                return;
            {
                PreviewControl control;
                control.kind = PreviewControl::Kind::Search;
                control.rect = QRectF(rect.left(), rect.top(),
                                      std::min(rect.width(), 220.0), 27.0);
                control.title = QStringLiteral("Поиск (Ctrl+F)");
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
                control.rect = QRectF(rect.left(), rect.top(), rect.width(), 19.0);
                control.title = item.synonym;
                m_controls->append(control);
            }
            return;
        case Type::Group: {
            if (item.groupKind == 1) {
                layoutButton(item, QRectF(rect.left(), rect.top(),
                                          std::min(rect.width(), preferredWidth(item)),
                                          27.0), true);
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
                layoutHorizontal(children, rect, depth, isCommandBar(item));
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
        const QString label = labelOverride.isEmpty() ? fieldLabel(item) : labelOverride;
        double labelW = forcedLabelWidth;
        if (labelW <= 0.0)
            labelW = std::min(210.0,
                m_metrics.horizontalAdvance(label + QStringLiteral(":")) + 10.0);
        labelW = std::min(labelW, rect.width() * 0.7);

        PreviewControl control;
        control.kind = PreviewControl::Kind::Field;
        control.rect = rect;
        control.title = label;
        control.labelWidth = labelW;
        control.checkBox = checkBox;
        control.dropDown = dropDown;
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
            && control.icon != ButtonIcon::CreateBasedOn;
        control.dropDown = dropDown;
        m_controls->append(control);
    }

    void tableColumns(const v8reader::FormItem &item, const QString &tableName,
                      QStringList *columns) const
    {
        for (const v8reader::FormItem &child : item.children) {
            if (isServiceItem(child) || isServiceDecoration(child))
                continue;
            if (child.type == v8reader::FormItem::Type::Field) {
                if (child.name == QStringLiteral("ЕстьФайлы"))
                    continue;
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
            } else if (child.type == v8reader::FormItem::Type::Group) {
                tableColumns(child, tableName, columns);
            }
        }
    }

    void layoutTable(const v8reader::FormItem &item, const QRectF &rect)
    {
        PreviewControl control;
        control.kind = PreviewControl::Kind::Table;
        control.rect = QRectF(rect.left(), rect.top(), rect.width(),
                              std::min(rect.height(), 360.0));
        tableColumns(item, item.name, &control.columns);
        m_controls->append(control);
    }

    void layoutPages(const v8reader::FormItem &pages, const QRectF &rect)
    {
        QVector<const v8reader::FormItem *> pageItems;
        visibleChildren(pages, &pageItems);

        double x = rect.left();
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
            x += width + 2.0;
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
        double x = rect.left();
        if (!label.isEmpty()) {
            const double labelWidth = std::min(control.labelWidth > 0.0
                    ? control.labelWidth
                    : std::max(40.0, metrics.horizontalAdvance(label + QStringLiteral(":")) + 6.0),
                rect.width() * 0.7);
            const QRectF labelRect(x, rect.top(), labelWidth, rect.height());
            painter.setPen(QColor(50, 50, 50));
            painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter,
                             label + QStringLiteral(":"));
            x += labelWidth + 2.0;
        }
        if (control.checkBox) {
            const QRectF box(x, rect.top() + (rect.height() - 15.0) / 2.0, 15.0, 15.0);
            painter.setPen(QColor(150, 150, 150));
            painter.setBrush(Qt::white);
            painter.drawRect(box);
            painter.setBrush(Qt::NoBrush);
            x += 19.0;
        }
        const QRectF edit(x, rect.top() + 3.0,
                          std::max(24.0, rect.right() - x - 4.0),
                          rect.height() - 6.0);
        painter.setPen(QColor(165, 165, 165));
        painter.setBrush(Qt::white);
        painter.drawRoundedRect(edit, 2.0, 2.0);
        painter.setBrush(Qt::NoBrush);
        if (control.dropDown) {
            const QPointF p(edit.right() - 12.0, edit.center().y());
            painter.setPen(QColor(90, 90, 90));
            painter.drawLine(p + QPointF(-4, -2), p + QPointF(0, 2));
            painter.drawLine(p + QPointF(0, 2), p + QPointF(4, -2));
        }
        break;
    }
    case PreviewControl::Kind::Button: {
        painter.setPen(QColor(125, 125, 125));
        painter.setBrush(QColor(240, 240, 240));
        painter.drawRoundedRect(rect, 3.0, 3.0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor(35, 35, 35));

        const double iconSize = std::min(18.0, rect.height() - 8.0);
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
            painter.setPen(QColor(35, 35, 35));
            painter.drawText(QRectF(textLeft, rect.top(), textRight - textLeft,
                                    rect.height()),
                             Qt::AlignLeft | Qt::AlignVCenter, text);
        }
        if (control.dropDown) {
            const QPointF p(rect.right() - 10.0, rect.center().y());
            painter.setPen(QColor(80, 80, 80));
            painter.drawLine(p + QPointF(-3, -2), p + QPointF(0, 2));
            painter.drawLine(p + QPointF(0, 2), p + QPointF(3, -2));
        }
        break;
    }
    case PreviewControl::Kind::Search: {
        painter.setPen(QColor(165, 165, 165));
        painter.setBrush(Qt::white);
        painter.drawRoundedRect(rect, 3.0, 3.0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor(140, 140, 140));
        const double cy = rect.center().y();
        painter.drawEllipse(QPointF(rect.left() + 15.0, cy), 5.0, 5.0);
        painter.drawLine(QPointF(rect.left() + 19.0, cy + 4.0),
                         QPointF(rect.left() + 24.0, cy + 9.0));
        painter.setPen(QColor(170, 170, 170));
        painter.drawText(rect.adjusted(30, 0, -22, 0),
                         Qt::AlignLeft | Qt::AlignVCenter, control.title);
        painter.setPen(QColor(130, 130, 130));
        painter.drawLine(QPointF(rect.right() - 18.0, cy - 4.0),
                         QPointF(rect.right() - 10.0, cy + 4.0));
        painter.drawLine(QPointF(rect.right() - 18.0, cy + 4.0),
                         QPointF(rect.right() - 10.0, cy - 4.0));
        break;
    }
    case PreviewControl::Kind::Table: {
        const QStringList columns = control.columns;
        if (columns.isEmpty())
            return;
        const int visible = std::min(9, int(columns.size()));
        const double columnWidth = rect.width() / visible;
        const double headerHeight = 28.0;
        const QRectF header(rect.left(), rect.top(), rect.width(), headerHeight);
        painter.setPen(QColor(150, 150, 150));
        painter.setBrush(QColor(242, 242, 242));
        painter.drawRect(header);
        painter.setBrush(Qt::NoBrush);
        QFont headerFont = font;
        headerFont.setBold(true);
        painter.setFont(headerFont);
        painter.setPen(QColor(40, 40, 40));
        for (int i = 0; i < visible; ++i) {
            const QRectF cell(rect.left() + i * columnWidth, rect.top(),
                              columnWidth, headerHeight);
            painter.drawRect(cell);
            const QString text = metrics.elidedText(columns.at(i), Qt::ElideRight,
                                                    int(columnWidth) - 8);
            painter.drawText(cell.adjusted(4, 0, -4, 0),
                             Qt::AlignLeft | Qt::AlignVCenter, text);
        }
        painter.setFont(font);
        painter.setPen(QColor(150, 150, 150));
        const double rowHeight = 20.0;
        const int rows = std::max(1, int((rect.height() - headerHeight) / rowHeight));
        for (int row = 0; row < rows; ++row) {
            for (int i = 0; i < visible; ++i) {
                const QRectF cell(rect.left() + i * columnWidth,
                                  rect.top() + headerHeight + row * rowHeight,
                                  columnWidth, rowHeight);
                painter.drawRect(cell);
            }
        }

        // Горизонтальная полоса прокрутки, как у списка.
        const QRectF track(rect.left(), rect.bottom() - 6.0, rect.width(), 6.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(225, 225, 225));
        painter.drawRoundedRect(track, 3.0, 3.0);
        const double thumbWidth = rect.width() * 0.45;
        painter.setBrush(QColor(190, 190, 190));
        painter.drawRoundedRect(QRectF(track.left() + 6.0, track.top(),
                                       thumbWidth, track.height()), 3.0, 3.0);
        painter.setBrush(Qt::NoBrush);
        break;
    }
    case PreviewControl::Kind::PageTab: {
        painter.setPen(QColor(140, 140, 140));
        painter.setBrush(control.selectedTab ? QColor(255, 255, 255)
                                             : QColor(238, 238, 238));
        painter.drawRect(rect);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor(40, 40, 40));
        const QString text = metrics.elidedText(control.title, Qt::ElideRight,
                                                int(rect.width()) - 8);
        painter.drawText(rect.adjusted(4, 0, -4, 0), Qt::AlignCenter, text);
        break;
    }
    case PreviewControl::Kind::Decoration: {
        painter.setPen(QColor(80, 80, 80));
        painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(control.title, Qt::ElideRight,
                                            int(rect.width())));
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

        painter.setFont(titleFont);
        painter.setPen(QColor(30, 30, 30));
        QString title = m_document->synonym;
        if (title.isEmpty())
            title = m_document->name;
        if (title.isEmpty())
            title = QStringLiteral("Форма");
        painter.drawText(QRectF(20, 14, kFormWidth - 40, 28),
                         Qt::AlignHCenter | Qt::AlignVCenter, title);

        QFont subtitleFont(QStringLiteral("Arial"), 8);
        painter.setFont(subtitleFont);
        painter.setPen(QColor(130, 130, 130));
        painter.drawText(QRectF(20, 40, kFormWidth - 40, 16),
                         Qt::AlignHCenter | Qt::AlignVCenter,
                         QStringLiteral("управляемая форма · %1 элементов · %2 полей · %3 команд")
                             .arg(m_document->itemCount())
                             .arg(m_document->fieldCount())
                             .arg(m_document->commands.size()));

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

        const double contentWidth = kFormWidth - 40.0;
        LayoutEngine engine(*m_document, contentWidth);
        double contentHeight = 0.0;
        QVector<PreviewControl> controls;
        engine.build(&controls, &contentHeight);
        for (PreviewControl &control : controls)
            control.rect.translate(20.0, 72.0);
        m_controls = controls;
        m_contentHeight = contentHeight;
        const double logicalHeight = qBound(280.0, 72.0 + contentHeight + 24.0, 8000.0);
        setFixedSize(int(kFormWidth * m_scale), int(logicalHeight * m_scale));
        update();
    }

    static constexpr double kFormWidth = 900.0;

    const v8reader::FormDocument *m_document = nullptr;
    QVector<PreviewControl> m_controls;
    double m_contentHeight = 0.0;
    double m_scale = 0.75;
};

namespace v8reader {

FormView::FormView(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 0);

    m_hint = new QLabel(QStringLiteral("Выберите форму в дереве метаданных."), this);
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint);

    auto *toolbar = new QHBoxLayout();
    toolbar->addWidget(new QLabel(QStringLiteral("Масштаб:"), this));
    m_scaleCombo = new QComboBox(this);
    m_scaleCombo->setObjectName(QStringLiteral("formScale"));
    const QVector<int> scales = {50, 75, 100, 125, 150};
    for (int percent : scales)
        m_scaleCombo->addItem(QStringLiteral("%1%").arg(percent), percent);
    m_scaleCombo->setCurrentIndex(1);
    toolbar->addWidget(m_scaleCombo);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

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
    splitter->setSizes({300, 900});
    layout->addWidget(splitter, 1);

    connect(m_scaleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (!m_preview)
                    return;
                const int percent = m_scaleCombo->itemData(index).toInt();
                m_preview->setScale(percent / 100.0);
            });

    clear();
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
            if (item.type == FormItem::Type::Group && item.groupKind >= 0) {
                node->setToolTip(1, QStringLiteral("код вида группы: %1, ориентация: %2")
                    .arg(item.groupKind)
                    .arg(item.orientation == 1 ? QStringLiteral("горизонтальная")
                                               : QStringLiteral("вертикальная")));
            }
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

    auto *moduleRoot = new QTreeWidgetItem(m_tree);
    moduleRoot->setText(0, QStringLiteral("Модуль формы"));
    moduleRoot->setText(1, QStringLiteral("Модуль"));
    moduleRoot->setText(2, QStringLiteral("%1 символов").arg(m_document.moduleText.size()));
}

} // namespace v8reader
