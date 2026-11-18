#ifndef QTV8READER_MODULEPANEL_H
#define QTV8READER_MODULEPANEL_H

#include "core/ModuleExtractor.h"

#include <QVector>
#include <QWidget>

class QComboBox;
class QPlainTextEdit;

namespace v8reader {

class BslHighlighter;

// Вкладка «Модуль»: выбор модуля объекта и текст с подсветкой.
class ModulePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ModulePanel(QWidget *parent = nullptr);

    void setModules(const QVector<ModuleInfo> &modules);
    void showSingleModule(const ModuleInfo &module);
    void setCurrentIndex(int index);
    int currentIndex() const;
    QString text() const;
    void setPlaceholder(const QString &text);
    void setHighlightEnabled(bool enabled);
    void setDarkMode(bool dark);

signals:
    void moduleChanged(int index);

private:
    QComboBox *m_combo = nullptr;
    QPlainTextEdit *m_edit = nullptr;
    BslHighlighter *m_highlighter = nullptr;
    QVector<ModuleInfo> m_modules;
};

} // namespace v8reader

#endif // QTV8READER_MODULEPANEL_H
