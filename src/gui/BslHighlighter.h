#ifndef QTV8READER_BSLHIGHLIGHTER_H
#define QTV8READER_BSLHIGHLIGHTER_H

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

class QTextDocument;

namespace v8reader {

// Подсветка текста модуля 1С (встроенный язык): ключевые слова, строки,
// комментарии, числа, директивы препроцессора и аннотации.
class BslHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit BslHighlighter(QTextDocument *document);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // Цвета для тёмной/светлой темы.
    void setDarkMode(bool dark);
    bool isDarkMode() const { return m_dark; }

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    void rebuildFormats();
    void applyRules(const QString &text, int from, int to);

    QVector<Rule> m_rules;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_commentFormat;
    bool m_enabled = true;
    bool m_dark = false;
};

} // namespace v8reader

#endif // QTV8READER_BSLHIGHLIGHTER_H
