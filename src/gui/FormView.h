#ifndef QTV8READER_FORMVIEW_H
#define QTV8READER_FORMVIEW_H

#include "core/FormDocument.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QTabWidget;
class QTreeWidget;
class FormPreviewWidget;

namespace v8reader {

class BslHighlighter;

// Просмотр управляемой формы: дерево элементов, макет и модуль формы.
class FormView : public QWidget
{
    Q_OBJECT

public:
    explicit FormView(QWidget *parent = nullptr);

    void setDocument(const FormDocument &document);
    void setMessage(const QString &message);
    void clear();
    bool hasDocument() const { return m_hasDocument; }
    void setScalePercent(int percent);
    void setModuleHighlightEnabled(bool enabled);
    void setDarkMode(bool dark);
    const FormDocument &document() const { return m_document; }

private:
    void fillTree();

    QLabel *m_hint = nullptr;
    QTreeWidget *m_tree = nullptr;
    QComboBox *m_scaleCombo = nullptr;
    FormPreviewWidget *m_preview = nullptr;
    QTabWidget *m_tabs = nullptr;
    QPlainTextEdit *m_moduleEdit = nullptr;
    BslHighlighter *m_highlighter = nullptr;
    FormDocument m_document;
    bool m_hasDocument = false;
};

} // namespace v8reader

#endif // QTV8READER_FORMVIEW_H
