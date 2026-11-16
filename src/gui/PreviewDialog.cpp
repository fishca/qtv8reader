#include "gui/PreviewDialog.h"

#include "core/V8Text.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <algorithm>

namespace v8reader {

PreviewDialog::PreviewDialog(const QString &title, const QByteArray &data, bool isText,
                             QWidget *parent)
    : QDialog(parent)
    , m_title(title)
    , m_data(data)
    , m_isText(isText)
{
    setWindowTitle(title);
    resize(900, 700);

    auto *layout = new QVBoxLayout(this);

    QString infoText;
    QString encoding;
    QString text;
    if (isText) {
        text = V8Text::decode(data, &encoding, nullptr);
        infoText = QStringLiteral("Текст, %1 байт").arg(data.size());
        if (!encoding.isEmpty())
            infoText += QStringLiteral(", %1").arg(encoding);
    } else {
        infoText = QStringLiteral("Двоичные данные, %1 байт").arg(data.size());
    }

    auto *info = new QLabel(infoText, this);
    layout->addWidget(info);

    m_editor = new QPlainTextEdit(this);
    m_editor->setReadOnly(true);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    if (isText)
        m_editor->setPlainText(text);
    else
        m_editor->setPlainText(hexDump(data));

    layout->addWidget(m_editor, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QPushButton *saveButton = buttons->addButton(QStringLiteral("Сохранить…"),
                                                  QDialogButtonBox::ActionRole);
    connect(saveButton, &QPushButton::clicked, this, &PreviewDialog::saveAs);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString PreviewDialog::hexDump(const QByteArray &data, int maxBytes)
{
    const int size = static_cast<int>(std::min<qsizetype>(data.size(), maxBytes));
    QString result;
    result.reserve(size * 4);

    for (int offset = 0; offset < size; offset += 16) {
        result += QStringLiteral("%1  ").arg(offset, 8, 16, QLatin1Char('0'));

        QString ascii;
        for (int i = 0; i < 16; ++i) {
            if (offset + i < size) {
                const auto byte = static_cast<unsigned char>(data.at(offset + i));
                result += QStringLiteral("%1 ").arg(byte, 2, 16, QLatin1Char('0'));
                const QChar ch = QChar(byte);
                ascii += ch.isPrint() ? ch : QLatin1Char('.');
            } else {
                result += QStringLiteral("   ");
            }
            if (i == 7)
                result += QLatin1Char(' ');
        }

        result += QStringLiteral(" |%1|\n").arg(ascii);
    }

    if (size < data.size()) {
        result += QStringLiteral("\n… показаны первые %1 из %2 байт\n")
                      .arg(size)
                      .arg(data.size());
    }

    return result;
}

void PreviewDialog::saveAs()
{
    QString filter = m_isText
        ? QStringLiteral("Текстовые файлы (*.txt);;Все файлы (*)")
        : QStringLiteral("Двоичные файлы (*.bin);;Все файлы (*)");
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Сохранить данные"),
                                                suggestedFileName(), filter);
    if (path.isEmpty())
        return;

    if (m_isText && !path.endsWith(QStringLiteral(".txt"), Qt::CaseInsensitive))
        path += QStringLiteral(".txt");
    if (!m_isText && !path.contains(QLatin1Char('.')))
        path += QStringLiteral(".bin");

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_editor->setPlainText(QStringLiteral("Не удалось сохранить файл: %1")
                                   .arg(file.errorString()));
        return;
    }

    const QByteArray bytes = m_isText
        ? m_editor->toPlainText().toUtf8()
        : m_data;
    file.write(bytes);
}

QString PreviewDialog::suggestedFileName() const
{
    QString name = m_title;
    name.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]")),
                 QStringLiteral("_"));
    name = name.trimmed();
    if (name.isEmpty())
        name = QStringLiteral("element");
    name += m_isText ? QStringLiteral(".txt") : QStringLiteral(".bin");
    return name;
}

} // namespace v8reader
