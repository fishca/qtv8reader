#ifndef QTV8READER_FORMVIEW_H
#define QTV8READER_FORMVIEW_H

#include "core/FormDocument.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QTreeWidget;
class FormPreviewWidget;

namespace v8reader {

// Просмотр управляемой формы: дерево элементов и схематичный макет.
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
    const FormDocument &document() const { return m_document; }

private:
    void fillTree();

    QLabel *m_hint = nullptr;
    QTreeWidget *m_tree = nullptr;
    QComboBox *m_scaleCombo = nullptr;
    FormPreviewWidget *m_preview = nullptr;
    FormDocument m_document;
    bool m_hasDocument = false;
};

} // namespace v8reader

#endif // QTV8READER_FORMVIEW_H
