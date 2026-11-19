#pragma once

#include <QVector>

namespace controls {

// Раскладка командной панели 1С: суммарная ширина, перенос по строкам
// и сдвиг служебных команд (поиск, «Еще», «?») к правому краю.
double commandBarTotal(const QVector<double> &widths, double gap = 8.0);

QVector<QVector<int>> commandBarRows(const QVector<double> &widths,
                                     double availableWidth, bool allowWrap,
                                     double gap = 8.0);

double commandBarRightShift(const QVector<double> &widths,
                            double availableWidth, int rightStart,
                            double gap = 8.0);

} // namespace controls
