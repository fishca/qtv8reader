#include "gui/MxlView.h"

#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace v8reader {

MxlView::MxlView(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setBackgroundRole(QPalette::Base);
    viewport()->setBackgroundRole(QPalette::Base);
    viewport()->setAutoFillBackground(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setMessage(QStringLiteral("Табличный документ не выбран"));
}

void MxlView::setMessage(const QString &message)
{
    m_message = message;
    viewport()->update();
}

void MxlView::clear()
{
    m_renderer.setDocument(mxl::Document());
    m_hasDocument = false;
    m_message = QStringLiteral("Табличный документ не выбран");
    m_activeArea.clear();
    updateScrollBars();
    viewport()->update();
}

void MxlView::setDocument(const mxl::Document &document)
{
    m_renderer.setDocument(document);
    m_hasDocument = document.valid;
    m_message.clear();
    m_activeArea.clear();
    updateScrollBars();
    viewport()->update();
}

int MxlView::rowCount() const
{
    return m_hasDocument ? m_renderer.rowCount() : 0;
}

int MxlView::columnCount() const
{
    return m_hasDocument ? m_renderer.columnCount() : 0;
}

void MxlView::setShowNamedAreas(bool show)
{
    if (m_showAreas == show)
        return;
    m_showAreas = show;
    viewport()->update();
}

void MxlView::setActiveNamedArea(const QString &name)
{
    if (m_activeArea == name)
        return;
    m_activeArea = name;
    viewport()->update();
}

void MxlView::setZoom(double zoom)
{
    const double clamped = std::clamp(zoom, 0.2, 5.0);
    if (std::abs(clamped - m_zoom) < 0.0001)
        return;
    m_zoom = clamped;
    updateScrollBars();
    viewport()->update();
}

void MxlView::zoomIn()
{
    setZoom(m_zoom * 1.25);
}

void MxlView::zoomOut()
{
    setZoom(m_zoom / 1.25);
}

void MxlView::resetZoom()
{
    setZoom(1.0);
}

void MxlView::updateScrollBars()
{
    if (m_hasDocument) {
        const double width = m_renderer.contentWidth();
        const double height = m_renderer.contentHeight();
        horizontalScrollBar()->setRange(
            0, std::max(0, static_cast<int>(std::ceil(width * m_zoom))
                               - viewport()->width()));
        verticalScrollBar()->setRange(
            0, std::max(0, static_cast<int>(std::ceil(height * m_zoom))
                               - viewport()->height()));
        horizontalScrollBar()->setPageStep(viewport()->width());
        verticalScrollBar()->setPageStep(viewport()->height());
        horizontalScrollBar()->setSingleStep(20);
        verticalScrollBar()->setSingleStep(20);
    } else {
        horizontalScrollBar()->setRange(0, 0);
        verticalScrollBar()->setRange(0, 0);
    }
}

void MxlView::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBars();
}

void MxlView::wheelEvent(QWheelEvent *event)
{
    if ((event->modifiers() & Qt::ControlModifier) != 0) {
        const int delta = event->angleDelta().y();
        if (delta > 0)
            zoomIn();
        else if (delta < 0)
            zoomOut();
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

void MxlView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), Qt::white);

    if (!m_hasDocument) {
        if (!m_message.isEmpty()) {
            painter.setPen(QColor(120, 120, 120));
            painter.drawText(viewport()->rect().adjusted(16, 16, -16, -16),
                             Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
                             m_message);
        }
        return;
    }

    const double zoom = m_zoom;
    const QPointF offset(horizontalScrollBar()->value() / zoom,
                         verticalScrollBar()->value() / zoom);
    const QRectF visible(offset,
                         QSizeF(viewport()->width() / zoom,
                                viewport()->height() / zoom));

    painter.save();
    painter.scale(zoom, zoom);
    painter.translate(-offset);
    painter.setClipRect(visible);
    m_renderer.draw(painter, visible, m_showAreas, m_activeArea);
    painter.restore();
}

} // namespace v8reader
