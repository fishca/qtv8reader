#pragma once

#include <QFontMetricsF>
#include <QPainter>
#include <QRectF>
#include <QString>

namespace controls {

enum class ButtonIcon {
    None,
    Create,
    CreateBasedOn,
    Copy,
    Change,
    Print,
    Hierarchy,
    Paperclip,
    List,
    Clock,
    Fill,
    Chart,
    Envelope,
    Help,
    News,
    Window,
    Up,
    Down
};

struct ButtonVisual
{
    QRectF rect;
    QString text;
    ButtonIcon icon = ButtonIcon::None;
    bool primary = false;
    bool bordered = false;
    bool iconOnly = false;
    bool dropDown = false;
};

void paintButton(QPainter &painter, const ButtonVisual &visual,
                 const QFontMetricsF &metrics);

} // namespace controls
