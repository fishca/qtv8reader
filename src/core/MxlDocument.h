#ifndef QTV8READER_MXLDOCUMENT_H
#define QTV8READER_MXLDOCUMENT_H

#include <QByteArray>
#include <QPair>
#include <QString>
#include <QVector>

namespace v8reader {
namespace mxl {

// Цвет из таблицы цветов документа. В файле хранится COLORREF (R | G<<8 | B<<16).
struct Color
{
    enum class Kind {
        Rgb,
        Auto
    };

    Kind kind = Kind::Auto;
    quint32 rgb = 0;

    bool isRgb() const { return kind == Kind::Rgb; }
    int red() const { return static_cast<int>(rgb & 0xFF); }
    int green() const { return static_cast<int>((rgb >> 8) & 0xFF); }
    int blue() const { return static_cast<int>((rgb >> 16) & 0xFF); }
};

// Стиль линии рамки (таблица линий документа).
struct LineStyle
{
    // 0 — нет линии/авто, 1 — сплошная, 2 — точечная, 3 — двойная,
    // 4 — редкий пунктир, 5 — частый пунктир, 6 — большой пунктир.
    int kind = 0;
    int thickness = 0;
};

// Шрифт из таблицы шрифтов документа.
struct FontSpec
{
    QString family;
    int sizeTenthPt = 80; // 80 = 8 pt
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikeout = false;
};

// Формат ячейки/строки/колонки. -1 — свойство не задано.
struct CellFormat
{
    int font = -1;          // 0-базный индекс в таблице шрифтов
    int borderLeft = -1;    // 0-базные индексы в таблице линий
    int borderTop = -1;
    int borderRight = -1;
    int borderBottom = -1;
    int bordersColor = -1;  // 0-базный индекс в таблице цветов
    int height = -1;        // 1/288 дюйма; 0 — автоподбор
    int width = -1;         // 1/8 ширины стандартного символа
    int hAlign = -1;        // 0 лево, 2 право, 4 по ширине, 6 центр
    int vAlign = -1;        // 0 верх, 8 низ, 24 центр
    int textColor = -1;     // 0-базный индекс в таблице цветов
    int bgColor = -1;       // 0-базный индекс в таблице цветов
    int pattern = -1;       // 255 нет узора, 0 сплошной, 1..17 штриховки
    int patternColor = -1;  // 0-базный индекс в таблице цветов
    int textControl = -1;   // 0 авто, 1 обрезать, 2 забивать, 3 переносить
    int orientation = -1;   // угол в десятых долях градуса (900 = 90°)
};

struct Cell
{
    int format = 0; // 1-базный индекс в таблице форматов; 0 — формата нет
    QString text;
};

struct Row
{
    int index = 0; // номер строки в документе
    int format = 0; // 1-базный индекс формата строки; 0 — нет
    QVector<QPair<int, Cell>> cells; // (номер колонки, ячейка)
};

struct Merge
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

// Группа колонок: у разных диапазонов строк могут быть разные ширины.
struct ColumnGroup
{
    QVector<QPair<int, int>> columns; // (номер колонки, 1-базный индекс формата)
};

enum class DrawingKind {
    Rectangle,
    Text,
    Picture,
    Other
};

// Графический объект листа (прямоугольник, надпись, картинка).
struct Drawing
{
    DrawingKind kind = DrawingKind::Other;
    int colStart = 0;
    int rowStart = 0;
    int colEnd = 0;
    int rowEnd = 0;
    int offsetLeft = 0;
    int offsetTop = 0;
    int offsetRight = 0;
    int offsetBottom = 0;
    int format = 0;
    QString text;
    int picture = -1; // 1-базный индекс в таблице картинок
};

// Разобранный табличный документ MXL8.
struct Document
{
    bool valid = false;
    int versionMajor = 0;
    int versionMinor = 0;
    int versionSub = 0;

    QVector<Row> rows;
    QVector<CellFormat> formats;
    QVector<FontSpec> fonts;
    QVector<LineStyle> lines;
    QVector<Color> colors;
    QVector<Merge> merges;
    ColumnGroup defaultGroup;
    QVector<ColumnGroup> extraGroups;
    QVector<QPair<int, int>> rowGroups; // (номер строки, индекс доп. группы)
    QVector<Drawing> drawings;
    QVector<QByteArray> pictures;

    int rowCount() const { return rows.size(); }
    int maxColumn() const;
    int maxRowIndex() const;

    // Формат по 1-базному индексу (0 → nullptr).
    const CellFormat *formatAt(int index1based) const;

    // RGB-цвет по 0-базному индексу; false для авто/отсутствия.
    bool colorAt(int index, quint32 *rgb) const;

    // Действующая группа колонок для строки с данным номером.
    const ColumnGroup &groupForRow(int rowIndex) const;

    // Ячейка по номеру строки/колонки (без учёта объединений).
    const Cell *cellAt(int rowIndex, int column) const;

    // Верхняя-левая ячейка объединения, покрывающего (row, column).
    const Merge *mergeAt(int rowIndex, int column) const;
};

} // namespace mxl
} // namespace v8reader

#endif // QTV8READER_MXLDOCUMENT_H
