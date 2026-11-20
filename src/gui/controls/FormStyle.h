#pragma once

#include <QColor>

// Палитра и метрики интерфейса 1С:Предприятие 8.3 («Такси»).
// Значения сняты с оформления конфигуратора 8.3: кнопки — белый градиент с
// более тёмной нижней кромкой, кнопка по умолчанию — жёлтая, поля — белые с
// рамкой #a0a0a0, заголовки таблиц — светло-серые со светлой подсветкой.
namespace FormStyle {

inline const QColor text(48, 48, 48);
inline const QColor secondaryText(109, 109, 109);
inline const QColor fieldBorder(160, 160, 160);
inline const QColor formBackground(242, 242, 242);
inline const QColor link(28, 85, 174);

// Кнопка: градиент #ffffff -> #ececec, рамка #b2b2b2 с нижней кромкой #9b9b9b.
inline const QColor buttonTop(255, 255, 255);
inline const QColor buttonMiddle(255, 255, 255);
inline const QColor buttonBottom(236, 236, 236);
inline const QColor buttonBorder(178, 178, 178);
inline const QColor buttonBorderBottom(155, 155, 155);

// Кнопка по умолчанию: жёлтый градиент 8.3.
inline const QColor primaryTop(255, 232, 60);
inline const QColor primaryMiddle(253, 223, 0);
inline const QColor primaryBottom(236, 208, 0);
inline const QColor primaryBorder(170, 150, 0);
inline const QColor primaryBorderBottom(148, 131, 0);
inline const QColor primaryText(26, 26, 26);

// Таблица: внешняя рамка #b1b1b1, сетка #c8c8c8, шапка #f2f2f2.
inline const QColor tableBorder(177, 177, 177);
inline const QColor gridLine(204, 204, 204);
inline const QColor headerBackground(242, 242, 242);
inline const QColor headerHighlight(255, 255, 255);
inline const QColor alternateRow(250, 250, 250);

// Вкладки страниц: полоса #e0e0e0, рамка #b3b3b3, активная — белая.
inline const QColor tabBackground(224, 224, 224);
inline const QColor tabBorder(179, 179, 179);
inline const QColor separator(208, 208, 208);
inline const QColor readonlyBackground(242, 242, 242);

inline constexpr double buttonHeight = 26.0;
inline constexpr double textBoxHeight = 25.0;
inline constexpr double checkBoxSize = 15.0;
inline constexpr double tableHeaderHeight = 24.0;
inline constexpr double tableRowHeight = 20.0;

} // namespace FormStyle
