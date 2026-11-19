#pragma once

#include <QFontMetricsF>
#include <QPainter>
#include <QRectF>
#include <QString>

namespace controls {

// Поле ввода 1С: подпись, само поле, флажок и кнопки выбора/календаря.
struct FieldVisual
{
    QRectF rect;
    QString label;
    double labelWidth = 0.0;
    double inputWidth = 0.0;
    bool labelOnly = false;
    bool checkBox = false;
    bool checkboxOnly = false;
    bool dropDown = false;
    bool calendar = false;
    bool choiceButton = false;
};

void paintField(QPainter &painter, const FieldVisual &visual,
                const QFontMetricsF &metrics);

} // namespace controls
