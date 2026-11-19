#include "ButtonControl.h"

#include "FormStyle.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>

#include <algorithm>

namespace controls {

void drawButtonIcon(QPainter &painter, ButtonIcon icon, const QRectF &rect)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPointF c = rect.center();
    const double s = rect.width();
    const double stroke = std::max(1.2, s * 0.08);

    switch (icon) {
    case ButtonIcon::Create:
    case ButtonIcon::CreateBasedOn: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(76, 175, 80));
        painter.drawEllipse(rect.adjusted(0, 0, 0, 0));
        QPen pen(Qt::white, stroke, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(QPointF(c.x() - s * 0.22, c.y()),
                         QPointF(c.x() + s * 0.22, c.y()));
        painter.drawLine(QPointF(c.x(), c.y() - s * 0.22),
                         QPointF(c.x(), c.y() + s * 0.22));
        if (icon == ButtonIcon::CreateBasedOn) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(76, 175, 80));
            painter.drawEllipse(QPointF(rect.right() - 2.0, rect.top() + 2.0), 3.5, 3.5);
            painter.setPen(QPen(Qt::white, 1.2));
            painter.drawLine(QPointF(rect.right() - 4.5, rect.top() + 2.0),
                             QPointF(rect.right() + 0.5, rect.top() + 2.0));
            painter.drawLine(QPointF(rect.right() - 2.0, rect.top() - 0.5),
                             QPointF(rect.right() - 2.0, rect.top() + 4.5));
        }
        break;
    }
    case ButtonIcon::Copy: {
        const QRectF back(rect.left() + s * 0.10, rect.top() + s * 0.25,
                          s * 0.58, s * 0.58);
        painter.setPen(QPen(QColor(100, 100, 100), stroke));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(back, 1.5, 1.5);
        const QRectF front(rect.left() + s * 0.28, rect.top() + s * 0.08,
                           s * 0.58, s * 0.58);
        painter.setBrush(Qt::white);
        painter.drawRoundedRect(front, 1.5, 1.5);
        painter.setBrush(Qt::NoBrush);
        break;
    }
    case ButtonIcon::Change: {
        QPen pen(QColor(245, 145, 30), stroke, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(QPointF(rect.left() + s * 0.2, rect.bottom() - s * 0.2),
                         QPointF(rect.right() - s * 0.2, rect.top() + s * 0.2));
        painter.setPen(QPen(QColor(90, 90, 90), stroke * 0.8));
        painter.drawLine(QPointF(rect.left() + s * 0.18, rect.bottom() - s * 0.18),
                         QPointF(rect.left() + s * 0.35, rect.bottom() - s * 0.35));
        break;
    }
    case ButtonIcon::Print: {
        painter.setPen(QPen(QColor(70, 70, 70), stroke));
        painter.setBrush(QColor(210, 210, 210));
        const QRectF body(rect.left() + s * 0.12, rect.top() + s * 0.35,
                          s * 0.76, s * 0.42);
        painter.drawRoundedRect(body, 1.5, 1.5);
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF(rect.left() + s * 0.25, rect.top() + s * 0.08,
                                s * 0.5, s * 0.3));
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF(rect.left() + s * 0.25, rect.top() + s * 0.62,
                                s * 0.5, s * 0.3));
        painter.setBrush(Qt::NoBrush);
        break;
    }
    case ButtonIcon::Hierarchy: {
        painter.setPen(QPen(QColor(80, 80, 80), stroke));
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF(rect.left() + s * 0.08, rect.top() + s * 0.12,
                                s * 0.34, s * 0.24));
        painter.drawRect(QRectF(rect.left() + s * 0.58, rect.top() + s * 0.12,
                                s * 0.34, s * 0.24));
        painter.drawRect(QRectF(rect.left() + s * 0.34, rect.top() + s * 0.62,
                                s * 0.34, s * 0.24));
        painter.drawLine(QPointF(c.x(), rect.top() + s * 0.36),
                         QPointF(c.x(), rect.top() + s * 0.56));
        painter.drawLine(QPointF(rect.left() + s * 0.25, rect.top() + s * 0.36),
                         QPointF(rect.left() + s * 0.25, rect.top() + s * 0.5));
        painter.drawLine(QPointF(rect.right() - s * 0.25, rect.top() + s * 0.36),
                         QPointF(rect.right() - s * 0.25, rect.top() + s * 0.5));
        break;
    }
    case ButtonIcon::Paperclip: {
        painter.setPen(QPen(QColor(90, 90, 90), stroke, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(rect.left() + s * 0.25, rect.top() + s * 0.7);
        path.cubicTo(rect.left() + s * 0.05, rect.top() + s * 0.45,
                     rect.left() + s * 0.25, rect.top() + s * 0.1,
                     rect.left() + s * 0.5, rect.top() + s * 0.25);
        path.cubicTo(rect.left() + s * 0.7, rect.top() + s * 0.35,
                     rect.left() + s * 0.55, rect.top() + s * 0.8,
                     rect.left() + s * 0.78, rect.top() + s * 0.7);
        painter.drawPath(path);
        break;
    }
    case ButtonIcon::List: {
        painter.setPen(QPen(QColor(80, 80, 80), stroke));
        for (int i = 0; i < 3; ++i) {
            const double y = rect.top() + s * (0.25 + i * 0.25);
            painter.drawLine(QPointF(rect.left() + s * 0.35, y),
                             QPointF(rect.right() - s * 0.1, y));
            painter.drawEllipse(QPointF(rect.left() + s * 0.15, y), s * 0.06, s * 0.06);
        }
        break;
    }
    case ButtonIcon::Clock: {
        painter.setPen(QPen(QColor(80, 80, 80), stroke));
        painter.setBrush(Qt::white);
        painter.drawEllipse(rect.adjusted(s * 0.12, s * 0.12, -s * 0.12, -s * 0.12));
        painter.drawLine(c, QPointF(c.x(), c.y() - s * 0.22));
        painter.drawLine(c, QPointF(c.x() + s * 0.18, c.y() + s * 0.08));
        break;
    }
    case ButtonIcon::Fill: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 193, 7));
        QPolygonF bolt;
        bolt << QPointF(c.x() + s * 0.08, rect.top() + s * 0.05)
             << QPointF(rect.left() + s * 0.2, c.y() + s * 0.02)
             << QPointF(c.x(), c.y() + s * 0.02)
             << QPointF(c.x() - s * 0.12, rect.bottom() - s * 0.05)
             << QPointF(rect.right() - s * 0.15, c.y() - s * 0.04)
             << QPointF(c.x() + s * 0.02, c.y() - s * 0.04);
        painter.drawPolygon(bolt);
        break;
    }
    case ButtonIcon::Chart: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(66, 133, 244));
        painter.drawRect(QRectF(rect.left() + s * 0.15, rect.top() + s * 0.5,
                                s * 0.16, s * 0.4));
        painter.setBrush(QColor(52, 168, 83));
        painter.drawRect(QRectF(rect.left() + s * 0.42, rect.top() + s * 0.28,
                                s * 0.16, s * 0.62));
        painter.setBrush(QColor(251, 188, 5));
        painter.drawRect(QRectF(rect.left() + s * 0.69, rect.top() + s * 0.12,
                                s * 0.16, s * 0.78));
        break;
    }
    case ButtonIcon::Envelope: {
        painter.setPen(QPen(QColor(80, 80, 80), stroke));
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF(rect.left() + s * 0.1, rect.top() + s * 0.22,
                                s * 0.8, s * 0.56));
        painter.drawLine(QPointF(rect.left() + s * 0.1, rect.top() + s * 0.22),
                         QPointF(c.x(), rect.top() + s * 0.56));
        painter.drawLine(QPointF(rect.right() - s * 0.1, rect.top() + s * 0.22),
                         QPointF(c.x(), rect.top() + s * 0.56));
        break;
    }
    case ButtonIcon::Help: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(52, 120, 200));
        painter.drawEllipse(rect.adjusted(s * 0.08, s * 0.08, -s * 0.08, -s * 0.08));
        painter.setPen(QColor(Qt::white));
        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(int(s * 0.7));
        painter.setFont(font);
        painter.drawText(rect, Qt::AlignCenter, QStringLiteral("?"));
        break;
    }
    case ButtonIcon::News: {
        painter.setPen(QPen(QColor(70, 110, 160), stroke));
        painter.setBrush(QColor(220, 235, 250));
        painter.drawEllipse(rect.adjusted(s * 0.1, s * 0.1, -s * 0.1, -s * 0.1));
        painter.drawLine(QPointF(c.x(), rect.top() + s * 0.1),
                         QPointF(c.x(), rect.bottom() - s * 0.1));
        painter.drawLine(QPointF(rect.left() + s * 0.1, c.y()),
                         QPointF(rect.right() - s * 0.1, c.y()));
        break;
    }
    case ButtonIcon::Window: {
        painter.setPen(QPen(QColor(80, 80, 80), stroke));
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF(rect.left() + s * 0.12, rect.top() + s * 0.22,
                                s * 0.5, s * 0.5));
        painter.setBrush(QColor(230, 230, 230));
        painter.drawRect(QRectF(rect.left() + s * 0.38, rect.top() + s * 0.38,
                                s * 0.5, s * 0.5));
        break;
    }
    case ButtonIcon::Up:
    case ButtonIcon::Down: {
        const bool up = icon == ButtonIcon::Up;
        painter.setPen(QPen(QColor(125, 125, 125), 1.2));
        const double y1 = up ? rect.bottom() - s * 0.2 : rect.top() + s * 0.2;
        const double y2 = up ? rect.top() + s * 0.2 : rect.bottom() - s * 0.2;
        painter.drawLine(QPointF(c.x(), y1), QPointF(c.x(), y2));
        const double head = up ? y2 : y1;
        const double direction = up ? 1.0 : -1.0;
        painter.drawLine(QPointF(c.x(), head),
                         QPointF(c.x() - s * 0.14, head + direction * s * 0.16));
        painter.drawLine(QPointF(c.x(), head),
                         QPointF(c.x() + s * 0.14, head + direction * s * 0.16));
        break;
    }
    case ButtonIcon::None:
        break;
    }
    painter.restore();
}

void paintButton(QPainter &painter, const ButtonVisual &visual,
                 const QFontMetricsF &metrics)
{
    const QRectF &rect = visual.rect;
    if (visual.primary) {
        QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
        gradient.setColorAt(0.0, FormStyle::primaryTop);
        gradient.setColorAt(1.0, FormStyle::primaryBottom);
        painter.setPen(FormStyle::primaryBorder);
        painter.setBrush(gradient);
        painter.drawRoundedRect(rect, 2.0, 2.0);
        painter.setBrush(Qt::NoBrush);
    } else if (visual.bordered) {
        QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
        gradient.setColorAt(0.0, QColor(247, 247, 247));
        gradient.setColorAt(1.0, QColor(228, 228, 228));
        painter.setPen(FormStyle::fieldBorder);
        painter.setBrush(gradient);
        painter.drawRoundedRect(rect, 2.0, 2.0);
        painter.setBrush(Qt::NoBrush);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::NoBrush);
    }
    painter.setPen(FormStyle::text);

    const double iconSize = std::min(16.0, rect.height() - 8.0);
    double textLeft = rect.left() + 6.0;
    double textRight = rect.right() - (visual.dropDown ? 18.0 : 6.0);
    if (visual.icon != ButtonIcon::None) {
        QRectF iconRect;
        if (visual.iconOnly) {
            iconRect = QRectF(rect.center().x() - iconSize / 2.0,
                              rect.center().y() - iconSize / 2.0,
                              iconSize, iconSize);
        } else {
            iconRect = QRectF(rect.left() + 5.0,
                              rect.center().y() - iconSize / 2.0,
                              iconSize, iconSize);
            textLeft = iconRect.right() + 4.0;
        }
        drawButtonIcon(painter, visual.icon, iconRect);
    }
    if (!visual.iconOnly) {
        const QString text = metrics.elidedText(visual.text, Qt::ElideRight,
                                                int(std::max(10.0, textRight - textLeft)));
        painter.setPen(FormStyle::text);
        painter.drawText(QRectF(textLeft, rect.top(), textRight - textLeft,
                                rect.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, text);
    }
    if (visual.dropDown) {
        const QPointF p(rect.right() - 10.0, rect.center().y());
        painter.setPen(FormStyle::secondaryText);
        painter.drawLine(p + QPointF(-3, -2), p + QPointF(0, 2));
        painter.drawLine(p + QPointF(0, 2), p + QPointF(3, -2));
    }
}

} // namespace controls
