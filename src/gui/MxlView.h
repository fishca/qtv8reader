#ifndef QTV8READER_MXLVIEW_H
#define QTV8READER_MXLVIEW_H

#include "gui/MxlRenderer.h"

#include <QAbstractScrollArea>
#include <QString>

namespace v8reader {

// Просмотр табличного документа 1С (MXL): сетка, объединения, шрифты,
// цвета, рамки, выравнивание, перенос и поворот текста, именованные области.
// Масштаб — Ctrl + колесо.
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

    const mxl::Document &document() const { return m_renderer.document(); }

    void setMessage(const QString &message);
    QString message() const { return m_message; }

    bool showNamedAreas() const { return m_showAreas; }
    void setShowNamedAreas(bool show);
    void setActiveNamedArea(const QString &name);

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
    void updateScrollBars();

    MxlRenderer m_renderer;
    bool m_hasDocument = false;
    QString m_message;
    bool m_showAreas = false;
    QString m_activeArea;
    double m_zoom = 1.0;
};

} // namespace v8reader

#endif // QTV8READER_MXLVIEW_H
