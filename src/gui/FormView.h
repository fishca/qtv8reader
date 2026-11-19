#ifndef QTV8READER_FORMVIEW_H
#define QTV8READER_FORMVIEW_H

#include "core/FormDocument.h"
#include "gui/BslHighlightSettings.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QResizeEvent;
class QShowEvent;
class QSplitter;
class QTabWidget;
class QToolButton;
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
    void setHighlightSettings(const BslHighlightSettings &settings);
    void setDarkMode(bool dark);
    const FormDocument &document() const { return m_document; }

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void fillTree();

    QLabel *m_hint = nullptr;
    QTreeWidget *m_tree = nullptr;
    QSplitter *m_splitter = nullptr;
    QToolButton *m_treeToggle = nullptr;
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
