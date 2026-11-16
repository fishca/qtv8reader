#include "gui/BslHighlighter.h"

#include <QColor>
#include <QFont>
#include <QStringList>

namespace v8reader {

BslHighlighter::BslHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
{
    rebuildFormats();
}

void BslHighlighter::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    rehighlight();
}

void BslHighlighter::setDarkMode(bool dark)
{
    if (m_dark == dark)
        return;
    m_dark = dark;
    rebuildFormats();
    rehighlight();
}

void BslHighlighter::rebuildFormats()
{
    m_rules.clear();

    QTextCharFormat keyword;
    keyword.setForeground(m_dark ? QColor(0x56, 0x9C, 0xD6) : QColor(0x00, 0x00, 0xA0));
    keyword.setFontWeight(QFont::Bold);

    const QStringList keywords = {
        QStringLiteral("Процедура"), QStringLiteral("Функция"),
        QStringLiteral("КонецПроцедуры"), QStringLiteral("КонецФункции"),
        QStringLiteral("Если"), QStringLiteral("Тогда"), QStringLiteral("Иначе"),
        QStringLiteral("ИначеЕсли"), QStringLiteral("КонецЕсли"),
        QStringLiteral("Для"), QStringLiteral("Каждого"), QStringLiteral("Из"),
        QStringLiteral("По"), QStringLiteral("Цикл"), QStringLiteral("КонецЦикла"),
        QStringLiteral("Пока"), QStringLiteral("Возврат"), QStringLiteral("Перем"),
        QStringLiteral("Экспорт"), QStringLiteral("Знач"), QStringLiteral("Новый"),
        QStringLiteral("Не"), QStringLiteral("И"), QStringLiteral("Или"),
        QStringLiteral("Истина"), QStringLiteral("Ложь"),
        QStringLiteral("Неопределено"), QStringLiteral("Попытка"),
        QStringLiteral("Исключение"), QStringLiteral("КонецПопытки"),
        QStringLiteral("ВызватьИсключение"), QStringLiteral("Перейти"),
        QStringLiteral("Продолжить"), QStringLiteral("Прервать"),
        QStringLiteral("Ждать"), QStringLiteral("Асинх"), QStringLiteral("Выполнить"),
        QStringLiteral("Procedure"), QStringLiteral("Function"),
        QStringLiteral("EndProcedure"), QStringLiteral("EndFunction"),
        QStringLiteral("If"), QStringLiteral("Then"), QStringLiteral("Else"),
        QStringLiteral("ElsIf"), QStringLiteral("EndIf"), QStringLiteral("For"),
        QStringLiteral("Each"), QStringLiteral("In"), QStringLiteral("To"),
        QStringLiteral("Do"), QStringLiteral("While"), QStringLiteral("EndDo"),
        QStringLiteral("Return"), QStringLiteral("Var"), QStringLiteral("Export"),
        QStringLiteral("Val"), QStringLiteral("New"), QStringLiteral("Not"),
        QStringLiteral("And"), QStringLiteral("Or"), QStringLiteral("True"),
        QStringLiteral("False"), QStringLiteral("Undefined"), QStringLiteral("Try"),
        QStringLiteral("Except"), QStringLiteral("EndTry"), QStringLiteral("Raise"),
        QStringLiteral("Goto"), QStringLiteral("Continue"), QStringLiteral("Break"),
        QStringLiteral("Async"),
    };
    for (const QString &word : keywords) {
        Rule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral("\\b%1\\b").arg(word),
            QRegularExpression::CaseInsensitiveOption);
        rule.format = keyword;
        m_rules.append(rule);
    }

    QTextCharFormat directive;
    directive.setForeground(m_dark ? QColor(0xC5, 0x86, 0xC0) : QColor(0x80, 0x00, 0x80));
    directive.setFontWeight(QFont::Bold);
    Rule directiveRule;
    directiveRule.pattern = QRegularExpression(QStringLiteral("^\\s*#\\w+"));
    directiveRule.format = directive;
    m_rules.append(directiveRule);

    QTextCharFormat annotation;
    annotation.setForeground(m_dark ? QColor(0x4E, 0xC9, 0xB0) : QColor(0x00, 0x80, 0x80));
    Rule annotationRule;
    annotationRule.pattern = QRegularExpression(QStringLiteral("&\\w+"));
    annotationRule.format = annotation;
    m_rules.append(annotationRule);

    QTextCharFormat number;
    number.setForeground(m_dark ? QColor(0xB5, 0xCE, 0xA8) : QColor(0x99, 0x33, 0x33));
    Rule numberRule;
    numberRule.pattern = QRegularExpression(QStringLiteral("\\b\\d+(\\.\\d+)?\\b"));
    numberRule.format = number;
    m_rules.append(numberRule);

    m_stringFormat = QTextCharFormat();
    m_stringFormat.setForeground(m_dark ? QColor(0xCE, 0x91, 0x78)
                                        : QColor(0xA3, 0x15, 0x15));

    m_commentFormat = QTextCharFormat();
    m_commentFormat.setForeground(m_dark ? QColor(0x6A, 0x99, 0x55)
                                         : QColor(0x00, 0x80, 0x00));
    m_commentFormat.setFontItalic(true);
}

void BslHighlighter::applyRules(const QString &text, int from, int to)
{
    if (to <= from)
        return;

    const QString segment = text.mid(from, to - from);
    for (const Rule &rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(segment);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            setFormat(from + match.capturedStart(), match.capturedLength(),
                      rule.format);
        }
    }
}

void BslHighlighter::highlightBlock(const QString &text)
{
    if (!m_enabled)
        return;

    // Сначала находим строки и комментарии: их не должны перекрывать правила
    // ключевых слов и чисел.
    struct Range
    {
        int start;
        int length;
        bool comment;
    };
    QVector<Range> ranges;

    int i = 0;
    while (i < text.size()) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('"')) {
            const int start = i;
            ++i;
            while (i < text.size()) {
                if (text.at(i) == QLatin1Char('"')) {
                    if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('"')) {
                        i += 2;
                        continue;
                    }
                    ++i;
                    break;
                }
                ++i;
            }
            ranges.append({start, i - start, false});
            continue;
        }
        if (ch == QLatin1Char('/') && i + 1 < text.size()
            && text.at(i + 1) == QLatin1Char('/')) {
            ranges.append({i, static_cast<int>(text.size() - i), true});
            break;
        }
        ++i;
    }

    int position = 0;
    for (const Range &range : ranges) {
        applyRules(text, position, range.start);
        setFormat(range.start, range.length,
                  range.comment ? m_commentFormat : m_stringFormat);
        position = range.start + range.length;
    }
    applyRules(text, position, text.size());
}

} // namespace v8reader
