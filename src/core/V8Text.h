#ifndef QTV8READER_V8TEXT_H
#define QTV8READER_V8TEXT_H

#include <QByteArray>
#include <QString>
#include <QVector>

namespace v8reader {

struct V8TextNode
{
    enum class Type {
        List,
        String,
        Value
    };

    Type type = Type::Value;
    QString value;
    QVector<V8TextNode> children;
    int line = 1;
    int column = 1;

    bool isList() const { return type == Type::List; }
    bool isString() const { return type == Type::String; }
    bool isScalar() const { return type != Type::List; }
    bool isInteger() const;
    bool isGuid() const;
    bool isEmpty() const { return type == Type::List ? children.isEmpty() : value.isEmpty(); }

    qint64 toInteger(bool *ok = nullptr) const;
    QString normalizedGuid() const;

    const V8TextNode *child(int index) const;
    V8TextNode *child(int index);

    QString scalar() const { return type == Type::List ? QString() : value; }
};

struct V8TextError
{
    int line = 0;
    int column = 0;
    QString message;

    bool isValid() const { return line > 0; }
    QString toString() const;
};

class V8Text
{
public:
    static QString decode(const QByteArray &data,
                          QString *encodingName = nullptr,
                          QString *error = nullptr);

    static bool parse(const QString &text, V8TextNode &root, V8TextError *error = nullptr);

    // Разбор текста, в котором может быть несколько значений верхнего уровня
    // (например, элемент configinfo расширения конфигурации). Если значение
    // одно, результат совпадает с parse(). Если значений несколько, корнем
    // становится список из них.
    static bool parseSequence(const QString &text, V8TextNode &root,
                              V8TextError *error = nullptr);

    static QString serialize(const V8TextNode &node, bool pretty = false);

    static bool isGuid(const QString &value);
    static QString normalizeGuid(const QString &value);

    static QString findFirstGuid(const V8TextNode &node);

    static bool decodeBase64Value(const QString &value, QByteArray *result);

private:
    V8Text() = delete;
};

} // namespace v8reader

#endif // QTV8READER_V8TEXT_H
