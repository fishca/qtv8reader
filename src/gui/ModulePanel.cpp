#include "gui/ModulePanel.h"

#include "gui/BslHighlighter.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace v8reader {

ModulePanel::ModulePanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 0);

    m_combo = new QComboBox(this);
    m_combo->setObjectName(QStringLiteral("moduleCombo"));
    m_combo->setEnabled(false);
    layout->addWidget(m_combo);

    m_edit = new QPlainTextEdit(this);
    m_edit->setObjectName(QStringLiteral("moduleEdit"));
    m_edit->setReadOnly(true);
    m_edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_edit->setPlaceholderText(QStringLiteral("Выберите объект метаданных"));
    layout->addWidget(m_edit, 1);

    m_highlighter = new BslHighlighter(m_edit->document());

    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                if (index < 0 || index >= m_modules.size()) {
                    m_edit->clear();
                    return;
                }
                m_edit->setPlainText(m_modules.at(index).text);
                emit moduleChanged(index);
            });
}

void ModulePanel::setModules(const QVector<ModuleInfo> &modules)
{
    m_modules = modules;
    m_combo->blockSignals(true);
    m_combo->clear();

    for (const ModuleInfo &module : m_modules) {
        QString title = module.name;
        if (title.isEmpty())
            title = module.elementName;
        if (!module.elementName.isEmpty() && !title.contains(module.elementName))
            title += QStringLiteral(" — %1").arg(module.elementName);
        m_combo->addItem(title);
    }

    const bool hasModules = !m_modules.isEmpty();
    m_combo->setEnabled(hasModules);
    m_combo->blockSignals(false);

    if (hasModules) {
        m_combo->setCurrentIndex(0);
        m_edit->setPlainText(m_modules.at(0).text);
        emit moduleChanged(0);
    } else {
        m_edit->setPlainText(QStringLiteral("Модули объекта не найдены."));
    }
}

void ModulePanel::showSingleModule(const ModuleInfo &module)
{
    m_modules = {module};
    m_combo->blockSignals(true);
    m_combo->clear();
    m_combo->addItem(module.name);
    m_combo->setEnabled(false);
    m_combo->blockSignals(false);
    m_edit->setPlainText(module.text);
}

void ModulePanel::setCurrentIndex(int index)
{
    m_combo->setCurrentIndex(index);
}

int ModulePanel::currentIndex() const
{
    return m_combo->currentIndex();
}

QString ModulePanel::text() const
{
    return m_edit->toPlainText();
}

void ModulePanel::setPlaceholder(const QString &text)
{
    m_modules.clear();
    m_combo->blockSignals(true);
    m_combo->clear();
    m_combo->setEnabled(false);
    m_combo->blockSignals(false);
    m_edit->setPlainText(text);
}

void ModulePanel::setHighlightEnabled(bool enabled)
{
    m_highlighter->setEnabled(enabled);
}

void ModulePanel::setHighlightSettings(const BslHighlightSettings &settings)
{
    m_highlighter->setSettings(settings);
}

void ModulePanel::setDarkMode(bool dark)
{
    m_highlighter->setDarkMode(dark);
}

} // namespace v8reader
