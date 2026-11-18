#ifndef QTV8READER_RECENTFILESCONTROLLER_H
#define QTV8READER_RECENTFILESCONTROLLER_H

#include <QObject>
#include <QStringList>

class QMenu;

namespace v8reader {

// Список недавних файлов: настройки, меню и проверка существования.
class RecentFilesController : public QObject
{
    Q_OBJECT

public:
    explicit RecentFilesController(QMenu *menu, QObject *parent = nullptr);

    void load();
    void save() const;
    void add(const QString &path);
    QStringList files() const { return m_files; }

signals:
    void openRequested(const QString &path);
    void missingFile(const QString &path);

private:
    void rebuildMenu();
    void remove(const QString &path);

    QMenu *m_menu = nullptr;
    QStringList m_files;
};

} // namespace v8reader

#endif // QTV8READER_RECENTFILESCONTROLLER_H
