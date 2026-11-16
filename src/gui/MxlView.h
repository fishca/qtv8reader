#ifndef QTV8READER_MXLVIEW_H
#define QTV8READER_MXLVIEW_H

#include "core/MxlDocument.h"

#include <QAbstractScrollArea>
#include <QFont>
#include <QHash>
#include <QVector>

class QPainter;

namespace v8reader {

// Просмотр табличного документа 1С (MXL): сетка, объединения, шрифты,
// цвета, рамки, выравнивание и перенос текста. Масштаб — Ctrl + колесо.
class MxlView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit MxlView(QWidget *parent = nullptr);

    void setDocument(const mxl::Document &document);
    void clear();
    bool hasDocument() const { return m_hasDocument; }

    int rowCount() const;
    int columnCount() const;

    void setMessage(const QString &message);
    QString message() const { return m_message; }

    double zoom() const { return m_zoom; }
    void setZoom(double zoom);

public slots:
    void zoomIn();
    void zoomOut();
    void resetZoom();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void rebuildLayout();
    void updateScrollBars();
    void drawCell(QPainter &painter, int rowIndex, int column,
                  const mxl::Merge *merge, const mxl::Cell *cell,
                  const mxl::CellFormat *format);

    QFont fontForFormat(const mxl::CellFormat *format) const;
    QFont baseFont() const;
    QColor colorForIndex(int index, const QColor &fallback) const;

    double columnUnits(int column) const;
    double rowUnits(const mxl::Row &row) const;
    double cellPixelWidth(int column) const;
    double cellPixelHeight(int rowIndex) const;

    mxl::Document m_document;
    bool m_hasDocument = false;
    QString m_message;
    double m_zoom = 1.0;

    QVector<double> m_columnLeft;   // левые границы колонок (+ sentinel)
    QHash<int, double> m_rowTop;    // верхние границы строк по номеру строки
    QHash<int, double> m_rowHeight;
    double m_contentWidth = 0.0;
    double m_contentHeight = 0.0;
    int m_maxColumn = 0;
};

} // namespace v8reader

#endif // QTV8READER_MXLVIEW_H
