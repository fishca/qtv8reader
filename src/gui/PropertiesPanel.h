#ifndef QTV8READER_PROPERTIESPANEL_H
#define QTV8READER_PROPERTIESPANEL_H

#include <QTableWidget>
#include <QVector>
#include <QPair>

namespace v8reader {

// Таблица «Свойства»: только чтение, две колонки.
class PropertiesPanel : public QTableWidget
{
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget *parent = nullptr);

    void setRows(const QVector<QPair<QString, QString>> &rows);
};

} // namespace v8reader

#endif // QTV8READER_PROPERTIESPANEL_H
