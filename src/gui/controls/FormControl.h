#pragma once

#include <QPainter>
#include <QRectF>

namespace controls {

// Базовый контрол псевдофреймворка: хранит геометрию и умеет рисовать себя.
// Наследники добавляют состояние и отрисовку конкретного элемента 1С.
class FormControl
{
public:
    virtual ~FormControl() = default;

    const QRectF &bounds() const { return m_bounds; }
    void setBounds(const QRectF &bounds) { m_bounds = bounds; }

    bool hitTest(const QPointF &point) const { return m_bounds.contains(point); }

    virtual void paint(QPainter &painter) const = 0;

private:
    QRectF m_bounds;
};

} // namespace controls
