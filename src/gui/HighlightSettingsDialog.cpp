#include "gui/HighlightSettingsDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace v8reader {
namespace {

QString colorButtonStyle(const QColor &color)
{
    return QStringLiteral("background-color: %1; color: %2; border: 1px solid #777; "
                          "padding: 3px 8px;")
        .arg(color.name(),
             color.lightnessF() > 0.6 ? QStringLiteral("#111111")
                                       : QStringLiteral("#ffffff"));
}

} // namespace

HighlightSettingsDialog::HighlightSettingsDialog(const BslHighlightSettings &settings,
                                                 bool dark, QWidget *parent)
    : QDialog(parent),
      m_settings(settings),
      m_dark(dark)
{
    setWindowTitle(QStringLiteral("Настройка подсветки модулей"));
    setModal(true);
    buildUi();
}

void HighlightSettingsDialog::buildUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *general = new QGroupBox(QStringLiteral("Подсветка"), this);
    auto *generalLayout = new QVBoxLayout(general);
    m_enabled = new QCheckBox(QStringLiteral("Включить подсветку текстов модулей"),
                              general);
    m_enabled->setObjectName(QStringLiteral("highlightEnabled"));
    m_enabled->setChecked(m_settings.enabled);
    generalLayout->addWidget(m_enabled);

    auto *styleRow = new QHBoxLayout();
    m_boldKeywords = new QCheckBox(QStringLiteral("Ключевые слова — полужирные"),
                                   general);
    m_boldKeywords->setObjectName(QStringLiteral("highlightBoldKeywords"));
    m_boldKeywords->setChecked(m_settings.boldKeywords);
    m_boldDirectives = new QCheckBox(QStringLiteral("Директивы — полужирные"),
                                     general);
    m_boldDirectives->setObjectName(QStringLiteral("highlightBoldDirectives"));
    m_boldDirectives->setChecked(m_settings.boldDirectives);
    m_italicComments = new QCheckBox(QStringLiteral("Комментарии — курсив"),
                                     general);
    m_italicComments->setObjectName(QStringLiteral("highlightItalicComments"));
    m_italicComments->setChecked(m_settings.italicComments);
    styleRow->addWidget(m_boldKeywords);
    styleRow->addWidget(m_boldDirectives);
    styleRow->addWidget(m_italicComments);
    styleRow->addStretch();
    generalLayout->addLayout(styleRow);
    layout->addWidget(general);

    auto *colors = new QGroupBox(QStringLiteral("Цвета"), this);
    auto *grid = new QGridLayout(colors);
    const struct
    {
        const char *key;
        const char *title;
        QColor BslHighlightSettings::*member;
    } specs[] = {
        {"keyword", "Ключевые слова", &BslHighlightSettings::keyword},
        {"directive", "Директивы препроцессора", &BslHighlightSettings::directive},
        {"annotation", "Аннотации", &BslHighlightSettings::annotation},
        {"number", "Числа", &BslHighlightSettings::number},
        {"string", "Строки", &BslHighlightSettings::string},
        {"comment", "Комментарии", &BslHighlightSettings::comment},
    };

    int row = 0;
    for (const auto &spec : specs) {
        grid->addWidget(new QLabel(QString::fromUtf8(spec.title), colors), row, 0);
        auto *button = new QPushButton(colors);
        button->setObjectName(QStringLiteral("highlightColor_%1")
                                  .arg(QString::fromLatin1(spec.key)));
        button->setMinimumWidth(120);
        connect(button, &QPushButton::clicked, this, [this, row] { chooseColor(row); });
        grid->addWidget(button, row, 1);

        ColorRow colorRow;
        colorRow.key = QString::fromLatin1(spec.key);
        colorRow.color = m_settings.*(spec.member);
        colorRow.button = button;
        m_rows.append(colorRow);
        ++row;
    }
    grid->setColumnStretch(0, 1);
    layout->addWidget(colors);

    auto *buttons = new QDialogButtonBox(this);
    auto *reset = buttons->addButton(QStringLiteral("Сбросить"),
                                     QDialogButtonBox::ResetRole);
    connect(reset, &QPushButton::clicked, this,
            &HighlightSettingsDialog::resetToDefaults);
    buttons->addButton(QDialogButtonBox::Ok);
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    refreshColorButtons();
}

void HighlightSettingsDialog::chooseColor(int index)
{
    if (index < 0 || index >= m_rows.size())
        return;
    const QColor selected = QColorDialog::getColor(m_rows.at(index).color, this,
                                                   QStringLiteral("Цвет подсветки"));
    if (!selected.isValid())
        return;
    m_rows[index].color = selected;
    m_settings.custom = true;
    refreshColorButtons();
}

void HighlightSettingsDialog::refreshColorButtons()
{
    for (ColorRow &row : m_rows) {
        if (!row.button)
            continue;
        row.button->setText(row.color.name().toUpper());
        row.button->setStyleSheet(colorButtonStyle(row.color));
    }
}

void HighlightSettingsDialog::resetToDefaults()
{
    m_settings = BslHighlightSettings::defaults(m_dark);
    m_enabled->setChecked(true);
    m_boldKeywords->setChecked(true);
    m_boldDirectives->setChecked(true);
    m_italicComments->setChecked(true);
    const QColor colors[] = {
        m_settings.keyword, m_settings.directive, m_settings.annotation,
        m_settings.number, m_settings.string, m_settings.comment,
    };
    for (int i = 0; i < m_rows.size() && i < 6; ++i)
        m_rows[i].color = colors[i];
    refreshColorButtons();
}

BslHighlightSettings HighlightSettingsDialog::settings() const
{
    BslHighlightSettings result = m_settings;
    result.enabled = m_enabled->isChecked();
    result.boldKeywords = m_boldKeywords->isChecked();
    result.boldDirectives = m_boldDirectives->isChecked();
    result.italicComments = m_italicComments->isChecked();
    for (const ColorRow &row : m_rows) {
        if (row.key == QStringLiteral("keyword"))
            result.keyword = row.color;
        else if (row.key == QStringLiteral("directive"))
            result.directive = row.color;
        else if (row.key == QStringLiteral("annotation"))
            result.annotation = row.color;
        else if (row.key == QStringLiteral("number"))
            result.number = row.color;
        else if (row.key == QStringLiteral("string"))
            result.string = row.color;
        else if (row.key == QStringLiteral("comment"))
            result.comment = row.color;
    }
    return result;
}

} // namespace v8reader
