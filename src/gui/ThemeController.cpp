#include "gui/ThemeController.h"

#include "gui/BslHighlighter.h"

#include <QApplication>
#include <QColor>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>

namespace v8reader {
namespace {

QPalette darkPalette()
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x2b, 0x2b, 0x2b));
    palette.setColor(QPalette::WindowText, QColor(0xdd, 0xdd, 0xdd));
    palette.setColor(QPalette::Base, QColor(0x1e, 0x1e, 0x1e));
    palette.setColor(QPalette::AlternateBase, QColor(0x2a, 0x2a, 0x2a));
    palette.setColor(QPalette::ToolTipBase, QColor(0x2b, 0x2b, 0x2b));
    palette.setColor(QPalette::ToolTipText, QColor(0xdd, 0xdd, 0xdd));
    palette.setColor(QPalette::Text, QColor(0xdd, 0xdd, 0xdd));
    palette.setColor(QPalette::Button, QColor(0x3c, 0x3f, 0x41));
    palette.setColor(QPalette::ButtonText, QColor(0xdd, 0xdd, 0xdd));
    palette.setColor(QPalette::BrightText, QColor(0xff, 0x55, 0x55));
    palette.setColor(QPalette::Link, QColor(0x4d, 0xaa, 0xfc));
    palette.setColor(QPalette::Highlight, QColor(0x2f, 0x65, 0xca));
    palette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::PlaceholderText, QColor(0x9a, 0x9a, 0x9a));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x80, 0x80, 0x80));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x80, 0x80, 0x80));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x80, 0x80, 0x80));
    return palette;
}

QString darkStyleSheet()
{
    return QStringLiteral(
        "QMainWindow, QDialog { background-color: #2b2b2b; }"
        "QWidget { color: #dddddd; }"
        "QMenuBar, QMenu, QToolBar, QStatusBar { background-color: #2b2b2b; "
        "color: #dddddd; }"
        "QTreeWidget, QTableWidget, QPlainTextEdit, QTextEdit, QListView, "
        "QListWidget, QComboBox, QLineEdit { background-color: #1e1e1e; "
        "color: #dddddd; selection-background-color: #2f65ca; "
        "selection-color: #ffffff; }"
        "QHeaderView::section { background-color: #3c3f41; color: #dddddd; "
        "border: 1px solid #555555; padding: 2px; }"
        "QTabWidget::pane { border: 1px solid #555555; background: #1e1e1e; }"
        "QTabBar::tab { background: #3c3f41; color: #dddddd; padding: 4px 10px; "
        "border: 1px solid #555555; }"
        "QTabBar::tab:selected { background: #1e1e1e; }"
        "QPushButton { background-color: #3c3f41; color: #dddddd; "
        "border: 1px solid #555555; padding: 3px 8px; }"
        "QPushButton:hover { background-color: #4a4d4f; }"
        "QToolTip { background-color: #2b2b2b; color: #dddddd; "
        "border: 1px solid #555555; }"
        "QScrollBar:vertical { background: #2b2b2b; width: 12px; }"
        "QScrollBar::handle:vertical { background: #555555; min-height: 20px; "
        "border-radius: 4px; }"
        "QScrollBar:horizontal { background: #2b2b2b; height: 12px; }"
        "QScrollBar::handle:horizontal { background: #555555; min-width: 20px; "
        "border-radius: 4px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }");
}

QString settingsKey(const QString &name)
{
    return QStringLiteral("MainWindow/") + name;
}

} // namespace

ThemeController::ThemeController(QObject *parent)
    : QObject(parent),
      m_defaultPalette(QApplication::palette()),
      m_defaultStyleName(QApplication::style()->objectName())
{
}

void ThemeController::setHighlighter(BslHighlighter *highlighter)
{
    m_highlighter = highlighter;
    if (m_highlighter)
        m_highlighter->setDarkMode(isDark());
}

bool ThemeController::isDark() const
{
    return m_theme.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0;
}

void ThemeController::apply(const QString &theme, bool persist)
{
    const bool dark = theme.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0;
    m_theme = dark ? QStringLiteral("dark") : QStringLiteral("light");

    if (dark) {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QApplication::setPalette(darkPalette());
        qApp->setStyleSheet(darkStyleSheet());
    } else {
        if (!m_defaultStyleName.isEmpty()) {
            if (QStyle *style = QStyleFactory::create(m_defaultStyleName))
                QApplication::setStyle(style);
        }
        QApplication::setPalette(m_defaultPalette);
        qApp->setStyleSheet(QString());
    }

    if (m_highlighter)
        m_highlighter->setDarkMode(dark);

    if (persist) {
        QSettings settings;
        settings.setValue(settingsKey(QStringLiteral("theme")), m_theme);
    }

    emit darkModeChanged(dark);
}

} // namespace v8reader
