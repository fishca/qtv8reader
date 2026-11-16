#ifndef QTV8READER_MAINWINDOW_H
#define QTV8READER_MAINWINDOW_H

#include "core/Configuration.h"
#include "core/ModuleExtractor.h"

#include <QByteArray>
#include <QHash>
#include <QMainWindow>
#include <QStringList>
#include <QVector>

#include <memory>

class QAction;
class QComboBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPlainTextEdit;
class QTabWidget;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace v8reader {

class ConfigurationReader;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool openFile(const QString &path);
    void openInitialFile(const QString &path);

    bool isFileOpen() const;
    int objectCount() const;
    QString fileKindTitle() const;
    // Проверка работоспособности без показа окна: открывает первый объект
    // в дереве и возвращает отчёт о заполнении вкладок.
    QString smokeReport();

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void openFileDialog();
    void refreshCurrent();
    void extractSelected();
    void extractAll();
    void showAbout();
    void onTreeSelectionChanged();
    void onModuleChanged(int index);
    void onContainerDoubleClicked(QTreeWidgetItem *item, int column);
    void onContainerContextMenu(const QPoint &position);
    void openRecentFile();

private:
    enum ItemKind {
        CategoryItem = 1,
        ObjectItem = 2
    };

    enum ItemRole {
        KindRole = Qt::UserRole,
        GuidRole,
        IndexRole
    };

    void setupUi();
    void setupMenus();
    void setupToolBar();
    void setupTree();
    void setupTabs();
    void updateRecentMenu();
    void loadRecentFiles();
    void saveRecentFiles();
    void addRecentFile(const QString &path);

    void buildTree();
    void applyTreeFilter(const QString &text);
    void buildContainerTree();
    void clearObjectView();
    void setProperties(const QVector<QPair<QString, QString>> &rows);
    void showConfiguration();
    void showCategory(const MetadataCategory &category);
    void showObject(MetadataObject &object);
    void fillComposition(const MetadataObject &object);
    void fillModules(const MetadataObject &object);
    void fillMetadata(const MetadataObject &object);
    void previewElement(int index);
    void saveElement(int index);

    MetadataObject *selectedObject();
    MetadataCategory *selectedCategory();
    QString settingsKey(const QString &name) const;

    std::unique_ptr<ConfigurationReader> m_reader;
    MetadataObject *m_currentObject = nullptr;
    QVector<ModuleInfo> m_currentModules;
    QHash<QString, QVector<ModuleInfo>> m_moduleCache;

    QLineEdit *m_treeFilter = nullptr;
    QTreeWidget *m_tree = nullptr;
    QLabel *m_treeHint = nullptr;
    QTabWidget *m_tabs = nullptr;
    QTableWidget *m_properties = nullptr;
    QTreeWidget *m_composition = nullptr;
    QComboBox *m_moduleCombo = nullptr;
    QPlainTextEdit *m_moduleEdit = nullptr;
    QPlainTextEdit *m_metadataEdit = nullptr;
    QTreeWidget *m_containerTree = nullptr;

    QAction *m_refreshAction = nullptr;
    QAction *m_extractAction = nullptr;
    QAction *m_extractAllAction = nullptr;
    QAction *m_closeAction = nullptr;
    QMenu *m_recentMenu = nullptr;

    QStringList m_recentFiles;
    QString m_currentPath;
};

} // namespace v8reader

#endif // QTV8READER_MAINWINDOW_H
