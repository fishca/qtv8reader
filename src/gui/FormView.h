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
class QStackedWidget;
class QTabWidget;
class QToolButton;
class QTreeWidget;
class FormPreviewWidget;

namespace v8reader {

class BslHighlighter;
class WebView2Host;

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

    // «Точный вид»: рендер формы браузерным движком (WebView2).
    void setExactViewAvailability(bool available, const QString &reason);
    void setExactViewEnabled(bool enabled);
    bool exactViewEnabled() const { return m_exactEnabled; }
    // Модель формы для точного вида: JSON, который отдаёт страница рендерера.
    void setExactFormModel(const QByteArray &modelJson, const QString &caption);
    void showExactViewMessage(const QString &message);

signals:
    // Пользователь переключил вид формы; настройку сохраняет главное окно.
    void exactViewModeChanged(bool enabled);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void fillTree();
    void applyExactViewState();
    bool provideExactResource(const QString &path, QByteArray *data, QString *mime) const;
    QString exactStartUrl() const;

    QLabel *m_hint = nullptr;
    QTreeWidget *m_tree = nullptr;
    QSplitter *m_splitter = nullptr;
    QToolButton *m_treeToggle = nullptr;
    QComboBox *m_scaleCombo = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QStackedWidget *m_views = nullptr;
    QWidget *m_exactPage = nullptr;
    QLabel *m_exactMessage = nullptr;
    WebView2Host *m_exactHost = nullptr;
    FormPreviewWidget *m_preview = nullptr;
    QTabWidget *m_tabs = nullptr;
    QPlainTextEdit *m_moduleEdit = nullptr;
    BslHighlighter *m_highlighter = nullptr;
    QByteArray m_exactModel;
    QString m_exactCaption;
    QString m_exactUnavailableReason;
    bool m_exactAvailable = false;
    bool m_exactEnabled = false;
    FormDocument m_document;
    bool m_hasDocument = false;
};

} // namespace v8reader

#endif // QTV8READER_FORMVIEW_H
