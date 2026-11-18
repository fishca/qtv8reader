#include "gui/PropertiesPanel.h"

#include <QHeaderView>
#include <QTableWidgetItem>

namespace v8reader {
namespace {

QTableWidgetItem *readOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setToolTip(text);
    return item;
}

} // namespace

PropertiesPanel::PropertiesPanel(QWidget *parent)
    : QTableWidget(0, 2, parent)
{
    setObjectName(QStringLiteral("propertiesTable"));
    setHorizontalHeaderLabels({QStringLiteral("Свойство"), QStringLiteral("Значение")});
    verticalHeader()->setVisible(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setWordWrap(true);
    setAlternatingRowColors(true);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void PropertiesPanel::setRows(const QVector<QPair<QString, QString>> &rows)
{
    setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        setItem(i, 0, readOnlyItem(rows.at(i).first));
        setItem(i, 1, readOnlyItem(rows.at(i).second));
    }
}

} // namespace v8reader
