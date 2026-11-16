#include "core/MxlReader.h"

#include "core/V8Text.h"

#include <QFile>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <limits>

namespace v8reader {

namespace {

using v8reader::V8TextNode;

constexpr int kRowsStartIndex = 15;
constexpr int kMaxRows = 1'000'000;
constexpr int kMaxCells = 100'000;

const V8TextNode *childAt(const V8TextNode &node, int index)
{
    return node.child(index);
}

bool nodeInt(const V8TextNode *node, int *value)
{
    if (!node)
        return false;
    bool ok = false;
    const qint64 parsed = node->toInteger(&ok);
    if (!ok || parsed < std::numeric_limits<int>::min()
        || parsed > std::numeric_limits<int>::max()) {
        return false;
    }
    if (value)
        *value = static_cast<int>(parsed);
    return true;
}

bool intAt(const V8TextNode &list, int index, int *value)
{
    return nodeInt(childAt(list, index), value);
}

int intOr(const V8TextNode &list, int index, int fallback = 0)
{
    int value = fallback;
    return intAt(list, index, &value) ? value : fallback;
}

bool isNumberString(const QString &value)
{
    if (value.isEmpty())
        return false;
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return false;
    int index = 0;
    if (trimmed.at(0) == QLatin1Char('-') || trimmed.at(0) == QLatin1Char('+'))
        index = 1;
    if (index >= trimmed.size())
        return false;
    for (int i = index; i < trimmed.size(); ++i) {
        if (!trimmed.at(i).isDigit())
            return false;
    }
    return true;
}

bool isGuidString(const QString &value)
{
    return V8Text::isGuid(value);
}

mxl::Cell cellFromValue(const V8TextNode &fields)
{
    mxl::Cell cell;
    int marker = 0;
    if (!nodeInt(childAt(fields, 0), &marker))
        return cell;
    int format = 0;
    if (nodeInt(childAt(fields, 1), &format) && format > 0)
        cell.format = format;

    int next = 2;
    if ((marker & 2) != 0) {
        const V8TextNode *typed = childAt(fields, next);
        if (typed && typed->isList() && typed->children.size() >= 2) {
            if (typed->child(0) && typed->child(0)->scalar() == QStringLiteral("S"))
                cell.text = typed->child(1) ? typed->child(1)->scalar() : QString();
        }
        ++next;
    }
    if ((marker & 4) != 0)
        ++next; // ссылка-расшифровка; сам текст обычно уже есть в кэше
    if ((marker & 8) != 0)
        ++next; // дополнительное строковое поле (например, имя типа) перед текстом
    if ((marker & 16) != 0) {
        const V8TextNode *localized = childAt(fields, next);
        int hasText = 0;
        if (localized && localized->isList()
            && nodeInt(childAt(*localized, 1), &hasText) && hasText == 1) {
            const V8TextNode *pair = childAt(*localized, 2);
            if (pair && pair->isList() && pair->children.size() >= 2) {
                const V8TextNode *text = childAt(*pair, 1);
                if (text && text->isScalar())
                    cell.text = text->value;
            }
        }
    }
    return cell;
}

struct RowsParseResult
{
    bool ok = false;
    QVector<mxl::Row> rows;
    int endIndex = 0;
};

RowsParseResult parseRows(const V8TextNode &root, int start)
{
    RowsParseResult result;
    int rowCount = 0;
    if (!intAt(root, start, &rowCount) || rowCount < 0 || rowCount > kMaxRows)
        return result;

    int index = start + 1;
    int previous = -1;
    result.rows.reserve(rowCount);
    for (int r = 0; r < rowCount; ++r) {
        int rowIndex = 0;
        int rowFormat = 0;
        int cellCount = 0;
        if (!intAt(root, index, &rowIndex)
            || !intAt(root, index + 1, &rowFormat)
            || !intAt(root, index + 2, &cellCount)) {
            return result;
        }
        if (rowIndex <= previous || rowIndex < 0)
            return result;
        if (cellCount < 0 || cellCount > kMaxCells)
            return result;
        previous = rowIndex;
        index += 3;

        mxl::Row row;
        row.index = rowIndex;
        row.format = rowFormat > 0 ? rowFormat : 0;
        row.cells.reserve(cellCount);
        for (int c = 0; c < cellCount; ++c) {
            int column = 0;
            if (!intAt(root, index, &column))
                return result;
            const V8TextNode *cellNode = childAt(root, index + 1);
            if (!cellNode || !cellNode->isList())
                return result;
            row.cells.append({column, cellFromValue(*cellNode)});
            index += 2;
        }
        result.rows.append(row);
    }

    result.endIndex = index;
    result.ok = !result.rows.isEmpty();
    return result;
}

bool columnGroupFromValue(const V8TextNode &node, mxl::ColumnGroup *group)
{
    if (!node.isList() || node.children.size() < 4)
        return false;
    int count = 0;
    if (!intAt(node, 3, &count) || count < 0)
        return false;
    const int expected = 4 + 2 * count;
    if (node.children.size() != expected)
        return false;

    mxl::ColumnGroup parsed;
    parsed.columns.reserve(count);
    for (int i = 0; i < count; ++i) {
        int column = 0;
        int format = 0;
        if (!intAt(node, 4 + 2 * i, &column) || !intAt(node, 5 + 2 * i, &format))
            return false;
        if (column < 0 || format < 0)
            return false;
        parsed.columns.append({column, format});
    }
    *group = parsed;
    return true;
}

mxl::CellFormat formatFromValue(const V8TextNode &node)
{
    mxl::CellFormat format;
    int flags = 0;
    if (!node.isList() || !nodeInt(childAt(node, 0), &flags) || flags < 0)
        return format;

    int valueIndex = 1;
    for (int bit = 0; bit < 40; ++bit) {
        const int mask = 1 << bit;
        if ((flags & mask) == 0)
            continue;
        int value = 0;
        const bool hasValue = nodeInt(childAt(node, valueIndex), &value);
        ++valueIndex;
        if (!hasValue || value < 0)
            continue;
        switch (bit) {
        case 0: format.font = value; break;
        case 1: format.borderLeft = value; break;
        case 2: format.borderTop = value; break;
        case 3: format.borderRight = value; break;
        case 4: format.borderBottom = value; break;
        case 5: format.bordersColor = value; break;
        case 6: format.height = value; break;
        case 7: format.width = value; break;
        case 8: format.hAlign = value; break;
        case 9: format.vAlign = value; break;
        case 10: format.textColor = value; break;
        case 11: format.bgColor = value; break;
        case 12: format.pattern = value; break;
        case 13: format.patternColor = value; break;
        case 14: format.textControl = value; break;
        case 18: format.orientation = value; break;
        default: break;
        }
    }
    return format;
}

mxl::FontSpec fontFromValue(const V8TextNode &node)
{
    mxl::FontSpec font;
    if (!node.isList() || node.children.size() < 3)
        return font;
    int tag = 0;
    if (!nodeInt(childAt(node, 0), &tag) || (tag != 6 && tag != 7))
        return font;

    int kind = 0;
    nodeInt(childAt(node, 1), &kind);
    if (kind == 0) {
        const int height = intOr(node, 3, 0);
        if (height > 0)
            font.sizeTenthPt = height;
        font.bold = intOr(node, 7, 0) >= 600;
        font.italic = intOr(node, 8, 0) != 0;
        font.underline = intOr(node, 9, 0) != 0;
        font.strikeout = intOr(node, 10, 0) != 0;
    } else {
        int flags = 0;
        nodeInt(childAt(node, 2), &flags);
        int valueIndex = 3;
        if (childAt(node, valueIndex) && childAt(node, valueIndex)->isList())
            ++valueIndex;
        auto take = [&](bool present, bool *isString, QString *stringValue) -> bool {
            if (!present)
                return false;
            const V8TextNode *value = childAt(node, valueIndex++);
            if (!value)
                return false;
            if (isString)
                *isString = value->isString();
            if (stringValue && value->isScalar())
                *stringValue = value->value;
            return true;
        };
        auto takeInt = [&](bool present) -> int {
            bool isString = false;
            QString value;
            if (!take(present, &isString, &value) || isString)
                return 0;
            bool ok = false;
            const qint64 parsed = value.toLongLong(&ok);
            return ok ? static_cast<int>(parsed) : 0;
        };

        constexpr int kFamily = 1;
        constexpr int kHeight = 2;
        constexpr int kWeight = 4;
        constexpr int kItalic = 8;
        constexpr int kUnderline = 16;
        constexpr int kStrikeout = 32;

        if ((flags & kFamily) != 0) {
            bool isString = false;
            QString value;
            if (take(true, &isString, &value) && isString)
                font.family = value;
        }
        if ((flags & kHeight) != 0) {
            const int height = takeInt(true);
            if (height > 0)
                font.sizeTenthPt = height;
        }
        if ((flags & kWeight) != 0)
            font.bold = takeInt(true) >= 600;
        if ((flags & kItalic) != 0)
            font.italic = takeInt(true) != 0;
        if ((flags & kUnderline) != 0)
            font.underline = takeInt(true) != 0;
        if ((flags & kStrikeout) != 0)
            font.strikeout = takeInt(true) != 0;
    }

    // Полная запись: имя шрифта ищется среди строковых полей хвоста.
    if (font.family.isEmpty()) {
        for (int i = 11; i < node.children.size(); ++i) {
            const V8TextNode &value = node.children.at(i);
            if (!value.isString())
                continue;
            const QString text = value.value.trimmed();
            if (text.isEmpty() || isNumberString(text) || isGuidString(text))
                continue;
            if (text == QStringLiteral("ru") || text == QStringLiteral("en"))
                continue;
            font.family = text;
            break;
        }
    }
    if (font.family.isEmpty())
        font.family = QStringLiteral("Arial");
    return font;
}

bool colorFromValue(const V8TextNode &node, mxl::Color *color)
{
    if (!node.isList() || node.children.size() < 3)
        return false;
    int tag = 0;
    if (!intAt(node, 0, &tag) || tag != 3)
        return false;
    int kind = 0;
    if (!intAt(node, 1, &kind))
        return false;
    const V8TextNode *valueNode = childAt(node, 2);
    if (!valueNode || !valueNode->isList() || valueNode->children.isEmpty())
        return false;
    int value = 0;
    if (!nodeInt(childAt(*valueNode, 0), &value))
        return false;

    mxl::Color parsed;
    if (kind == 0 && value >= 0 && value <= 0xFFFFFF) {
        parsed.kind = mxl::Color::Kind::Rgb;
        parsed.rgb = static_cast<quint32>(value);
    } else {
        parsed.kind = mxl::Color::Kind::Auto;
    }
    *color = parsed;
    return true;
}

bool findColorTable(const V8TextNode &root, int start, QVector<mxl::Color> *colors,
                    int *endIndex)
{
    for (int i = start; i + 1 < root.children.size(); ++i) {
        int count = 0;
        if (!intAt(root, i, &count) || count <= 0 || count > 10000)
            continue;
        if (i + count >= root.children.size())
            continue;

        QVector<mxl::Color> parsed;
        parsed.reserve(count);
        bool ok = true;
        for (int k = 0; k < count; ++k) {
            mxl::Color color;
            if (!colorFromValue(root.children.at(i + 1 + k), &color)) {
                ok = false;
                break;
            }
            parsed.append(color);
        }
        if (ok && parsed.size() == count) {
            *colors = parsed;
            if (endIndex)
                *endIndex = i + 1 + count;
            return true;
        }
    }
    return false;
}

bool lineFromRecord(const V8TextNode &record, mxl::LineStyle *line)
{
    if (!record.isList() || record.children.size() < 5)
        return false;
    int tag = 0;
    int zero = 0;
    if (!intAt(record, 0, &tag) || tag != 4 || !intAt(record, 1, &zero) || zero != 0)
        return false;
    int kind = 0;
    int thickness = 0;
    if (!intAt(record, 3, &kind) || !intAt(record, 4, &thickness))
        return false;
    line->kind = std::max(0, kind);
    line->thickness = std::max(0, thickness);
    return true;
}

QVector<mxl::LineStyle> findLineTable(const V8TextNode &root)
{
    for (const V8TextNode &field : root.children) {
        if (!field.isList() || field.children.isEmpty())
            continue;
        int count = 0;
        if (!nodeInt(childAt(field, 0), &count) || count < 1 || count > 100000)
            continue;
        if (field.children.size() != 1 + 3 * count)
            continue;
        QVector<mxl::LineStyle> lines;
        lines.reserve(count);
        bool ok = true;
        for (int k = 0; k < count; ++k) {
            const V8TextNode *record = childAt(field, 2 + 3 * k);
            mxl::LineStyle line;
            if (!record || !lineFromRecord(*record, &line)) {
                ok = false;
                break;
            }
            lines.append(line);
        }
        if (ok && !lines.isEmpty())
            return lines;
    }
    return {};
}

bool drawingFromValue(const V8TextNode &node, mxl::Drawing *drawing)
{
    if (!node.isList() || node.children.size() < 11)
        return false;
    const V8TextNode *cellLike = childAt(node, 0);
    if (!cellLike || !cellLike->isList())
        return false;

    int kindNumber = 0;
    if (!intAt(node, 1, &kindNumber))
        return false;

    mxl::Drawing parsed;
    switch (kindNumber) {
    case 2: parsed.kind = mxl::DrawingKind::Rectangle; break;
    case 3: parsed.kind = mxl::DrawingKind::Text; break;
    case 5: parsed.kind = mxl::DrawingKind::Picture; break;
    default: parsed.kind = mxl::DrawingKind::Other; break;
    }

    const mxl::Cell cell = cellFromValue(*cellLike);
    parsed.format = cell.format;
    parsed.text = cell.text;
    parsed.colStart = std::max(0, intOr(node, 2, 0));
    parsed.rowStart = std::max(0, intOr(node, 3, 0));
    parsed.offsetLeft = std::max(0, intOr(node, 4, 0));
    parsed.offsetTop = std::max(0, intOr(node, 5, 0));
    parsed.colEnd = std::max(0, intOr(node, 6, 0));
    parsed.rowEnd = std::max(0, intOr(node, 7, 0));
    parsed.offsetRight = std::max(0, intOr(node, 8, 0));
    parsed.offsetBottom = std::max(0, intOr(node, 9, 0));
    if (parsed.kind == mxl::DrawingKind::Picture) {
        const int picture = intOr(node, 11, -1);
        parsed.picture = picture > 0 ? picture : -1;
    }
    *drawing = parsed;
    return true;
}

bool pictureFromValue(const V8TextNode &node, QByteArray *data)
{
    if (!node.isList())
        return false;
    for (const V8TextNode &field : node.children) {
        if (!field.isList())
            continue;
        bool allScalar = true;
        bool hasBase64 = false;
        QString joined;
        for (const V8TextNode &item : field.children) {
            if (!item.isScalar()) {
                allScalar = false;
                break;
            }
            if (item.value.startsWith(QLatin1String("#base64:"))
                || item.value.startsWith(QLatin1String("#data:"))) {
                hasBase64 = true;
            }
            joined += item.value;
        }
        if (allScalar && hasBase64) {
            QByteArray decoded;
            if (V8Text::decodeBase64Value(joined, &decoded)) {
                *data = decoded;
                return true;
            }
        }
        QByteArray nested;
        if (pictureFromValue(field, &nested)) {
            *data = nested;
            return true;
        }
    }
    return false;
}

bool findPictureTable(const V8TextNode &root, int start, QVector<QByteArray> *pictures)
{
    for (int i = start; i + 1 < root.children.size(); ++i) {
        int count = 0;
        if (!intAt(root, i, &count) || count <= 0 || count > 1000)
            continue;
        if (i + count >= root.children.size())
            continue;

        bool anyPicture = false;
        QVector<QByteArray> parsed;
        parsed.reserve(count);
        for (int k = 0; k < count; ++k) {
            const V8TextNode &entry = root.children.at(i + 1 + k);
            if (!entry.isList()) {
                anyPicture = false;
                break;
            }
            QByteArray data;
            if (pictureFromValue(entry, &data))
                anyPicture = true;
            parsed.append(data);
        }
        if (anyPicture) {
            *pictures = parsed;
            return true;
        }
    }
    return false;
}

bool mergesFromValue(const V8TextNode &node, QVector<mxl::Merge> *merges)
{
    if (!node.isList() || node.children.isEmpty())
        return false;
    int count = 0;
    if (!nodeInt(childAt(node, 0), &count) || count <= 0)
        return false;
    if (node.children.size() != count + 1)
        return false;

    QVector<mxl::Merge> parsed;
    parsed.reserve(count);
    for (int i = 1; i <= count; ++i) {
        const V8TextNode &item = node.children.at(i);
        if (!item.isList() || item.children.size() != 5)
            return false;
        int values[5] = {0, 0, 0, 0, 0};
        for (int k = 0; k < 5; ++k) {
            if (!nodeInt(childAt(item, k), &values[k]))
                return false;
        }
        const int left = values[0];
        const int top = values[1];
        const int right = values[2];
        const int bottom = values[3];
        const int flag = values[4];
        if (flag != 0 || left < 0 || top < 0 || right < left || bottom < top)
            continue;
        mxl::Merge merge;
        merge.left = left;
        merge.top = top;
        merge.right = right;
        merge.bottom = bottom;
        parsed.append(merge);
    }
    *merges = parsed;
    return true;
}

bool parseTypedValue(const V8TextNode &node, bool *isNumber, qint64 *number,
                     QString *text)
{
    if (!node.isList() || node.children.size() < 2)
        return false;
    const V8TextNode *typeNode = childAt(node, 0);
    const V8TextNode *valueNode = childAt(node, 1);
    if (!typeNode || !typeNode->isScalar() || !valueNode || !valueNode->isScalar())
        return false;

    const QString type = typeNode->value;
    if (type == QStringLiteral("S")) {
        if (isNumber)
            *isNumber = false;
        if (text)
            *text = valueNode->value;
        return true;
    }
    if (type == QStringLiteral("N")) {
        bool ok = false;
        const qint64 parsed = valueNode->toInteger(&ok);
        if (!ok)
            return false;
        if (isNumber)
            *isNumber = true;
        if (number)
            *number = parsed;
        return true;
    }
    return false;
}

bool namedAreasFromValue(const V8TextNode &node, QVector<mxl::NamedArea> *areas)
{
    if (!node.isList() || node.children.size() < 3)
        return false;
    int count = 0;
    if (!nodeInt(childAt(node, 0), &count) || count <= 0 || count > 10000)
        return false;
    if (node.children.size() != 1 + 2 * count)
        return false;

    QVector<mxl::NamedArea> parsed;
    parsed.reserve(count);
    for (int i = 0; i < count; ++i) {
        const V8TextNode *nameNode = childAt(node, 1 + 2 * i);
        const V8TextNode *record = childAt(node, 2 + 2 * i);
        if (!nameNode || !nameNode->isString() || nameNode->value.isEmpty())
            return false;
        if (!record || !record->isList() || record->children.size() < 2)
            return false;
        const V8TextNode *bounds = childAt(*record, 1);
        if (!bounds || !bounds->isList() || bounds->children.size() < 5)
            return false;

        int left = -1;
        int top = -1;
        int right = -1;
        int bottom = -1;
        if (!nodeInt(childAt(*bounds, 1), &left)
            || !nodeInt(childAt(*bounds, 2), &top)
            || !nodeInt(childAt(*bounds, 3), &right)
            || !nodeInt(childAt(*bounds, 4), &bottom)) {
            return false;
        }
        if (left < -1 || top < -1 || right < -1 || bottom < -1)
            return false;
        if (!(left >= 0 || top >= 0 || right >= 0 || bottom >= 0))
            return false;

        mxl::NamedArea area;
        area.name = nameNode->value;
        area.left = left;
        area.top = top;
        area.right = right;
        area.bottom = bottom;

        if (area.hasRows() && area.hasColumns())
            area.type = mxl::NamedArea::Type::Rectangle;
        else if (area.hasRows())
            area.type = mxl::NamedArea::Type::Rows;
        else if (area.hasColumns())
            area.type = mxl::NamedArea::Type::Columns;
        else
            area.type = mxl::NamedArea::Type::Unknown;

        parsed.append(area);
    }
    *areas = parsed;
    return true;
}

bool printSettingsFromValue(const V8TextNode &node, mxl::PrintSettings *settings)
{
    if (!node.isList() || node.children.size() != 1)
        return false;
    const V8TextNode *inner = childAt(node, 0);
    if (!inner || !inner->isList() || inner->children.size() < 4)
        return false;

    int flags = 0;
    int count = 0;
    int firstKey = 0;
    if (!nodeInt(childAt(*inner, 0), &flags)
        || !nodeInt(childAt(*inner, 1), &count)
        || !nodeInt(childAt(*inner, 2), &firstKey)) {
        return false;
    }
    if (count <= 0 || count > 200)
        return false;

    QVector<QPair<int, QPair<bool, qint64>>> numericPairs;
    QVector<QPair<int, QString>> stringPairs;
    QPair<bool, qint64> dummy{true, 0};

    const auto addPair = [&](int key, const V8TextNode &valueNode) -> bool {
        bool isNumber = false;
        qint64 number = 0;
        QString text;
        if (!parseTypedValue(valueNode, &isNumber, &number, &text))
            return false;
        if (isNumber)
            numericPairs.append({key, {true, number}});
        else
            stringPairs.append({key, text});
        return true;
    };

    if (!addPair(firstKey, *childAt(*inner, 3)))
        return false;

    int index = 4;
    int parsedCount = 1;
    while (index + 1 < inner->children.size()) {
        int key = 0;
        if (!nodeInt(childAt(*inner, index), &key))
            return false;
        if (!addPair(key, *childAt(*inner, index + 1)))
            return false;
        index += 2;
        ++parsedCount;
    }
    if (parsedCount != count)
        return false;

    mxl::PrintSettings parsed;
    parsed.valid = true;
    for (const auto &pair : stringPairs) {
        if (pair.first == 0 && !pair.second.isEmpty())
            parsed.pageSize = pair.second;
    }
    for (const auto &pair : numericPairs) {
        const int key = pair.first;
        const qint64 value = pair.second.second;
        switch (key) {
        case 0:
            break; // размер страницы задаётся строкой
        case 1:
            parsed.orientation = value == 2 ? 2 : 1;
            break;
        case 2:
            if (value > 0)
                parsed.scale = static_cast<int>(value);
            break;
        case 6:
            if (value >= 0)
                parsed.topMarginMm = value / 100.0;
            break;
        case 7:
            if (value >= 0)
                parsed.bottomMarginMm = value / 100.0;
            break;
        case 8:
            if (value >= 0)
                parsed.leftMarginMm = value / 100.0;
            break;
        case 9:
            if (value >= 0)
                parsed.rightMarginMm = value / 100.0;
            break;
        case 10:
            if (value >= 0)
                parsed.headerMm = value / 100.0;
            break;
        case 11:
            if (value >= 0)
                parsed.footerMm = value / 100.0;
            break;
        case 16:
            if (value > 0)
                parsed.pageWidthMm = static_cast<int>(value);
            break;
        case 17:
            if (value > 0)
                parsed.pageHeightMm = static_cast<int>(value);
            break;
        default:
            parsed.extra.append({key, static_cast<int>(value)});
            break;
        }
    }
    *settings = parsed;
    return true;
}
bool findFormatAndFontTables(const V8TextNode &root, int start,
                             QVector<mxl::CellFormat> *formats,
                             QVector<mxl::FontSpec> *fonts, int *endIndex)
{
    for (int i = start; i + 2 < root.children.size(); ++i) {
        int count = 0;
        if (!intAt(root, i, &count) || count <= 0 || count > 10000)
            continue;
        if (i + count + 1 >= root.children.size())
            continue;

        bool formatsOk = true;
        for (int k = 0; k < count; ++k) {
            const V8TextNode &entry = root.children.at(i + 1 + k);
            int tag = 0;
            if (!entry.isList() || !nodeInt(childAt(entry, 0), &tag)) {
                formatsOk = false;
                break;
            }
        }
        if (!formatsOk)
            continue;

        const int fontsIndex = i + count + 1;
        int fontCount = 0;
        if (!intAt(root, fontsIndex, &fontCount) || fontCount <= 0 || fontCount > 1000)
            continue;
        if (fontsIndex + fontCount >= root.children.size())
            continue;

        bool fontsOk = true;
        for (int k = 0; k < fontCount; ++k) {
            const V8TextNode &entry = root.children.at(fontsIndex + 1 + k);
            int tag = 0;
            if (!entry.isList() || !nodeInt(childAt(entry, 0), &tag)
                || (tag != 6 && tag != 7)) {
                fontsOk = false;
                break;
            }
        }
        if (!fontsOk)
            continue;

        QVector<mxl::CellFormat> parsedFormats;
        parsedFormats.reserve(count);
        for (int k = 0; k < count; ++k)
            parsedFormats.append(formatFromValue(root.children.at(i + 1 + k)));

        QVector<mxl::FontSpec> parsedFonts;
        parsedFonts.reserve(fontCount);
        for (int k = 0; k < fontCount; ++k) {
            const V8TextNode &entry = root.children.at(fontsIndex + 1 + k);
            parsedFonts.append(fontFromValue(entry));
        }

        *formats = parsedFormats;
        *fonts = parsedFonts;
        if (endIndex)
            *endIndex = fontsIndex + 1 + fontCount;
        return true;
    }
    return false;
}

void parseTail(const V8TextNode &root, int start, mxl::Document *document)
{
    int index = start;

    mxl::ColumnGroup defaultGroup;
    const V8TextNode *groupNode = childAt(root, index);
    if (groupNode && columnGroupFromValue(*groupNode, &defaultGroup)) {
        document->defaultGroup = defaultGroup;
        ++index;

        int rowCount = 0;
        if (intAt(root, index, &rowCount))
            ++index;

        int extraCount = 0;
        if (intAt(root, index, &extraCount) && extraCount >= 0 && extraCount <= 1000) {
            QVector<mxl::ColumnGroup> extraGroups;
            bool groupsOk = true;
            for (int k = 0; k < extraCount; ++k) {
                const V8TextNode *extraNode = childAt(root, index + 1 + k);
                mxl::ColumnGroup group;
                if (!extraNode || !columnGroupFromValue(*extraNode, &group)) {
                    groupsOk = false;
                    break;
                }
                extraGroups.append(group);
            }
            if (groupsOk) {
                document->extraGroups = extraGroups;
                index += 1 + extraCount;

                int pairCount = 0;
                if (intAt(root, index, &pairCount) && pairCount >= 0
                    && pairCount <= kMaxRows) {
                    const int needed = 2 * pairCount;
                    bool pairsOk = index + needed < root.children.size();
                    QVector<QPair<int, int>> rowGroups;
                    for (int k = 0; pairsOk && k < pairCount; ++k) {
                        int row = 0;
                        int group = 0;
                        if (!intAt(root, index + 1 + 2 * k, &row)
                            || !intAt(root, index + 2 + 2 * k, &group)) {
                            pairsOk = false;
                            break;
                        }
                        if (row >= 0 && group >= 0)
                            rowGroups.append({row, group});
                    }
                    if (pairsOk) {
                        document->rowGroups = rowGroups;
                        index += 1 + needed;
                    }
                }
            }
        }
    }

    // Графические объекты: <число>, <N>, N записей.
    int drawingsMarker = 0;
    int drawingsCount = 0;
    if (intAt(root, index, &drawingsMarker) && intAt(root, index + 1, &drawingsCount)
        && drawingsCount >= 1 && drawingsCount <= 10000) {
        QVector<mxl::Drawing> drawings;
        bool ok = true;
        for (int k = 0; k < drawingsCount; ++k) {
            const V8TextNode *node = childAt(root, index + 2 + k);
            mxl::Drawing drawing;
            if (!node || !drawingFromValue(*node, &drawing)) {
                ok = false;
                break;
            }
            drawings.append(drawing);
        }
        if (ok) {
            document->drawings = drawings;
            index += 2 + drawingsCount;
        }
    }

    // Объединения ячеек: первый объект подходящей формы.
    for (int i = index; i < root.children.size(); ++i) {
        QVector<mxl::Merge> merges;
        if (mergesFromValue(root.children.at(i), &merges)) {
            document->merges = merges;
            break;
        }
    }

    // Таблицы форматов и шрифтов.
    QVector<mxl::CellFormat> formats;
    QVector<mxl::FontSpec> fonts;
    int formatsEnd = 0;
    if (findFormatAndFontTables(root, index, &formats, &fonts, &formatsEnd)) {
        document->formats = formats;
        document->fonts = fonts;

        QVector<mxl::Color> colors;
        int colorsEnd = 0;
        if (findColorTable(root, formatsEnd, &colors, &colorsEnd)) {
            document->colors = colors;
            const bool hasPictures =
                std::any_of(document->drawings.cbegin(), document->drawings.cend(),
                            [](const mxl::Drawing &drawing) {
                                return drawing.kind == mxl::DrawingKind::Picture;
                            });
            if (hasPictures) {
                QVector<QByteArray> pictures;
                if (findPictureTable(root, colorsEnd, &pictures))
                    document->pictures = pictures;
            }
        }
    }

    document->lines = findLineTable(root);

    // Именованные области и параметры печати ищутся по форме, а не по позиции:
    // их наличие/порядок зависит от версии и содержимого макета.
    for (const V8TextNode &field : root.children) {
        QVector<mxl::NamedArea> areas;
        if (document->namedAreas.isEmpty() && namedAreasFromValue(field, &areas)) {
            document->namedAreas = areas;
            continue;
        }
        mxl::PrintSettings settings;
        if (!document->printSettings.valid && printSettingsFromValue(field, &settings))
            document->printSettings = settings;
    }
}

bool validateHeader(const QByteArray &data, int *major, int *minor, int *sub,
                    QByteArray *body, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    if (data.size() >= 16 && data.startsWith("MOXCEL")) {
        if (data.size() >= 12 && data.at(6) == 0
            && data.at(7) == 0 && data.at(8) == 0 && data.at(9) == 0
            && data.at(10) == 0 && (data.at(11) == 6 || data.at(11) == 7)) {
            return fail(QStringLiteral(
                "Файл сохранён в формате 1С 7.7 (бинарный MOXCEL). "
                "Поддерживается только MXL 8.x."));
        }
        const auto word = [&data](int offset) {
            return static_cast<quint16>(static_cast<quint8>(data.at(offset))
                                        | (static_cast<quint16>(static_cast<quint8>(data.at(offset + 1))) << 8));
        };
        const int headerMajor = word(7);
        const int headerMinor = word(9);
        const int headerSub = word(11);
        if (headerMajor != 8 || headerMinor != 1)
            return fail(QStringLiteral("Неизвестная версия формата MXL (ожидался 8.1.x)."));
        if (data.at(13) != static_cast<char>(0xEF) || data.at(14) != static_cast<char>(0xBB)
            || data.at(15) != static_cast<char>(0xBF)) {
            return fail(QStringLiteral("Повреждённый MXL: после заголовка нет BOM UTF-8."));
        }
        if (major)
            *major = headerMajor;
        if (minor)
            *minor = headerMinor;
        if (sub)
            *sub = headerSub;
        *body = data.mid(16);
        return true;
    }

    // Допускаем «голое» тело без заголовка (например, если вызывающий код
    // уже отделил заголовок).
    QByteArray trimmed = data;
    while (!trimmed.isEmpty()
           && (trimmed.at(0) == ' ' || trimmed.at(0) == '\t'
               || trimmed.at(0) == '\r' || trimmed.at(0) == '\n')) {
        trimmed.remove(0, 1);
    }
    if (trimmed.isEmpty() || trimmed.at(0) != '{')
        return fail(QStringLiteral("Данные не являются табличным документом 1С (нет сигнатуры MOXCEL)."));
    *body = data;
    return true;
}

} // namespace

bool MxlReader::looksLikeMxl(const QByteArray &data)
{
    if (data.startsWith("MOXCEL"))
        return true;
    QByteArray trimmed = data;
    while (!trimmed.isEmpty()
           && (trimmed.at(0) == ' ' || trimmed.at(0) == '\t'
               || trimmed.at(0) == '\r' || trimmed.at(0) == '\n')) {
        trimmed.remove(0, 1);
    }
    return !trimmed.isEmpty() && trimmed.at(0) == '{';
}

bool MxlReader::parseBytes(const QByteArray &data, mxl::Document *document,
                           QString *error)
{
    if (!document)
        return false;
    *document = mxl::Document();

    QByteArray body;
    int major = 8;
    int minor = 1;
    int sub = 0;
    if (!validateHeader(data, &major, &minor, &sub, &body, error))
        return false;

    QString text = V8Text::decode(body, nullptr, error);
    if (text.isEmpty() && error && !error->isEmpty())
        return false;

    V8TextNode root;
    V8TextError parseError;
    if (!V8Text::parse(text, root, &parseError)) {
        if (error) {
            *error = QStringLiteral("Ошибка разбора MXL: %1")
                         .arg(parseError.toString());
        }
        return false;
    }
    if (!root.isList()) {
        if (error)
            *error = QStringLiteral("Некорректный MXL: корневой элемент не является списком.");
        return false;
    }

    document->versionMajor = major;
    document->versionMinor = minor;
    document->versionSub = sub;

    // Версия продублирована в начале корневого объекта.
    int rootMajor = 0;
    int rootMinor = 0;
    int rootSub = 0;
    if (intAt(root, 0, &rootMajor) && intAt(root, 1, &rootMinor)
        && intAt(root, 2, &rootSub)) {
        document->versionMajor = rootMajor;
        document->versionMinor = rootMinor;
        document->versionSub = rootSub;
    }

    RowsParseResult rows = parseRows(root, kRowsStartIndex);
    if (!rows.ok) {
        // На случай другой версии преамбулы ищем блок строк по форме.
        for (int start = 3; start < 40 && start < root.children.size(); ++start) {
            RowsParseResult candidate = parseRows(root, start);
            if (candidate.ok) {
                rows = candidate;
                break;
            }
        }
    }
    if (!rows.ok) {
        if (error)
            *error = QStringLiteral("Не удалось найти блок строк табличного документа.");
        return false;
    }

    document->rows = rows.rows;
    parseTail(root, rows.endIndex, document);
    document->valid = true;
    return true;
}

bool MxlReader::parseFile(const QString &path, mxl::Document *document,
                          QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Не удалось открыть файл: %1").arg(path);
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();
    return parseBytes(data, document, error);
}

} // namespace v8reader
