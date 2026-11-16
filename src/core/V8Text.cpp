#include "core/V8Text.h"

#include <QRegularExpression>
#include <QStringConverter>
#include <QStringDecoder>

#include <algorithm>

namespace v8reader {
namespace {

bool hasUtf16LeBom(const QByteArray &data)
{
    return data.size() >= 2
        && static_cast<unsigned char>(data.at(0)) == 0xff
        && static_cast<unsigned char>(data.at(1)) == 0xfe;
}

bool hasUtf16BeBom(const QByteArray &data)
{
    return data.size() >= 2
        && static_cast<unsigned char>(data.at(0)) == 0xfe
        && static_cast<unsigned char>(data.at(1)) == 0xff;
}

QString decodeUtf16(const QByteArray &data, bool littleEndian)
{
    const int start = 2;
    const int count = (data.size() - start) / 2;
    QString result;
    result.reserve(count);

    for (int i = 0; i < count; ++i) {
        const int offset = start + i * 2;
        const auto b0 = static_cast<unsigned char>(data.at(offset));
        const auto b1 = static_cast<unsigned char>(data.at(offset + 1));
        const ushort codeUnit = littleEndian
            ? static_cast<ushort>(b0 | (b1 << 8))
            : static_cast<ushort>((b0 << 8) | b1);
        result.append(QChar(codeUnit));
    }

    if (!result.isEmpty() && result.at(0) == QChar(0xfeff))
        result.remove(0, 1);

    return result;
}

bool looksLikeUtf16(const QByteArray &data, bool *littleEndian)
{
    const int sampleSize = static_cast<int>(std::min<qsizetype>(data.size(), 512));
    if (sampleSize < 4)
        return false;

    int zeroEven = 0;
    int zeroOdd = 0;
    int asciiEven = 0;
    int asciiOdd = 0;

    for (int i = 0; i < sampleSize; ++i) {
        const unsigned char byte = static_cast<unsigned char>(data.at(i));
        const bool zero = byte == 0;
        const bool ascii = byte >= 0x09 && byte < 0x7f;
        if ((i & 1) == 0) {
            zeroEven += zero ? 1 : 0;
            asciiEven += ascii ? 1 : 0;
        } else {
            zeroOdd += zero ? 1 : 0;
            asciiOdd += ascii ? 1 : 0;
        }
    }

    const int evenCount = (sampleSize + 1) / 2;
    const int oddCount = sampleSize / 2;

    // UTF-16LE ASCII text has non-zero bytes at even positions and zeroes at
    // odd positions; UTF-16BE is the opposite.
    if (zeroOdd > oddCount * 0.7 && asciiEven > evenCount * 0.3) {
        *littleEndian = true;
        return true;
    }
    if (zeroEven > evenCount * 0.7 && asciiOdd > oddCount * 0.3) {
        *littleEndian = false;
        return true;
    }

    return false;
}

QString decodeWithEncoding(const QByteArray &data, const char *encoding, bool *ok)
{
    QStringDecoder decoder(encoding);
    if (!decoder.isValid()) {
        if (ok)
            *ok = false;
        return QString();
    }

    const QString result = decoder.decode(data);
    if (ok)
        *ok = !decoder.hasError();
    return result;
}

class Parser
{
public:
    Parser(const QString &text, V8TextError *error)
        : m_text(text)
        , m_error(error)
    {
    }

    bool parse(V8TextNode &root)
    {
        skipWhitespace();
        if (atEnd()) {
            fail(QStringLiteral("Пустой текст"));
            return false;
        }

        if (!parseValue(root))
            return false;

        skipWhitespace();
        if (!atEnd()) {
            fail(QStringLiteral("Лишние данные после значения"));
            return false;
        }

        return true;
    }

    bool parseSequence(V8TextNode &root)
    {
        skipWhitespace();
        if (atEnd()) {
            fail(QStringLiteral("Пустой текст"));
            return false;
        }

        V8TextNode first;
        if (!parseValue(first))
            return false;

        skipWhitespace();
        if (atEnd()) {
            root = std::move(first);
            return true;
        }

        V8TextNode sequence;
        sequence.type = V8TextNode::Type::List;
        sequence.line = first.line;
        sequence.column = first.column;
        sequence.children.append(std::move(first));

        while (true) {
            const QChar separator = current();
            if (separator != QLatin1Char(',')) {
                fail(QStringLiteral("Ожидалась запятая между значениями"));
                return false;
            }
            advance(); // ','

            skipWhitespace();
            if (atEnd()) {
                fail(QStringLiteral("Неожиданный конец текста после запятой"));
                return false;
            }

            V8TextNode value;
            if (!parseValue(value))
                return false;
            sequence.children.append(std::move(value));

            skipWhitespace();
            if (atEnd()) {
                root = std::move(sequence);
                return true;
            }
        }
    }

private:
    bool atEnd() const { return m_pos >= m_text.size(); }
    QChar current() const { return atEnd() ? QChar() : m_text.at(m_pos); }

    void advance()
    {
        if (atEnd())
            return;
        if (m_text.at(m_pos) == QLatin1Char('\n')) {
            ++m_line;
            m_column = 1;
        } else {
            ++m_column;
        }
        ++m_pos;
    }

    void skipWhitespace()
    {
        while (!atEnd() && current().isSpace())
            advance();
    }

    void fail(const QString &message)
    {
        if (!m_error)
            return;
        m_error->line = m_line;
        m_error->column = m_column;
        m_error->message = message;
    }

    bool parseValue(V8TextNode &node)
    {
        skipWhitespace();
        if (atEnd()) {
            fail(QStringLiteral("Неожиданный конец текста"));
            return false;
        }

        node.line = m_line;
        node.column = m_column;

        if (current() == QLatin1Char('{'))
            return parseList(node);
        if (current() == QLatin1Char('"'))
            return parseString(node);

        return parsePlainValue(node);
    }

    bool parseList(V8TextNode &node)
    {
        node.type = V8TextNode::Type::List;
        advance(); // {

        skipWhitespace();
        if (atEnd()) {
            fail(QStringLiteral("Не закрыта фигурная скобка"));
            return false;
        }
        if (current() == QLatin1Char('}')) {
            advance();
            return true;
        }

        bool afterComma = false;
        while (true) {
            skipWhitespace();
            if (atEnd()) {
                fail(QStringLiteral("Не закрыта фигурная скобка"));
                return false;
            }

            if (current() == QLatin1Char('}')) {
                if (afterComma) {
                    V8TextNode empty;
                    empty.type = V8TextNode::Type::Value;
                    empty.line = m_line;
                    empty.column = m_column;
                    node.children.append(empty);
                }
                advance();
                return true;
            }

            if (current() == QLatin1Char(',')) {
                V8TextNode empty;
                empty.type = V8TextNode::Type::Value;
                empty.line = m_line;
                empty.column = m_column;
                node.children.append(empty);
                afterComma = true;
                advance();
                continue;
            }

            V8TextNode child;
            if (!parseValue(child))
                return false;
            node.children.append(child);
            afterComma = false;

            skipWhitespace();
            if (atEnd()) {
                fail(QStringLiteral("Не закрыта фигурная скобка"));
                return false;
            }

            if (current() == QLatin1Char(',')) {
                afterComma = true;
                advance();
                continue;
            }
            if (current() == QLatin1Char('}')) {
                advance();
                return true;
            }

            fail(QStringLiteral("Ожидалась запятая или закрывающая скобка"));
            return false;
        }
    }

    bool parseString(V8TextNode &node)
    {
        node.type = V8TextNode::Type::String;
        advance(); // opening quote

        while (!atEnd()) {
            const QChar ch = current();
            if (ch == QLatin1Char('"')) {
                advance();
                if (!atEnd() && current() == QLatin1Char('"')) {
                    node.value.append(QLatin1Char('"'));
                    advance();
                    continue;
                }
                return true;
            }

            node.value.append(ch);
            advance();
        }

        fail(QStringLiteral("Не закрыта кавычка"));
        return false;
    }

    bool parsePlainValue(V8TextNode &node)
    {
        node.type = V8TextNode::Type::Value;
        const int start = m_pos;

        while (!atEnd()) {
            const QChar ch = current();
            if (ch == QLatin1Char(',') || ch == QLatin1Char('}'))
                break;
            advance();
        }

        node.value = m_text.mid(start, m_pos - start).trimmed();
        return true;
    }

    const QString &m_text;
    V8TextError *m_error = nullptr;
    int m_pos = 0;
    int m_line = 1;
    int m_column = 1;
};

QString quoteString(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

bool containsList(const V8TextNode &node)
{
    for (const V8TextNode &child : node.children) {
        if (child.isList())
            return true;
    }
    return false;
}

void serializeNode(const V8TextNode &node, bool pretty, int indent, QString &out)
{
    if (node.isScalar()) {
        if (node.type == V8TextNode::Type::String)
            out += quoteString(node.value);
        else
            out += node.value;
        return;
    }

    out += QLatin1Char('{');

    if (node.children.isEmpty()) {
        out += QLatin1Char('}');
        return;
    }

    const bool multiline = pretty && containsList(node);
    if (multiline)
        out += QLatin1Char('\n');

    for (int i = 0; i < node.children.size(); ++i) {
        if (i > 0) {
            out += QLatin1Char(',');
            if (multiline)
                out += QLatin1Char('\n');
        }
        if (multiline)
            out += QString(indent + 2, QLatin1Char(' '));
        serializeNode(node.children.at(i), pretty, indent + 2, out);
    }

    if (multiline)
        out += QLatin1Char('\n') + QString(indent, QLatin1Char(' '));
    out += QLatin1Char('}');
}

} // namespace

QString V8TextError::toString() const
{
    if (line <= 0)
        return message;
    return QStringLiteral("Строка %1, позиция %2: %3").arg(line).arg(column).arg(message);
}

bool V8TextNode::isInteger() const
{
    if (type == Type::List)
        return false;
    bool ok = false;
    value.trimmed().toLongLong(&ok);
    return ok;
}

bool V8TextNode::isGuid() const
{
    return type != Type::List && V8Text::isGuid(value);
}

qint64 V8TextNode::toInteger(bool *ok) const
{
    if (type == Type::List) {
        if (ok)
            *ok = false;
        return 0;
    }
    return value.trimmed().toLongLong(ok);
}

QString V8TextNode::normalizedGuid() const
{
    return type == Type::List ? QString() : V8Text::normalizeGuid(value);
}

const V8TextNode *V8TextNode::child(int index) const
{
    if (index < 0 || index >= children.size())
        return nullptr;
    return &children.at(index);
}

V8TextNode *V8TextNode::child(int index)
{
    if (index < 0 || index >= children.size())
        return nullptr;
    return &children[index];
}

QString V8Text::decode(const QByteArray &data, QString *encodingName, QString *error)
{
    if (error)
        error->clear();
    if (encodingName)
        encodingName->clear();

    if (data.isEmpty())
        return QString();

    if (data.startsWith("\xef\xbb\xbf")) {
        if (encodingName)
            *encodingName = QStringLiteral("UTF-8 (BOM)");
        return QString::fromUtf8(data.constData() + 3, data.size() - 3);
    }

    if (hasUtf16LeBom(data)) {
        if (encodingName)
            *encodingName = QStringLiteral("UTF-16LE (BOM)");
        return decodeUtf16(data, true);
    }

    if (hasUtf16BeBom(data)) {
        if (encodingName)
            *encodingName = QStringLiteral("UTF-16BE (BOM)");
        return decodeUtf16(data, false);
    }

    bool littleEndian = true;
    if (looksLikeUtf16(data, &littleEndian)) {
        if (encodingName)
            *encodingName = littleEndian ? QStringLiteral("UTF-16LE")
                                         : QStringLiteral("UTF-16BE");
        return decodeUtf16(data, littleEndian);
    }

    QStringDecoder utf8Decoder(QStringConverter::Utf8);
    QString utf8 = utf8Decoder.decode(data);
    if (!utf8Decoder.hasError()) {
        if (!utf8.isEmpty() && utf8.at(0) == QChar(0xfeff))
            utf8.remove(0, 1);
        if (encodingName)
            *encodingName = QStringLiteral("UTF-8");
        return utf8;
    }

    bool cp1251Ok = false;
    const QString cp1251 = decodeWithEncoding(data, "windows-1251", &cp1251Ok);
    if (cp1251Ok) {
        if (encodingName)
            *encodingName = QStringLiteral("Windows-1251");
        return cp1251;
    }

    if (error)
        *error = QStringLiteral("Не удалось определить кодировку текста");
    if (encodingName)
        *encodingName = QStringLiteral("System");
    return QString::fromLocal8Bit(data);
}

bool V8Text::parse(const QString &text, V8TextNode &root, V8TextError *error)
{
    if (error)
        *error = V8TextError();
    root = V8TextNode();

    Parser parser(text, error);
    return parser.parse(root);
}

bool V8Text::parseSequence(const QString &text, V8TextNode &root, V8TextError *error)
{
    if (error)
        *error = V8TextError();
    root = V8TextNode();

    Parser parser(text, error);
    return parser.parseSequence(root);
}

QString V8Text::serialize(const V8TextNode &node, bool pretty)
{
    QString result;
    serializeNode(node, pretty, 0, result);
    return result;
}

bool V8Text::isGuid(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.size() != 36)
        return false;

    for (int i = 0; i < trimmed.size(); ++i) {
        const QChar ch = trimmed.at(i);
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (ch != QLatin1Char('-'))
                return false;
            continue;
        }
        if (!ch.isDigit() && !(ch >= QLatin1Char('a') && ch <= QLatin1Char('f'))
            && !(ch >= QLatin1Char('A') && ch <= QLatin1Char('F'))) {
            return false;
        }
    }

    return true;
}

QString V8Text::normalizeGuid(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (isGuid(trimmed))
        return trimmed.toLower();
    return QString();
}

QString V8Text::findFirstGuid(const V8TextNode &node)
{
    if (node.isScalar()) {
        const QString guid = normalizeGuid(node.value);
        if (!guid.isEmpty())
            return guid;
        return QString();
    }

    for (const V8TextNode &child : node.children) {
        const QString guid = findFirstGuid(child);
        if (!guid.isEmpty())
            return guid;
    }

    return QString();
}

bool V8Text::decodeBase64Value(const QString &value, QByteArray *result)
{
    QString payload = value.trimmed();
    if (payload.startsWith(QLatin1String("#base64:")))
        payload.remove(0, 8);
    else if (payload.startsWith(QLatin1String("#data:")))
        payload.remove(0, 6);
    else
        return false;

    payload.remove(QRegularExpression(QStringLiteral("\\s+")));
    if (payload.isEmpty())
        return false;

    const QByteArray decoded = QByteArray::fromBase64(payload.toLatin1());
    if (decoded.isEmpty() && !payload.isEmpty())
        return false;

    if (result)
        *result = decoded;
    return true;
}

} // namespace v8reader
