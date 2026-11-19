#include "FieldControl.h"

#include "FormStyle.h"

#include <QLinearGradient>
#include <QPen>

#include <algorithm>

namespace controls {

void paintField(QPainter &painter, const FieldVisual &visual,
                const QFontMetricsF &metrics)
{
    const QRectF &rect = visual.rect;
    const QString &label = visual.label;
    if (visual.labelOnly) {
        if (label.isEmpty())
            return;
        const bool link = label.contains(QStringLiteral("Цен"), Qt::CaseInsensitive)
            || label.contains(QStringLiteral("НДС"), Qt::CaseInsensitive)
            || label.contains(QStringLiteral("Настрой"), Qt::CaseInsensitive)
            || label.contains(QStringLiteral("Изменить"), Qt::CaseInsensitive);
        painter.setPen(link ? QColor(0, 102, 204) : FormStyle::text);
        painter.drawText(rect.adjusted(2, 0, -2, 0),
                         Qt::AlignLeft | Qt::AlignVCenter, label);
        return;
    }

    double x = rect.left();
    if (!label.isEmpty()) {
        const double labelWidth = std::min(visual.labelWidth > 0.0
                ? visual.labelWidth
                : std::max(40.0, metrics.horizontalAdvance(label + QStringLiteral(":"))
                    + 6.0),
            rect.width() * 0.7);
        const QRectF labelRect(rect.left(), rect.top(),
                               std::max(0.0, labelWidth), rect.height());
        painter.setPen(FormStyle::text);
        if (visual.checkBox) {
            painter.drawText(labelRect.adjusted(0, 0, -2, 0),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             label + QStringLiteral(":"));
            x = labelRect.right() + 2.0;
        } else {
            painter.drawText(labelRect.adjusted(0, 0, -4, 0),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             label + QStringLiteral(":"));
            x = labelRect.right() + 3.0;
        }
    }
    if (visual.checkBox) {
        const QRectF box(x + 1.0, rect.top() + (rect.height() - 13.0) / 2.0,
                         13.0, 13.0);
        painter.setPen(FormStyle::fieldBorder);
        painter.setBrush(Qt::white);
        painter.drawRect(box);
        painter.setBrush(Qt::NoBrush);
        x += 19.0;
        if (visual.checkboxOnly)
            return;
    }
    double editWidth = std::max(24.0, rect.right() - x - 4.0);
    if (visual.inputWidth > 0.0)
        editWidth = std::min(editWidth, visual.inputWidth);
    const QRectF edit(x, rect.top() + 2.0, editWidth, rect.height() - 4.0);
    painter.setPen(FormStyle::fieldBorder);
    painter.setBrush(Qt::white);
    painter.drawRect(edit);
    painter.setBrush(Qt::NoBrush);
    const bool fieldButton = visual.dropDown || visual.calendar
        || visual.choiceButton;
    if (fieldButton && edit.width() > 44.0) {
        const double buttonWidth = 15.0;
        const QRectF button(edit.right() - buttonWidth, edit.top() + 1.0,
                            buttonWidth - 1.0, edit.height() - 2.0);
        if (visual.dropDown) {
            painter.setPen(QColor(160, 160, 160));
            painter.setBrush(Qt::white);
            painter.drawRect(button);
        } else {
            QLinearGradient gradient(button.topLeft(), button.bottomLeft());
            gradient.setColorAt(0.0, QColor(251, 251, 247));
            gradient.setColorAt(1.0, QColor(236, 231, 216));
            painter.setPen(QColor(165, 165, 165));
            painter.setBrush(gradient);
            painter.drawRect(button);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor(80, 80, 80));
        const QPointF center = button.center();
        if (visual.dropDown) {
            painter.drawLine(center + QPointF(-4, -2), center + QPointF(0, 2));
            painter.drawLine(center + QPointF(0, 2), center + QPointF(4, -2));
        } else if (visual.calendar) {
            const QRectF cal(center.x() - 4.5, center.y() - 4.5, 9.0, 9.0);
            painter.drawRect(QRectF(cal.left(), cal.top() + 2.0,
                                    cal.width(), cal.height() - 2.0));
            painter.drawLine(QPointF(cal.left(), cal.top() + 4.0),
                             QPointF(cal.right(), cal.top() + 4.0));
            painter.drawLine(QPointF(cal.left() + 2.0, cal.top()),
                             QPointF(cal.left() + 2.0, cal.top() + 3.0));
            painter.drawLine(QPointF(cal.right() - 2.0, cal.top()),
                             QPointF(cal.right() - 2.0, cal.top() + 3.0));
        } else {
            painter.setBrush(QColor(80, 80, 80));
            for (int i = 0; i < 3; ++i) {
                painter.drawEllipse(QPointF(center.x() - 4.0 + i * 4.0,
                                           center.y()), 1.2, 1.2);
            }
            painter.setBrush(Qt::NoBrush);
        }
    }
}

} // namespace controls
