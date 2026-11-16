#include "gui/FormView.h"

#include <QComboBox>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>
#include <QSizePolicy>
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
    double labelWidth = 0.0;
    bool selectedTab = false;
};

bool isServiceItem(const v8reader::FormItem &item)
{
    using Type = v8reader::FormItem::Type;
    if (item.type == Type::AutoCommandBar)
        return true;
    if (item.type == Type::Group
        && (item.groupKind == 8 || item.groupKind == 1))
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

QString splitCamelCase(const QString &text)
{
    QString result;
    QString word;
    auto flush = [&]() {
        if (word.isEmpty())
            return;
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
    if (!result.isEmpty())
        result[0] = result.at(0).toUpper();
    return result;
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
    return splitCamelCase(text);
}

class LayoutEngine
{
public:
    LayoutEngine(const v8reader::FormDocument &document, double width)
        : m_document(document), m_width(width),
          m_font(QStringLiteral("Arial"), 9), m_metrics(m_font)
    {
        for (const v8reader::FormAttribute &attribute : document.attributes) {
            if (!attribute.name.isEmpty())
                m_attributeTitles.insert(attribute.name, attribute.synonym);
        }
    }

    void build(QVector<PreviewControl> *controls, double *contentHeight)
    {
        m_controls = controls;
        const double top = 0.0;
        QVector<const v8reader::FormItem *> items;
        for (const v8reader::FormItem &item : m_document.items) {
            if (!isServiceItem(item))
                items.append(&item);
        }
        const double height = layoutVertical(items, QRectF(0.0, top, m_width, 100000.0), 0);
        *contentHeight = height;
    }

private:
    QString fieldLabel(const v8reader::FormItem &item) const
    {
        QString title = item.synonym;
        if (title.isEmpty())
            title = m_attributeTitles.value(item.name);
        if (title.isEmpty())
            title = humanizeCommand(item.name);
        return title;
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
        case Type::Field: {
            const double label = labelWidth(item);
            return std::max(170.0, label + 160.0);
        }
        case Type::Button: {
            const QString title = item.synonym.isEmpty()
                ? humanizeCommand(item.name) : item.synonym;
            return std::min(220.0, m_metrics.horizontalAdvance(title) + 34.0);
        }
        case Type::Table:
            return 340.0;
        case Type::Decoration:
            return std::max(40.0, m_metrics.horizontalAdvance(item.synonym) + 10.0);
        case Type::Group: {
            if (isHorizontal(item)) {
                double width = 0.0;
                int count = 0;
                for (const v8reader::FormItem &child : item.children) {
                    if (isServiceItem(child) || isServiceDecoration(child))
                        continue;
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

    bool isHorizontal(const v8reader::FormItem &item) const
    {
        return item.type == v8reader::FormItem::Type::Group
            && (item.groupKind == 9 || item.groupKind == 6 || item.orientation == 1);
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
            return 27.0;
        case Type::Decoration:
            return item.synonym.isEmpty() ? 0.0 : 19.0;
        case Type::Table:
            return 230.0;
        case Type::Group: {
            if (isPages(item)) {
                return 26.0 + pageContentHeight(item, width);
            }
            QVector<const v8reader::FormItem *> children;
            visibleChildren(item, &children);
            if (children.isEmpty())
                return 0.0;
            if (isHorizontal(item)) {
                double height = 0.0;
                for (const v8reader::FormItem *child : children)
                    height = std::max(height, naturalHeight(*child, width));
                return height;
            }
            double height = 0.0;
            for (const v8reader::FormItem *child : children)
                height += naturalHeight(*child, width) + 5.0;
            return height;
        }
        default:
            return 0.0;
        }
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

    double layoutVertical(const QVector<const v8reader::FormItem *> &items,
                          const QRectF &rect, int depth)
    {
        // Подписи полей в колонке выравниваются по самой длинной (как в 1С).
        double commonLabel = 0.0;
        for (const v8reader::FormItem *item : items) {
            if (item->type == v8reader::FormItem::Type::Field)
                commonLabel = std::max(commonLabel, labelWidth(*item));
        }

        double y = rect.top();
        for (const v8reader::FormItem *item : items) {
            const double height = naturalHeight(*item, rect.width());
            if (height <= 0.0)
                continue;
            if (item->type == v8reader::FormItem::Type::Field) {
                layoutField(*item, QRectF(rect.left(), y, rect.width(), 27.0),
                            commonLabel);
                y += 27.0 + 5.0;
            } else {
                layoutNode(*item, QRectF(rect.left(), y, rect.width(), height), depth + 1);
                y += height + 5.0;
            }
        }
        return y - rect.top();
    }

    double layoutHorizontal(const QVector<const v8reader::FormItem *> &items,
                            const QRectF &rect, int depth)
    {
        if (items.isEmpty())
            return 0.0;

        QVector<double> widths;
        widths.reserve(items.size());
        double total = 0.0;
        for (const v8reader::FormItem *item : items) {
            const double width = preferredWidth(*item);
            widths.append(width);
            total += width;
        }
        total += 8.0 * (items.size() - 1);

        double scale = 1.0;
        if (total > rect.width() && total > 0.0)
            scale = rect.width() / total;

        double height = 0.0;
        for (int i = 0; i < items.size(); ++i)
            height = std::max(height, naturalHeight(*items.at(i), widths.at(i) * scale));

        double x = rect.left();
        for (int i = 0; i < items.size(); ++i) {
            const double width = std::max(34.0, widths.at(i) * scale);
            layoutNode(*items.at(i), QRectF(x, rect.top(), width, height), depth + 1);
            x += width + 8.0;
        }
        return height;
    }

    void layoutNode(const v8reader::FormItem &item, const QRectF &rect, int depth)
    {
        using Type = v8reader::FormItem::Type;
        if (depth > 40)
            return;

        switch (item.type) {
        case Type::Field:
            layoutField(item, rect, 0.0, false);
            return;
        case Type::Button:
            layoutButton(item, QRectF(rect.left(), rect.top(),
                                      std::min(rect.width(), preferredWidth(item)),
                                      27.0));
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
            QVector<const v8reader::FormItem *> children;
            visibleChildren(item, &children);
            if (children.isEmpty())
                return;

            if (isPages(item)) {
                layoutPages(item, rect);
                return;
            }
            if (isHorizontal(item)) {
                layoutHorizontal(children, rect, depth);
                return;
            }
            layoutVertical(children, rect, depth);
            return;
        }
        default:
            return;
        }
    }

    void layoutField(const v8reader::FormItem &item, const QRectF &rect,
                     double forcedLabelWidth, bool align = true)
    {
        const QString label = fieldLabel(item);
        double labelW = forcedLabelWidth;
        if (labelW <= 0.0)
            labelW = labelWidth(item);
        if (!align)
            labelW = std::min(labelW, rect.width() * 0.45);
        labelW = std::min(labelW, rect.width() * 0.6);

        PreviewControl control;
        control.kind = PreviewControl::Kind::Field;
        control.rect = rect;
        control.title = label;
        control.labelWidth = labelW;
        m_controls->append(control);
    }

    void layoutButton(const v8reader::FormItem &item, const QRectF &rect)
    {
        PreviewControl control;
        control.kind = PreviewControl::Kind::Button;
        control.rect = rect.adjusted(0.0, 0.0, 0.0, 0.0);
        control.title = item.synonym.isEmpty()
            ? humanizeCommand(item.name) : item.synonym;
        m_controls->append(control);
    }

    int collectTableFields(const v8reader::FormItem &item, int *count) const
    {
        int result = 0;
        for (const v8reader::FormItem &child : item.children) {
            if (isServiceItem(child) || isServiceDecoration(child))
                continue;
            if (child.type == v8reader::FormItem::Type::Field) {
                ++result;
                ++(*count);
            } else {
                result += collectTableFields(child, count);
            }
        }
        return result;
    }

    void tableColumns(const v8reader::FormItem &item, QStringList *columns) const
    {
        for (const v8reader::FormItem &child : item.children) {
            if (isServiceItem(child) || isServiceDecoration(child))
                continue;
            if (child.type == v8reader::FormItem::Type::Field) {
                columns->append(fieldLabel(child));
            } else if (child.type == v8reader::FormItem::Type::Group) {
                tableColumns(child, columns);
            }
        }
    }

    void layoutTable(const v8reader::FormItem &item, const QRectF &rect)
    {
        PreviewControl control;
        control.kind = PreviewControl::Kind::Table;
        control.rect = QRectF(rect.left(), rect.top(), rect.width(),
                              std::min(rect.height(), 230.0));
        control.title = item.synonym;
        tableColumns(item, &control.columns);
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
    QHash<QString, QString> m_attributeTitles;
    QVector<PreviewControl> *m_controls = nullptr;
};

void paintControl(QPainter &painter, const PreviewControl &control,
                  const QFont &font, const QFontMetricsF &metrics)
{
    painter.setFont(font);
    const QRectF rect = control.rect;

    switch (control.kind) {
    case PreviewControl::Kind::Field: {
        const QString label = control.title;
        if (!label.isEmpty()) {
            double labelWidth = control.labelWidth;
            if (labelWidth <= 0.0) {
                labelWidth = std::min(rect.width() * 0.6,
                    std::max(40.0, metrics.horizontalAdvance(label + QStringLiteral(":")) + 6.0));
            }
            const QRectF labelRect(rect.left(), rect.top(), labelWidth, rect.height());
            painter.setPen(QColor(50, 50, 50));
            painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter,
                             label + QStringLiteral(":"));
            const QRectF edit(labelRect.right() + 4.0, rect.top() + 3.0,
                              std::max(24.0, rect.width() - labelWidth - 8.0),
                              rect.height() - 6.0);
            painter.setPen(QColor(165, 165, 165));
            painter.setBrush(Qt::white);
            painter.drawRoundedRect(edit, 2.0, 2.0);
            painter.setBrush(Qt::NoBrush);
        } else {
            const QRectF edit(rect.left(), rect.top() + 3.0,
                              rect.width(), rect.height() - 6.0);
            painter.setPen(QColor(165, 165, 165));
            painter.setBrush(Qt::white);
            painter.drawRoundedRect(edit, 2.0, 2.0);
            painter.setBrush(Qt::NoBrush);
        }
        break;
    }
    case PreviewControl::Kind::Button: {
        painter.setPen(QColor(125, 125, 125));
        painter.setBrush(QColor(240, 240, 240));
        painter.drawRoundedRect(rect, 3.0, 3.0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor(35, 35, 35));
        const QString text = metrics.elidedText(control.title, Qt::ElideRight,
                                                int(rect.width()) - 8);
        painter.drawText(rect.adjusted(4, 0, -4, 0), Qt::AlignCenter, text);
        break;
    }
    case PreviewControl::Kind::Table: {
        const QStringList columns = control.columns;
        if (columns.isEmpty())
            return;
        const int visible = std::min(10, int(columns.size()));
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
// таблицы и командные панели.
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
        painter.drawText(QRectF(20, 14, width() - 40, 28),
                         Qt::AlignHCenter | Qt::AlignVCenter, title);

        QFont subtitleFont(QStringLiteral("Arial"), 8);
        painter.setFont(subtitleFont);
        painter.setPen(QColor(130, 130, 130));
        painter.drawText(QRectF(20, 40, width() - 40, 16),
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
