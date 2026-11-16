#ifndef QTV8READER_PREVIEWDIALOG_H
#define QTV8READER_PREVIEWDIALOG_H

#include <QByteArray>
#include <QDialog>
#include <QString>

class QPlainTextEdit;

namespace v8reader {

// Окно просмотра текстовых и двоичных данных элемента контейнера.
class PreviewDialog : public QDialog
{
    Q_OBJECT

public:
    PreviewDialog(const QString &title, const QByteArray &data, bool isText,
                  QWidget *parent = nullptr);

    static QString hexDump(const QByteArray &data, int maxBytes = 1024 * 1024);

private slots:
    void saveAs();

private:
    QString suggestedFileName() const;

    QString m_title;
    QByteArray m_data;
    bool m_isText = false;
    QPlainTextEdit *m_editor = nullptr;
};

} // namespace v8reader

#endif // QTV8READER_PREVIEWDIALOG_H
