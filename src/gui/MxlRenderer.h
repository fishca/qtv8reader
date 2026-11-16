#ifndef QTV8READER_MXLRENDERER_H
#define QTV8READER_MXLRENDERER_H

#include "core/MxlDocument.h"

#include <QFont>
#include <QHash>
#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;

namespace v8reader {

// Раскладка и отрисовка табличного документа MXL в координатах документа
// (пиксели при 96 dpi). Используется и просмотром (MxlView), и печатью.
class MxlRenderer
{
public:
    MxlRenderer() = default;
    explicit MxlRenderer(const mxl::Document &document);

    void setDocument(const mxl::Document &document);
    const mxl::Document &document() const { return m_document; }
    bool isValid() const { return m_document.valid; }

    int rowCount() const;
    int columnCount() const;
    int maxRowIndex() const;

    double contentWidth() const { return m_contentWidth; }
    double contentHeight() const { return m_contentHeight; }

    double columnLeft(int column) const;
    double columnWidth(int column) const;
    double rowTop(int rowIndex) const;
    double rowHeight(int rowIndex) const;

    QFont baseFont() const;

    // Рисует часть документа, попадающую в visible (координаты документа).
    // Painter уже должен быть трансформирован в координаты документа.
    void draw(QPainter &painter, const QRectF &visible,
              bool showNamedAreas = false,
              const QString &activeArea = QString()) const;

    // Прямоугольник именованной области в координатах документа.
    QRectF namedAreaRect(const mxl::NamedArea &area) const;

private:
    void rebuildLayout();
    QFont fontForFormat(const mxl::CellFormat *format) const;
    QColor colorForIndex(int index, const QColor &fallback) const;
    void drawCell(QPainter &painter, int rowIndex, int column,
                  const mxl::Merge *merge, const mxl::Cell *cell,
                  const mxl::CellFormat *format) const;
    void drawNamedAreas(QPainter &painter, const QRectF &visible,
                        const QString &activeArea) const;

    mxl::Document m_document;
    QVector<double> m_columnLeft;
    QHash<int, double> m_rowTop;
    QHash<int, double> m_rowHeight;
    double m_contentWidth = 0.0;
    double m_contentHeight = 0.0;
    int m_maxColumn = 0;
    int m_maxRowIndex = 0;
};

} // namespace v8reader

#endif // QTV8READER_MXLRENDERER_H
