#include "gui/RecentFilesController.h"

#include <QAction>
#include <QFileInfo>
#include <QMenu>
#include <QSettings>

namespace v8reader {
namespace {

constexpr int kMaxRecentFiles = 10;

QString settingsKey(const QString &name)
{
    return QStringLiteral("MainWindow/") + name;
}

} // namespace

RecentFilesController::RecentFilesController(QMenu *menu, QObject *parent)
    : QObject(parent),
      m_menu(menu)
{
}

void RecentFilesController::load()
{
    QSettings settings;
    const QStringList stored = settings.value(settingsKey(QStringLiteral("recentFiles")))
                                   .toStringList();

    m_files.clear();
    for (const QString &path : stored) {
        if (!path.trimmed().isEmpty()
            && !m_files.contains(path, Qt::CaseInsensitive)) {
            m_files.append(path);
        }
        if (m_files.size() >= kMaxRecentFiles)
            break;
    }
    rebuildMenu();
}

void RecentFilesController::save() const
{
    QSettings settings;
    settings.setValue(settingsKey(QStringLiteral("recentFiles")), m_files);
}

void RecentFilesController::add(const QString &path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();

    QStringList filtered;
    filtered.append(absolute);
    for (const QString &existing : m_files) {
        if (existing.compare(absolute, Qt::CaseInsensitive) == 0)
            continue;
        filtered.append(existing);
        if (filtered.size() >= kMaxRecentFiles)
            break;
    }
    m_files = filtered;

    save();
    rebuildMenu();
}

void RecentFilesController::remove(const QString &path)
{
    m_files.removeAll(path);
    save();
    rebuildMenu();
}

void RecentFilesController::rebuildMenu()
{
    if (!m_menu)
        return;

    m_menu->clear();
    if (m_files.isEmpty()) {
        QAction *emptyAction = m_menu->addAction(QStringLiteral("Нет недавних файлов"));
        emptyAction->setEnabled(false);
        return;
    }

    for (const QString &path : m_files) {
        const QFileInfo info(path);
        auto *action = m_menu->addAction(info.fileName());
        action->setToolTip(path);
        action->setData(path);
        connect(action, &QAction::triggered, this, [this, path] {
            if (!QFileInfo::exists(path)) {
                emit missingFile(path);
                remove(path);
                return;
            }
            emit openRequested(path);
        });
    }

    m_menu->addSeparator();
    QAction *clearAction = m_menu->addAction(QStringLiteral("Очистить список"));
    connect(clearAction, &QAction::triggered, this, [this] {
        m_files.clear();
        save();
        rebuildMenu();
    });
}

} // namespace v8reader
