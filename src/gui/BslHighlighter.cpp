#include "gui/BslHighlighter.h"

#include <QColor>
#include <QFont>
#include <QStringList>

namespace v8reader {

BslHighlighter::BslHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document),
      m_settings(BslHighlightSettings::defaults(false))
{
    rebuildFormats();
}

void BslHighlighter::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    m_settings.enabled = enabled;
    rehighlight();
}

void BslHighlighter::setSettings(const BslHighlightSettings &settings)
{
    m_settings = settings;
    m_enabled = settings.enabled;
    rebuildFormats();
    rehighlight();
}

void BslHighlighter::setDarkMode(bool dark)
{
    if (m_dark == dark)
        return;
    m_dark = dark;
    if (!m_settings.custom) {
        const bool enabled = m_settings.enabled;
        m_settings = BslHighlightSettings::defaults(dark);
        m_settings.enabled = enabled;
    }
    rebuildFormats();
    rehighlight();
}

void BslHighlighter::rebuildFormats()
{
    m_rules.clear();

    QTextCharFormat keyword;
    keyword.setForeground(m_settings.keyword);
    keyword.setFontWeight(m_settings.boldKeywords ? QFont::Bold : QFont::Normal);

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
            QRegularExpression::CaseInsensitiveOption
                | QRegularExpression::UseUnicodePropertiesOption);
        rule.format = keyword;
        m_rules.append(rule);
    }

    QTextCharFormat directive;
    directive.setForeground(m_settings.directive);
    directive.setFontWeight(m_settings.boldDirectives ? QFont::Bold : QFont::Normal);
    Rule directiveRule;
    directiveRule.pattern = QRegularExpression(
        QStringLiteral("^\\s*#\\w+"), QRegularExpression::UseUnicodePropertiesOption);
    directiveRule.format = directive;
    m_rules.append(directiveRule);

    QTextCharFormat annotation;
    annotation.setForeground(m_settings.annotation);
    Rule annotationRule;
    annotationRule.pattern = QRegularExpression(
        QStringLiteral("&\\w+"), QRegularExpression::UseUnicodePropertiesOption);
    annotationRule.format = annotation;
    m_rules.append(annotationRule);

    QTextCharFormat number;
    number.setForeground(m_settings.number);
    Rule numberRule;
    numberRule.pattern = QRegularExpression(QStringLiteral("\\b\\d+(\\.\\d+)?\\b"));
    numberRule.format = number;
    m_rules.append(numberRule);

    m_stringFormat = QTextCharFormat();
    m_stringFormat.setForeground(m_settings.string);

    m_commentFormat = QTextCharFormat();
    m_commentFormat.setForeground(m_settings.comment);
    m_commentFormat.setFontItalic(m_settings.italicComments);
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
