#ifndef QTV8READER_BSLHIGHLIGHTSETTINGS_H
#define QTV8READER_BSLHIGHLIGHTSETTINGS_H

#include <QColor>
#include <QSettings>

namespace v8reader {

// Настройки подсветки синтаксиса BSL. Если custom == false, цвета берутся
// из светлой/тёмной темы; изменение цвета делает настройки пользовательскими.
struct BslHighlightSettings
{
    bool enabled = true;
    bool custom = false;
    QColor keyword;
    QColor directive;
    QColor annotation;
    QColor number;
    QColor string;
    QColor comment;
    bool boldKeywords = true;
    bool boldDirectives = true;
    bool italicComments = true;

    static BslHighlightSettings defaults(bool dark);
    static BslHighlightSettings load(QSettings &settings, bool dark);
    void save(QSettings &settings) const;
};

} // namespace v8reader

#endif // QTV8READER_BSLHIGHLIGHTSETTINGS_H
