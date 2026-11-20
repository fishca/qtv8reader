#include "FieldControl.h"

#include "FormStyle.h"

#include <QLinearGradient>
#include <QPen>

#include <algorithm>

namespace controls {

namespace {

// Поле ввода: рамка, белый фон и кнопка выбора/календаря справа.
void drawInput(QPainter &painter, const QRectF &rect, const FieldVisual &visual)
{
    double width = std::max(24.0, rect.width());
    if (visual.inputWidth > 0.0)
        width = std::min(width, visual.inputWidth);
    const QRectF edit(rect.left(), rect.top(), width, rect.height());
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

} // namespace

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
        if (link) {
            QFont font = painter.font();
            font.setUnderline(true);
            painter.setFont(font);
        }
        painter.setPen(link ? FormStyle::link : FormStyle::text);
        painter.drawText(rect.adjusted(2, 0, -2, 0),
                         Qt::AlignLeft | Qt::AlignVCenter, label);
        return;
    }

    double x = rect.left();
    // Флажок перед подписью: 1С рисует булево поле как «[ ] Заголовок».
    if (visual.checkBox && visual.checkBoxLeading) {
        const QRectF box(x + 1.0,
                         rect.top() + (rect.height() - FormStyle::checkBoxSize) / 2.0,
                         FormStyle::checkBoxSize, FormStyle::checkBoxSize);
        painter.setPen(FormStyle::fieldBorder);
        painter.setBrush(Qt::white);
        painter.drawRect(box);
        painter.setBrush(Qt::NoBrush);
        x += FormStyle::checkBoxSize + 6.0;
        if (!label.isEmpty()) {
            painter.setPen(FormStyle::text);
            painter.drawText(QRectF(x, rect.top(), rect.right() - x, rect.height()),
                             Qt::AlignLeft | Qt::AlignVCenter, label);
        }
        return;
    }

    const int placement = visual.labelPlacement;
    const bool stacked = placement == 1 || placement == 3;

    QRectF labelRect;
    double labelColumn = 0.0;
    if (!stacked && !label.isEmpty()) {
        labelColumn = std::min(visual.labelWidth > 0.0
                ? visual.labelWidth
                : std::max(40.0, metrics.horizontalAdvance(label + QStringLiteral(":"))
                    + 6.0),
            rect.width() * 0.7);
    }

    if (placement == 1) {
        // Заголовок сверху: подпись во всю ширину, поле под ней.
        const double labelHeight = std::min(17.0, rect.height() * 0.45);
        labelRect = QRectF(rect.left(), rect.top(), rect.width(), labelHeight);
        x = rect.left();
        const double top = rect.top() + labelHeight + 1.0;
        painter.setPen(FormStyle::text);
        painter.drawText(labelRect.adjusted(0, 0, -2, 0),
                         Qt::AlignLeft | Qt::AlignBottom, label);
        drawInput(painter, QRectF(rect.left(), top,
                                  std::max(24.0, rect.width()), rect.bottom() - top),
                  visual);
        return;
    }
    if (placement == 3) {
        const double labelHeight = std::min(17.0, rect.height() * 0.45);
        const double bottom = rect.bottom() - labelHeight - 1.0;
        painter.setPen(FormStyle::text);
        painter.drawText(QRectF(rect.left(), bottom + 1.0, rect.width(), labelHeight),
                         Qt::AlignLeft | Qt::AlignTop, label);
        drawInput(painter, QRectF(rect.left(), rect.top(),
                                  std::max(24.0, rect.width()), bottom - rect.top()),
                  visual);
        return;
    }
    if (placement == 2) {
        // Заголовок справа от поля ввода.
        labelRect = QRectF(rect.right() - labelColumn, rect.top(),
                           labelColumn, rect.height());
        painter.setPen(FormStyle::text);
        painter.drawText(labelRect.adjusted(3, 0, -2, 0),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         label + QStringLiteral(":"));
        drawInput(painter, QRectF(rect.left(), rect.top() + 2.0,
                                  std::max(24.0, rect.width() - labelColumn - 3.0),
                                  rect.height() - 4.0),
                  visual);
        return;
    }

    if (!label.isEmpty() || visual.checkBox) {
        const QRectF labelBox(rect.left(), rect.top(),
                              std::max(0.0, labelColumn), rect.height());
        if (!label.isEmpty()) {
            const QString text = label + QStringLiteral(":");
            const QRectF textRect = labelBox.adjusted(
                0, 0, visual.checkBox ? -2 : -4, 0);
            painter.setPen(FormStyle::text);
            // В 8.3 подпись поля выровнена по правому краю колонки: двоеточия
            // всех полей колонки стоят на одной вертикали.
            if (metrics.horizontalAdvance(text) > textRect.width() + 0.5) {
                painter.drawText(textRect.adjusted(0, 0, -2, 0),
                                 Qt::AlignRight | Qt::AlignVCenter | Qt::TextWordWrap,
                                 text);
            } else {
                painter.drawText(textRect.adjusted(0, 0, -2, 0),
                                 Qt::AlignRight | Qt::AlignVCenter, text);
            }
        }
        x = labelBox.right() + (visual.checkBox ? 2.0 : 3.0);
    }
    if (visual.checkBox) {
        const QRectF box(x + 1.0,
                         rect.top() + (rect.height() - FormStyle::checkBoxSize) / 2.0,
                         FormStyle::checkBoxSize, FormStyle::checkBoxSize);
        painter.setPen(FormStyle::fieldBorder);
        painter.setBrush(Qt::white);
        painter.drawRect(box);
        painter.setBrush(Qt::NoBrush);
        x += FormStyle::checkBoxSize + 6.0;
        if (visual.checkboxOnly)
            return;
    }
    drawInput(painter, QRectF(x, rect.top() + 2.0,
                              std::max(24.0, rect.right() - x - 4.0),
                              rect.height() - 4.0),
              visual);
}

} // namespace controls
