#include "CommandBarControl.h"

namespace controls {

double commandBarTotal(const QVector<double> &widths, double gap)
{
    double total = 0.0;
    for (double width : widths)
        total += width;
    if (widths.size() > 1)
        total += gap * (widths.size() - 1);
    return total;
}

QVector<QVector<int>> commandBarRows(const QVector<double> &widths,
                                     double availableWidth, bool allowWrap,
                                     double gap)
{
    QVector<QVector<int>> rows;
    if (!allowWrap) {
        QVector<int> row;
        for (int i = 0; i < widths.size(); ++i)
            row.append(i);
        rows.append(row);
        return rows;
    }

    QVector<int> currentRow;
    double currentWidth = 0.0;
    for (int i = 0; i < widths.size(); ++i) {
        const double rowGap = currentRow.isEmpty() ? 0.0 : gap;
        if (!currentRow.isEmpty()
            && currentWidth + rowGap + widths.at(i) > availableWidth) {
            rows.append(currentRow);
            currentRow.clear();
            currentWidth = 0.0;
        }
        currentRow.append(i);
        currentWidth += (currentRow.size() > 1 ? gap : 0.0) + widths.at(i);
    }
    if (!currentRow.isEmpty())
        rows.append(currentRow);
    return rows;
}

double commandBarRightShift(const QVector<double> &widths,
                            double availableWidth, int rightStart,
                            double gap)
{
    if (rightStart <= 0)
        return 0.0;
    const double used = commandBarTotal(widths, gap);
    return used < availableWidth ? availableWidth - used : 0.0;
}

} // namespace controls
