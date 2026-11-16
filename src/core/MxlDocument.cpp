#include "core/MxlDocument.h"

namespace v8reader {
namespace mxl {

QString NamedArea::boundsText() const
{
    auto range = [](int from, int to) {
        return QStringLiteral("%1..%2").arg(from).arg(to);
    };
    switch (type) {
    case Type::Rows:
        return QStringLiteral("строки %1").arg(range(top, bottom));
    case Type::Columns:
        return QStringLiteral("колонки %1").arg(range(left, right));
    case Type::Rectangle:
        return QStringLiteral("строки %1, колонки %2")
            .arg(range(top, bottom), range(left, right));
    case Type::Unknown:
        break;
    }
    return QStringLiteral("границы не заданы");
}

int Document::maxColumn() const
{
    int maximum = 0;
    for (const Row &row : rows) {
        for (const auto &cell : row.cells) {
            if (cell.first > maximum)
                maximum = cell.first;
        }
    }
    for (const Merge &merge : merges) {
        if (merge.right > maximum)
            maximum = merge.right;
    }
    for (const auto &column : defaultGroup.columns) {
        if (column.first > maximum)
            maximum = column.first;
    }
    for (const ColumnGroup &group : extraGroups) {
        for (const auto &column : group.columns) {
            if (column.first > maximum)
                maximum = column.first;
        }
    }
    return maximum;
}

int Document::maxRowIndex() const
{
    int maximum = 0;
    for (const Row &row : rows) {
        if (row.index > maximum)
            maximum = row.index;
    }
    for (const Merge &merge : merges) {
        if (merge.bottom > maximum)
            maximum = merge.bottom;
    }
    return maximum;
}

const CellFormat *Document::formatAt(int index1based) const
{
    if (index1based <= 0 || index1based > formats.size())
        return nullptr;
    return &formats.at(index1based - 1);
}

bool Document::colorAt(int index, quint32 *rgb) const
{
    if (index < 0 || index >= colors.size())
        return false;
    const Color &color = colors.at(index);
    if (!color.isRgb())
        return false;
    if (rgb)
        *rgb = color.rgb;
    return true;
}

const ColumnGroup &Document::groupForRow(int rowIndex) const
{
    for (const auto &pair : rowGroups) {
        if (pair.first != rowIndex)
            continue;
        if (pair.second >= 0 && pair.second < extraGroups.size())
            return extraGroups.at(pair.second);
        break;
    }
    return defaultGroup;
}

const Cell *Document::cellAt(int rowIndex, int column) const
{
    for (const Row &row : rows) {
        if (row.index != rowIndex)
            continue;
        for (const auto &cell : row.cells) {
            if (cell.first == column)
                return &cell.second;
        }
        return nullptr;
    }
    return nullptr;
}

const Merge *Document::mergeAt(int rowIndex, int column) const
{
    for (const Merge &merge : merges) {
        if (rowIndex >= merge.top && rowIndex <= merge.bottom
            && column >= merge.left && column <= merge.right) {
            return &merge;
        }
    }
    return nullptr;
}

} // namespace mxl
} // namespace v8reader
