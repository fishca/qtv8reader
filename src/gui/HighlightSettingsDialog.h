#ifndef QTV8READER_HIGHLIGHTSETTINGSDIALOG_H
#define QTV8READER_HIGHLIGHTSETTINGSDIALOG_H

#include "gui/BslHighlightSettings.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QPushButton;

namespace v8reader {

// Настройки подсветки модулей: включение, цвета и начертания.
class HighlightSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    HighlightSettingsDialog(const BslHighlightSettings &settings, bool dark,
                            QWidget *parent = nullptr);

    BslHighlightSettings settings() const;

private:
    struct ColorRow
    {
        QString key;
        QColor color;
        QPushButton *button = nullptr;
    };

    void buildUi();
    void chooseColor(int index);
    void refreshColorButtons();
    void resetToDefaults();

    BslHighlightSettings m_settings;
    bool m_dark = false;
    QCheckBox *m_enabled = nullptr;
    QCheckBox *m_boldKeywords = nullptr;
    QCheckBox *m_boldDirectives = nullptr;
    QCheckBox *m_italicComments = nullptr;
    QVector<ColorRow> m_rows;
};

} // namespace v8reader

#endif // QTV8READER_HIGHLIGHTSETTINGSDIALOG_H
