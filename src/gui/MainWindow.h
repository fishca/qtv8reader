#ifndef QTV8READER_MAINWINDOW_H
#define QTV8READER_MAINWINDOW_H

#include "core/Configuration.h"
#include "core/ModuleExtractor.h"

#include <QByteArray>
#include <QHash>
#include <QIcon>
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
class MxlView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool openFile(const QString &path);
    void openInitialFile(const QString &path);

    bool isFileOpen() const;
    void expandCommonBranch();
    void expandDocumentsBranch();
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
    void onTreeItemExpanded(QTreeWidgetItem *item);
    void onModuleChanged(int index);
    void onContainerDoubleClicked(QTreeWidgetItem *item, int column);
    void onContainerContextMenu(const QPoint &position);
    void openRecentFile();

private:
    // Вид узла дерева метаданных.
    enum ItemKind {
        RootItem = 1,
        GroupItem,
        CategoryItem,
        ObjectItem,
        SectionItem,
        EntryItem,
        ModuleNodeItem
    };

    enum ItemRole {
        KindRole = Qt::UserRole,
        GuidRole,          // GUID объекта для Object/Section/Entry/Module
        SectionKindRole,   // MetadataSectionKind для Section/Entry
        EntryIndexRole,    // индекс элемента раздела
        ChildIndexRole,    // индекс вложенного реквизита ТЧ
        ModuleIndexRole,   // индекс модуля объекта
        LoadedRole,        // дочерние узлы объекта уже построены
        IndexRole          // индекс элемента контейнера (вкладка «Контейнер»)

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
    QTreeWidgetItem *addCategoryItem(QTreeWidgetItem *parent,
                                     const MetadataCategory &category);
    QTreeWidgetItem *addPlaceholderCategoryItem(QTreeWidgetItem *parent,
                                                const QString &guid);
    void buildSubsystemTree(QTreeWidgetItem *parent, MetadataCategory &category);
    QTreeWidgetItem *addSubsystemItem(QTreeWidgetItem *parent,
                                      const MetadataObject &object,
                                      int depth = 0);
    void addSubsystemCompositionItems(QTreeWidgetItem *parent,
                                      const QStringList &guids,
                                      int depth = 0);
    QTreeWidgetItem *addObjectItem(QTreeWidgetItem *parent,
                                   const MetadataObject &object);
    void applyTreeFilter(const QString &text);
    bool filterTreeItem(QTreeWidgetItem *item, const QString &needle);
    void updateTreeHint();
    void buildContainerTree();

    MetadataObject *objectForItem(const QTreeWidgetItem *item);
    QTreeWidgetItem *findObjectItem(const QString &guid) const;
    void populateObjectItem(QTreeWidgetItem *item, MetadataObject &object);
    void ensureObjectItemChildren(const QString &guid);

    void clearObjectView();
    void setProperties(const QVector<QPair<QString, QString>> &rows);
    void showConfiguration();
    void showGroup(const QString &title, const QVector<const MetadataCategory *> &categories);
    void showCategory(const MetadataCategory &category);
    void showPlaceholderCategory(const QString &guid);
    void showObject(MetadataObject &object);
    void showSection(MetadataObject &object, MetadataSectionKind kind);
    void showEntry(MetadataObject &object, MetadataSectionKind kind, int entryIndex,
                   int childIndex);
    void showModuleNode(MetadataObject &object, int moduleIndex);
    void fillComposition(const MetadataObject &object);
    void fillSectionComposition(MetadataObject &object, MetadataSectionKind kind);
    void fillEntryComposition(const MetadataEntry &entry);
    void fillModules(const MetadataObject &object);
    void fillMetadata(const MetadataObject &object);
    void previewElement(int index);
    void saveElement(int index);
    void loadTemplatePreview(const MetadataEntry &entry);

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
    MxlView *m_mxlView = nullptr;
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

    QIcon m_rootIcon;
    QIcon m_groupIcon;
    QIcon m_categoryIcon;
    QIcon m_sectionIcon;
    QIcon m_entryIcon;
    QIcon m_moduleIcon;
};

} // namespace v8reader

#endif // QTV8READER_MAINWINDOW_H
