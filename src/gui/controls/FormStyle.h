#pragma once

#include <QColor>

// Палитра и метрики интерфейса 1С:Предприятие 8.3 («Такси»).
// Форма управляемого приложения рисуется на белом фоне; заголовки таблиц,
// поля и кнопки имеют собственные цвета платформы.
namespace FormStyle {

inline const QColor text(0, 0, 0);
inline const QColor secondaryText(90, 90, 90);
inline const QColor fieldBorder(160, 160, 160);
inline const QColor primaryTop(255, 250, 205);
inline const QColor primaryBottom(250, 237, 158);
inline const QColor primaryBorder(191, 164, 90);
inline const QColor tableBorder(183, 183, 183);
inline const QColor gridLine(199, 199, 199);
inline const QColor headerBackground(242, 242, 242);
inline const QColor alternateRow(250, 250, 250);
inline const QColor separator(208, 208, 208);

inline constexpr double buttonHeight = 24.0;
inline constexpr double tableHeaderHeight = 22.0;
inline constexpr double tableRowHeight = 20.0;

} // namespace FormStyle
