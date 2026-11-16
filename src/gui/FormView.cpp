#include "gui/FormView.h"

#include <QFontMetricsF>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QScrollArea>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

struct PreviewControl
{
    enum class Kind { Field, Button, Table, PageTab, Decoration };
    Kind kind = Kind::Field;
    QRectF rect;
    QString title;
    QString text;
    QStringList columns;
};

bool isServiceItem(const v8reader::FormItem &item)
{
    if (item.type == v8reader::FormItem::Type::AutoCommandBar)
        return true;
    if (item.type == v8reader::FormItem::Type::Group
        && (item.groupKind == 8 || item.groupKind == 1))
        return true;
    if (item.name.contains(QStringLiteral("КонтекстноеМеню"))
        || item.name.endsWith(QStringLiteral("РасширеннаяПодсказка"))
        || item.name.contains(QStringLiteral("АвтоКоманднаяПанель")))
        return true;
    return false;
}

bool isServiceDecoration(const v8reader::FormItem &item)
{
    return item.type == v8reader::FormItem::Type::Decoration
        && item.synonym.isEmpty() && item.name.isEmpty();
}

void collectTableFields(const v8reader::FormItem &item, QStringList *columns)
{
    for (const v8reader::FormItem &child : item.children) {
        if (isServiceItem(child) || isServiceDecoration(child))
            continue;
        if (child.type == v8reader::FormItem::Type::Field) {
            columns->append(child.title());
        } else {
            collectTableFields(child, columns);
        }
    }
}

class LayoutContext
{
public:
    QFont font;
    QFontMetricsF metrics;

    LayoutContext()
        : font(QFont(QStringLiteral("Arial"), 9)), metrics(font)
    {
    }

    void add(PreviewControl control) { controls.append(control); }

    double preferredWidth(const v8reader::FormItem &item, int depth) const
    {
        using Type = v8reader::FormItem::Type;
        switch (item.type) {
        case Type::Field: {
            const QString label = item.title();
            const double labelWidth = std::min(220.0,
                metrics.horizontalAdvance(label) + 10.0);
            return std::max(160.0, labelWidth + 150.0);
        }
        case Type::Button:
            return std::min(240.0,
                metrics.horizontalAdvance(item.title()) + 30.0);
        case Type::Table:
            return 420.0;
        case Type::Decoration:
            return metrics.horizontalAdvance(item.title()) + 8.0;
        case Type::Group: {
            if (item.groupKind == 9 || item.groupKind == 6 || item.orientation == 1) {
                double width = 0.0;
                int count = 0;
                for (const v8reader::FormItem &child : item.children) {
                    if (isServiceItem(child) || isServiceDecoration(child))
                        continue;
                    width += preferredWidth(child, depth + 1) + 8.0;
                    ++count;
                }
                return count > 0 ? std::max(160.0, width) : 0.0;
            }
            double width = 180.0;
            for (const v8reader::FormItem &child : item.children) {
                if (isServiceItem(child) || isServiceDecoration(child))
                    continue;
                width = std::max(width, preferredWidth(child, depth + 1));
            }
            return width;
        }
        case Type::AutoCommandBar:
        case Type::Column:
        case Type::Other:
            return 0.0;
        }
        return 0.0;
    }

    double naturalHeight(const v8reader::FormItem &item, double width) const
    {
        using Type = v8reader::FormItem::Type;
        switch (item.type) {
        case Type::Field:
        case Type::Button:
            return 26.0;
        case Type::Table:
            return 150.0;
        case Type::Decoration:
            return item.synonym.isEmpty() ? 0.0 : 18.0;
        case Type::Group: {
            QVector<const v8reader::FormItem *> children;
            for (const v8reader::FormItem &child : item.children) {
                if (!isServiceItem(child) && !isServiceDecoration(child))
                    children.append(&child);
            }
            if (children.isEmpty())
                return 0.0;
            if (item.groupKind == 3) {
                return 26.0 + naturalHeight(*children.first(), width);
            }
            const bool horizontal = item.groupKind == 9 || item.groupKind == 6
                || item.orientation == 1;
            if (horizontal) {
                double height = 0.0;
                for (const v8reader::FormItem *child : children)
                    height = std::max(height, naturalHeight(*child, width));
                return height;
            }
            double height = 0.0;
            for (const v8reader::FormItem *child : children)
                height += naturalHeight(*child, width) + 4.0;
            return height;
        }
        default:
            return 0.0;
        }
    }

    double layoutNode(const v8reader::FormItem &item, const QRectF &rect)
    {
        using Type = v8reader::FormItem::Type;
        if (rect.width() <= 1.0)
            return 0.0;

        switch (item.type) {
        case Type::Field: {
            PreviewControl control;
            control.kind = PreviewControl::Kind::Field;
            control.rect = rect;
            control.title = item.title();
            control.text = item.name == item.synonym ? QString() : item.name;
            add(control);
            return rect.height();
        }
        case Type::Button: {
            PreviewControl control;
            control.kind = PreviewControl::Kind::Button;
            control.rect = QRectF(rect.left(), rect.top(), rect.width(), 26.0);
            control.title = item.title();
            add(control);
            return 26.0;
        }
        case Type::Table: {
            PreviewControl control;
            control.kind = PreviewControl::Kind::Table;
            control.rect = rect;
            control.title = item.title();
            collectTableFields(item, &control.columns);
            add(control);
            return rect.height();
        }
        case Type::Decoration: {
            if (!item.synonym.isEmpty()) {
                PreviewControl control;
                control.kind = PreviewControl::Kind::Decoration;
                control.rect = QRectF(rect.left(), rect.top(), rect.width(), 18.0);
                control.title = item.synonym;
                add(control);
                return 18.0;
            }
            return 0.0;
        }
        case Type::Group:
            return layoutGroup(item, rect);
        default:
            return 0.0;
        }
    }

    double layoutGroup(const v8reader::FormItem &item, const QRectF &rect)
    {
        QVector<const v8reader::FormItem *> children;
        for (const v8reader::FormItem &child : item.children) {
            if (!isServiceItem(child) && !isServiceDecoration(child))
                children.append(&child);
        }
        if (children.isEmpty())
            return 0.0;

        if (item.groupKind == 3) {
            double x = rect.left();
            double y = rect.top();
            for (const v8reader::FormItem *child : children) {
                if (child->type != v8reader::FormItem::Type::Group)
                    continue;
                PreviewControl tab;
                tab.kind = PreviewControl::Kind::PageTab;
                tab.rect = QRectF(x, y, 120.0, 22.0);
                tab.title = child->title();
                add(tab);
                x += 122.0;
            }
            const v8reader::FormItem *firstPage = nullptr;
            for (const v8reader::FormItem *child : children) {
                if (child->type == v8reader::FormItem::Type::Group) {
                    firstPage = child;
                    break;
                }
            }
            if (!firstPage)
                return 22.0;
            const QRectF pageRect(rect.left(), rect.top() + 26.0,
                                  rect.width(), rect.height() - 26.0);
            return 26.0 + layoutGroup(*firstPage, pageRect);
        }

        const bool horizontal = item.groupKind == 9 || item.groupKind == 6
            || item.orientation == 1;

        if (horizontal) {
            QVector<double> widths;
            double total = 0.0;
            for (const v8reader::FormItem *child : children) {
                const double width = preferredWidth(*child, 0);
                widths.append(width);
                total += width + 6.0;
            }
            const double available = std::max(1.0, rect.width() - 6.0 * children.size());
            const double scale = total > 0.0 && total > rect.width()
                ? std::max(0.25, available / total) : 1.0;

            double height = 0.0;
            double x = rect.left();
            for (int i = 0; i < children.size(); ++i) {
                const double width = std::max(40.0, widths.at(i) * scale);
                height = std::max(height, naturalHeight(*children.at(i), width));
            }
            for (int i = 0; i < children.size(); ++i) {
                const double width = std::max(40.0, widths.at(i) * scale);
                layoutNode(*children.at(i),
                           QRectF(x, rect.top(), width, height));
                x += width + 6.0;
            }
            return height;
        }

        double y = rect.top();
        for (const v8reader::FormItem *child : children) {
            const double height = naturalHeight(*child, rect.width());
            if (height <= 0.0)
                continue;
            layoutNode(*child, QRectF(rect.left(), y, rect.width(), height));
            y += height + 4.0;
        }
        return y - rect.top();
    }

    QVector<PreviewControl> controls;
};

void paintControl(QPainter &painter, const PreviewControl &control,
                  const QFont &font, const QFontMetricsF &metrics)
{
    painter.setFont(font);
    const QRectF rect = control.rect;
    switch (control.kind) {
    case PreviewControl::Kind::Field: {
        const double labelWidth = std::min(rect.width() * 0.55,
            std::max(80.0, metrics.horizontalAdvance(control.title) + 10.0));
        const QRectF labelRect(rect.left(), rect.top(), labelWidth, rect.height());
        painter.setPen(QColor(60, 60, 60));
        painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, control.title);

        const QRectF editRect(labelRect.right() + 4.0, rect.top() + 3.0,
                              std::max(30.0, rect.width() - labelWidth - 8.0),
                              rect.height() - 6.0);
        painter.setPen(QColor(170, 170, 170));
        painter.setBrush(QColor(255, 255, 255));
        painter.drawRoundedRect(editRect, 2.0, 2.0);
        painter.setBrush(Qt::NoBrush);
        if (!control.text.isEmpty()) {
            painter.setPen(QColor(150, 150, 150));
            painter.drawText(editRect.adjusted(4, 0, -4, 0),
                             Qt::AlignLeft | Qt::AlignVCenter, control.text);
        }
        break;
    }
    case PreviewControl::Kind::Button: {
        painter.setPen(QColor(120, 120, 120));
        painter.setBrush(QColor(240, 240, 240));
        painter.drawRoundedRect(rect, 3.0, 3.0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor(40, 40, 40));
        painter.drawText(rect.adjusted(4, 0, -4, 0),
                         Qt::AlignCenter, control.title);
        break;
    }
    case PreviewControl::Kind::Table: {
        painter.setPen(QColor(150, 150, 150));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect);
        QStringList columns = control.columns;
        if (columns.isEmpty())
            columns << QStringLiteral("Колонка");
        const int visible = std::min(10, int(columns.size()));
        const double columnWidth = rect.width() / visible;
        const double headerHeight = 28.0;
        const QRectF headerRect(rect.left(), rect.top(), rect.width(), headerHeight);
        painter.fillRect(headerRect, QColor(242, 242, 242));
        painter.drawLine(headerRect.bottomLeft(), headerRect.bottomRight());
        painter.setPen(QColor(40, 40, 40));
        QFont headerFont = font;
        headerFont.setBold(true);
        painter.setFont(headerFont);
        for (int i = 0; i < visible; ++i) {
            const QRectF cell(rect.left() + i * columnWidth, rect.top(),
                              columnWidth, headerHeight);
            painter.drawRect(cell);
            painter.drawText(cell.adjusted(4, 1, -4, -1),
                             Qt::AlignLeft | Qt::AlignVCenter, columns.at(i));
        }
        painter.setFont(font);
        painter.setPen(QColor(150, 150, 150));
        const double rowHeight = 18.0;
        const int rows = std::max(1, int((rect.height() - headerHeight) / rowHeight));
        for (int row = 0; row < rows && row < 6; ++row) {
            for (int i = 0; i < visible; ++i) {
                const QRectF cell(rect.left() + i * columnWidth,
                                  rect.top() + headerHeight + row * rowHeight,
                                  columnWidth, rowHeight);
                painter.drawRect(cell);
            }
        }
        break;
    }
    case PreviewControl::Kind::PageTab: {
        painter.setPen(QColor(120, 120, 120));
        painter.setBrush(QColor(245, 245, 245));
        painter.drawRect(rect);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor(40, 40, 40));
        painter.drawText(rect.adjusted(4, 0, -4, 0), Qt::AlignCenter, control.title);
        break;
    }
    case PreviewControl::Kind::Decoration: {
        painter.setPen(QColor(80, 80, 80));
        painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, control.title);
        break;
    }
    }
}

} // namespace

// Схематичное представление формы: поля/кнопки/таблицы/группы.
class FormPreviewWidget : public QWidget
{
public:
    explicit FormPreviewWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedWidth(760);
        setMinimumHeight(240);
        setAutoFillBackground(true);
        QPalette palette = this->palette();
        palette.setColor(QPalette::Window, Qt::white);
        setPalette(palette);
    }

    void setDocument(const v8reader::FormDocument *document)
    {
        m_document = document;
        updateSize();
    }

    void clear()
    {
        m_document = nullptr;
        updateSize();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), Qt::white);

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
        painter.drawText(QRectF(20, 16, width() - 40, 28),
                         Qt::AlignHCenter | Qt::AlignVCenter, title);

        QFont subtitleFont(QStringLiteral("Arial"), 8);
        painter.setFont(subtitleFont);
        painter.setPen(QColor(130, 130, 130));
        painter.drawText(QRectF(20, 40, width() - 40, 18),
                         Qt::AlignHCenter | Qt::AlignVCenter,
                         QStringLiteral("управляемая форма · %1 элементов · %2 полей · %3 команд")
                             .arg(m_document->itemCount())
                             .arg(m_document->fieldCount())
                             .arg(m_document->commands.size()));

        LayoutContext context;
        const QRectF area(20.0, 70.0, width() - 40.0, height() - 90.0);
        double y = area.top();
        for (const v8reader::FormItem &item : m_document->items) {
            if (isServiceItem(item) || isServiceDecoration(item))
                continue;
            const double height = context.naturalHeight(item, area.width());
            if (height <= 0.0)
                continue;
            context.layoutNode(item, QRectF(area.left(), y, area.width(), height));
            y += height + 6.0;
        }

        for (const PreviewControl &control : context.controls)
            paintControl(painter, control, font, metrics);

        if (context.controls.isEmpty()) {
            painter.setFont(subtitleFont);
            painter.setPen(QColor(140, 140, 140));
            painter.drawText(area, Qt::AlignHCenter | Qt::AlignTop,
                             QStringLiteral("Элементы формы не найдены"));
        }
    }

private:
    void updateSize()
    {
        if (!m_document || m_document->items.isEmpty()) {
            setFixedHeight(240);
            update();
            return;
        }
        LayoutContext context;
        const double width = 720.0;
        double y = 0.0;
        for (const v8reader::FormItem &item : m_document->items) {
            if (isServiceItem(item) || isServiceDecoration(item))
                continue;
            const double height = context.naturalHeight(item, width);
            if (height > 0.0)
                y += height + 6.0;
        }
        setFixedHeight(qBound(240, int(y) + 100, 6000));
        update();
    }

    const v8reader::FormDocument *m_document = nullptr;
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
    scroll->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_preview = new FormPreviewWidget(scroll);
    scroll->setWidget(m_preview);

    splitter->addWidget(m_tree);
    splitter->addWidget(scroll);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({380, 620});
    layout->addWidget(splitter, 1);

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
