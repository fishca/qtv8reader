#ifndef QTV8READER_THEMECONTROLLER_H
#define QTV8READER_THEMECONTROLLER_H

#include <QObject>
#include <QPalette>
#include <QString>

class QAction;

namespace v8reader {

class BslHighlighter;

class ThemeController : public QObject
{
    Q_OBJECT

public:
    explicit ThemeController(QObject *parent = nullptr);

    void setHighlighter(BslHighlighter *highlighter);
    void apply(const QString &theme, bool persist);
    QString current() const { return m_theme; }
    bool isDark() const;

signals:
    void darkModeChanged(bool dark);

private:
    BslHighlighter *m_highlighter = nullptr;
    QPalette m_defaultPalette;
    QString m_defaultStyleName;
    QString m_theme = QStringLiteral("light");
};

} // namespace v8reader

#endif // QTV8READER_THEMECONTROLLER_H
