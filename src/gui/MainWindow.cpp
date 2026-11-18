#include "gui/MainWindow.h"

#include "gui/BslHighlighter.h"
#include "core/BinaryText.h"
#include "core/ConfigurationReader.h"
#include "core/DisplayText.h"
#include "gui/MetadataIcons.h"
#include "gui/ModulePanel.h"
#include "gui/PropertiesPanel.h"
#include "gui/RecentFilesController.h"
#include "gui/ExtractionController.h"
#include "gui/ThemeController.h"
#include "gui/MainWindowSupport.h"
#include "core/Extractor.h"
#include "core/V8Container.h"
#include "core/V8Text.h"
#include "core/FormReader.h"
#include "gui/FormView.h"
#include "gui/MxlPrinter.h"
#include "gui/MxlView.h"

#include <QCheckBox>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include "gui/PreviewDialog.h"

#include "core/MxlReader.h"
#include "core/V8Container.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBrush>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QShortcut>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QShowEvent>
#include <QPlainTextEdit>
#include <QPoint>
#include <QSet>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <functional>

namespace v8reader {


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_themeController = new ThemeController(this);
    m_rootIcon = QIcon();
    for (int size : {16, 24, 32, 48}) {
        const QString path = QStringLiteral(":/icons/app/qtv8reader_%1.png").arg(size);
        if (QFile::exists(path))
            m_rootIcon.addFile(path, QSize(size, size));
    }
    if (m_rootIcon.isNull())
        m_rootIcon = MetadataIcons::icon(QStringLiteral("root"));
    setWindowIcon(m_rootIcon);
    m_groupIcon = MetadataIcons::icon(QStringLiteral("group"));
    m_categoryIcon = MetadataIcons::icon(QStringLiteral("common"));
    m_sectionIcon = MetadataIcons::icon(QStringLiteral("tabularSection"));
    m_entryIcon = MetadataIcons::icon(QStringLiteral("column"));
    m_moduleIcon = MetadataIcons::icon(QStringLiteral("editObject"));

    QSettings settings;
    m_highlightEnabled =
        settings.value(settingsKey(QStringLiteral("moduleHighlight")), true).toBool();
    m_theme = settings.value(settingsKey(QStringLiteral("theme")),
                             QStringLiteral("light")).toString();

    setupUi();
    setupMenus();
    setupToolBar();
    setupTree();
    setupTabs();
    m_modulePanel->setDarkMode(m_themeController->isDark());
    if (m_formView)
        m_formView->setDarkMode(m_themeController->isDark());
    connect(m_themeController, &ThemeController::darkModeChanged, this, [this](bool dark) {
        if (m_lightThemeAction)
            m_lightThemeAction->setChecked(!dark);
        if (m_darkThemeAction)
            m_darkThemeAction->setChecked(dark);
        if (m_modulePanel)
            m_modulePanel->setDarkMode(dark);
        if (m_formView)
            m_formView->setDarkMode(dark);
    });
    m_recentFilesController->load();
    applyTheme(m_theme, false);

    const QByteArray geometry = settings.value(settingsKey(QStringLiteral("geometry"))).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    const QByteArray state = settings.value(settingsKey(QStringLiteral("state"))).toByteArray();
    if (!state.isEmpty())
        restoreState(state);

    setAcceptDrops(true);
    statusBar()->showMessage(QStringLiteral("Откройте файл конфигурации 1С"));
    showConfiguration();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("V8 Reader — просмотр конфигураций 1С"));
    resize(1280, 820);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    auto *leftPanel = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);

    auto *treeTitle = new QLabel(QStringLiteral("Объекты метаданных"), leftPanel);
    QFont titleFont = treeTitle->font();
    titleFont.setBold(true);
    treeTitle->setFont(titleFont);
    leftLayout->addWidget(treeTitle);

    m_treeFilter = new QLineEdit(leftPanel);
    m_treeFilter->setObjectName(QStringLiteral("treeFilter"));
    m_treeFilter->setPlaceholderText(QStringLiteral("Поиск по объектам и метаданным (Ctrl+F)…"));
    m_treeFilter->setClearButtonEnabled(true);
    leftLayout->addWidget(m_treeFilter);

    m_tree = new QTreeWidget(leftPanel);
    m_tree->setObjectName(QStringLiteral("objectTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(m_tree, 1);

    m_treeHint = new QLabel(QStringLiteral("Файл не открыт"), leftPanel);
    m_treeHint->setWordWrap(true);
    m_treeHint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    leftLayout->addWidget(m_treeHint);

    splitter->addWidget(leftPanel);

    m_tabs = new QTabWidget(splitter);
    m_tabs->setObjectName(QStringLiteral("objectTabs"));
    splitter->addWidget(m_tabs);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({360, 920});

    setCentralWidget(splitter);

    connect(m_treeFilter, &QLineEdit::textChanged, this,
            [this](const QString &text) { applyTreeFilter(text); });

    auto *findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, this, [this] {
        m_treeFilter->setFocus();
        m_treeFilter->selectAll();
    });
}

void MainWindow::setupMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&Файл"));

    QAction *openAction = fileMenu->addAction(
        style()->standardIcon(QStyle::SP_DialogOpenButton),
        QStringLiteral("&Открыть…"));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFileDialog);

    QMenu *recentMenu = fileMenu->addMenu(QStringLiteral("Недавние файлы"));
    m_recentFilesController = new RecentFilesController(recentMenu, this);
    connect(m_recentFilesController, &RecentFilesController::openRequested,
            this, [this](const QString &path) { openFile(path); });
    connect(m_recentFilesController, &RecentFilesController::missingFile,
            this, [this](const QString &path) {
                QMessageBox::warning(
                    this, QStringLiteral("Файл не найден"),
                    QStringLiteral("Файл «%1» больше не существует.").arg(path));
            });
    fileMenu->addSeparator();

    m_closeAction = fileMenu->addAction(QStringLiteral("Закрыть файл"));
    m_closeAction->setEnabled(false);
    connect(m_closeAction, &QAction::triggered, this, [this] {
        m_reader.reset();
        m_currentPath.clear();
        m_currentObject = nullptr;
        m_currentModules.clear();
        m_moduleCache.clear();
        m_tree->clear();
        m_containerTree->clear();
        m_treeHint->setText(QStringLiteral("Файл не открыт"));
        setWindowTitle(QStringLiteral("V8 Reader — просмотр конфигураций 1С"));
        m_closeAction->setEnabled(false);
        m_refreshAction->setEnabled(false);
        m_extractAction->setEnabled(false);
        m_extractAllAction->setEnabled(false);
        m_mxlDocument = mxl::Document();
        m_mxlDocumentValid = false;
        updateMxlActions();
        showConfiguration();
        statusBar()->showMessage(QStringLiteral("Файл закрыт"));
    });

    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(QStringLiteral("В&ыход"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *objectMenu = menuBar()->addMenu(QStringLiteral("&Объект"));

    m_refreshAction = objectMenu->addAction(
        style()->standardIcon(QStyle::SP_BrowserReload),
        QStringLiteral("Обновить"));
    m_refreshAction->setShortcut(QKeySequence::Refresh);
    m_refreshAction->setEnabled(false);
    connect(m_refreshAction, &QAction::triggered, this, &MainWindow::refreshCurrent);

    m_extractAction = objectMenu->addAction(
        style()->standardIcon(QStyle::SP_DialogSaveButton),
        QStringLiteral("Извлечь объект…"));
    m_extractAction->setEnabled(false);
    connect(m_extractAction, &QAction::triggered, this, &MainWindow::extractSelected);

    m_extractAllAction = objectMenu->addAction(QStringLiteral("Извлечь все объекты…"));
    m_extractAllAction->setEnabled(false);
    connect(m_extractAllAction, &QAction::triggered, this, &MainWindow::extractAll);

    objectMenu->addSeparator();
    m_printMxlAction = objectMenu->addAction(QStringLiteral("Печать макета…"));
    m_printMxlAction->setShortcut(QKeySequence::Print);
    m_printMxlAction->setEnabled(false);
    connect(m_printMxlAction, &QAction::triggered, this, &MainWindow::printMxl);

    m_pdfMxlAction = objectMenu->addAction(QStringLiteral("Экспорт макета в PDF…"));
    m_pdfMxlAction->setEnabled(false);
    connect(m_pdfMxlAction, &QAction::triggered, this, &MainWindow::exportMxlPdf);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("&Вид"));
    m_highlightAction = viewMenu->addAction(QStringLiteral("Подсветка модулей"));
    m_highlightAction->setCheckable(true);
    m_highlightAction->setChecked(m_highlightEnabled);
    m_highlightAction->setToolTip(
        QStringLiteral("Синтаксическая подсветка текстов модулей 1С"));
    connect(m_highlightAction, &QAction::toggled, this,
            &MainWindow::setModuleHighlighting);

    QMenu *themeMenu = viewMenu->addMenu(QStringLiteral("Тема"));
    auto *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    m_lightThemeAction = themeMenu->addAction(QStringLiteral("Светлая"));
    m_darkThemeAction = themeMenu->addAction(QStringLiteral("Тёмная"));
    for (QAction *action : {m_lightThemeAction, m_darkThemeAction}) {
        action->setCheckable(true);
        themeGroup->addAction(action);
    }
    connect(m_lightThemeAction, &QAction::triggered, this,
            [this] { applyTheme(QStringLiteral("light")); });
    connect(m_darkThemeAction, &QAction::triggered, this,
            [this] { applyTheme(QStringLiteral("dark")); });

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("&Справка"));
    QAction *aboutAction = helpMenu->addAction(QStringLiteral("О программе…"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::setupToolBar()
{
    auto *toolBar = addToolBar(QStringLiteral("Основная панель"));
    toolBar->setMovable(false);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    auto *openAction = toolBar->addAction(
        style()->standardIcon(QStyle::SP_DialogOpenButton),
        QStringLiteral("Открыть"));
    connect(openAction, &QAction::triggered, this, &MainWindow::openFileDialog);

    toolBar->addSeparator();
    toolBar->addAction(m_refreshAction);
    toolBar->addAction(m_extractAction);
}

void MainWindow::setupTree()
{
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &MainWindow::onTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::itemExpanded,
            this, &MainWindow::onTreeItemExpanded);
}

void MainWindow::setupTabs()
{
    m_properties = new PropertiesPanel();
    m_tabs->addTab(m_properties, QStringLiteral("Свойства"));

    m_composition = new QTreeWidget();
    m_composition->setObjectName(QStringLiteral("compositionTree"));
    m_composition->setColumnCount(5);
    m_composition->setHeaderLabels({
        QStringLiteral("Вид"),
        QStringLiteral("Имя"),
        QStringLiteral("Синоним"),
        QStringLiteral("GUID"),
        QStringLiteral("Комментарий")
    });
    m_composition->setAlternatingRowColors(true);
    m_composition->setUniformRowHeights(true);
    m_composition->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_composition->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_composition->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_composition->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_composition->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_composition->setColumnWidth(1, 220);
    m_composition->setColumnWidth(2, 180);
    m_tabs->addTab(m_composition, QStringLiteral("Состав"));

    m_modulePanel = new ModulePanel();
    m_modulePanel->setHighlightEnabled(m_highlightEnabled);
    m_tabs->addTab(m_modulePanel, QStringLiteral("Модуль"));

    m_metadataEdit = new QPlainTextEdit();
    m_metadataEdit->setReadOnly(true);
    m_metadataEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_metadataEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_metadataEdit->setPlaceholderText(QStringLiteral("Метаданные объекта"));
    m_tabs->addTab(m_metadataEdit, QStringLiteral("Метаданные"));

    m_containerTree = new QTreeWidget();
    m_containerTree->setColumnCount(5);
    m_containerTree->setHeaderLabels({
        QStringLiteral("Имя"),
        QStringLiteral("Создан"),
        QStringLiteral("Изменён"),
        QStringLiteral("Размер данных"),
        QStringLiteral("Расположение")
    });
    m_containerTree->setAlternatingRowColors(true);
    m_containerTree->setUniformRowHeights(true);
    m_containerTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_containerTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_containerTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_containerTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_containerTree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_containerTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_containerTree, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onContainerDoubleClicked);
    connect(m_containerTree, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::onContainerContextMenu);
    m_tabs->addTab(m_containerTree, QStringLiteral("Контейнер"));
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (m_tabs->widget(index) == m_containerTree)
            ensureContainerTree();
    });

    m_formPage = new QWidget();
    auto *formLayout = new QVBoxLayout(m_formPage);
    formLayout->setContentsMargins(0, 0, 0, 0);
    m_formView = new FormView(m_formPage);
    m_formView->setObjectName(QStringLiteral("formView"));
    m_formView->setModuleHighlightEnabled(m_highlightEnabled);
    formLayout->addWidget(m_formView, 1);
    m_tabs->addTab(m_formPage, QStringLiteral("Форма"));

    m_mxlPage = new QWidget();
    auto *mxlLayout = new QVBoxLayout(m_mxlPage);
    mxlLayout->setContentsMargins(0, 4, 0, 0);
    auto *mxlHeader = new QHBoxLayout();
    m_showAreasCheck = new QCheckBox(QStringLiteral("Именованные области"), m_mxlPage);
    auto *areasCheck = m_showAreasCheck;
    areasCheck->setObjectName(QStringLiteral("showNamedAreas"));
    areasCheck->setToolTip(QStringLiteral("Показывать границы именованных областей макета"));
    mxlHeader->addWidget(areasCheck);

    auto *gridCheck = new QCheckBox(QStringLiteral("Сетка"), m_mxlPage);
    gridCheck->setChecked(true);
    gridCheck->setToolTip(QStringLiteral("Показывать сетку ячеек (как в конфигураторе)"));
    mxlHeader->addWidget(gridCheck);
    mxlHeader->addStretch();
    mxlLayout->addLayout(mxlHeader);

    m_mxlView = new MxlView(m_mxlPage);
    m_mxlView->setObjectName(QStringLiteral("mxlView"));
    mxlLayout->addWidget(m_mxlView, 1);
    connect(areasCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_mxlView)
            m_mxlView->setShowNamedAreas(checked);
    });
    connect(gridCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_mxlView)
            m_mxlView->setShowGrid(checked);
    });
    m_tabs->addTab(m_mxlPage, QStringLiteral("Табличный документ"));
}

bool MainWindow::openFile(const QString &path)
{
    if (path.trimmed().isEmpty())
        return false;

    auto reader = std::make_unique<ConfigurationReader>();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const bool ok = reader->open(path, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::critical(this, QStringLiteral("Ошибка открытия"),
                              QStringLiteral("Не удалось открыть файл «%1»:\n%2")
                                  .arg(path, error));
        return false;
    }

    m_reader = std::move(reader);
    m_currentPath = QFileInfo(path).absoluteFilePath();
    m_currentObject = nullptr;
    m_currentModules.clear();
    m_moduleCache.clear();
    m_rootModules = m_reader->configurationModules();

    m_recentFilesController->add(m_currentPath);
    buildTree();
    // «Контейнер» строится лениво: на больших конфигурациях это тысячи
    // QTreeWidgetItem и заметная задержка при открытии файла.
    m_containerTreeDirty = true;
    if (m_containerTree)
        m_containerTree->clear();
    if (m_tabs && m_tabs->currentWidget() == m_containerTree)
        ensureContainerTree();
    showConfiguration();

    const Configuration &configuration = m_reader->configuration();
    updateTreeHint();
    setWindowTitle(QStringLiteral("V8 Reader — %1").arg(QFileInfo(path).fileName()));

    m_closeAction->setEnabled(true);
    m_refreshAction->setEnabled(true);
    m_extractAllAction->setEnabled(configuration.objectCount() > 0);

    statusBar()->showMessage(QStringLiteral("%1, элементов контейнера: %2, объектов: %3")
                                 .arg(m_reader->container().format().label())
                                 .arg(m_reader->container().count())
                                 .arg(configuration.objectCount()));
    return true;
}
void MainWindow::openInitialFile(const QString &path)
{
    openFile(path);
}

bool MainWindow::isFileOpen() const
{
    return m_reader != nullptr && m_reader->isOpen();
}

int MainWindow::objectCount() const
{
    return m_reader ? m_reader->configuration().objectCount() : 0;
}

QString MainWindow::fileKindTitle() const
{
    return m_reader ? m_reader->configuration().kindTitle : QString();
}

QString MainWindow::smokeReport()
{
    QStringList lines;
    if (!m_reader) {
        lines << QStringLiteral("файл не открыт");
        return lines.join(QLatin1Char('\n'));
    }

    const Configuration &configuration = m_reader->configuration();
    lines << QStringLiteral("вид: %1").arg(configuration.kindTitle);
    lines << QStringLiteral("объектов: %1").arg(configuration.objectCount());

    std::function<QTreeWidgetItem *(QTreeWidgetItem *)> findObject =
        [&](QTreeWidgetItem *item) -> QTreeWidgetItem * {
        if (item->data(0, KindRole).toInt() == ObjectItem)
            return item;
        for (int i = 0; i < item->childCount(); ++i) {
            if (QTreeWidgetItem *found = findObject(item->child(i)))
                return found;
        }
        return nullptr;
    };

    QTreeWidgetItem *firstObject = nullptr;
    for (int i = 0; i < m_tree->topLevelItemCount() && !firstObject; ++i)
        firstObject = findObject(m_tree->topLevelItem(i));

    if (firstObject) {
        m_tree->setCurrentItem(firstObject);
        m_tree->scrollToItem(firstObject);
    }
    QCoreApplication::processEvents();
    lines << QStringLiteral("дерево: %1 узлов верхнего уровня")
                 .arg(m_tree->topLevelItemCount());

    lines << QStringLiteral("состав: %1 разделов").arg(m_composition->topLevelItemCount());
    lines << QStringLiteral("свойства: %1 строк").arg(m_properties->rowCount());
    lines << QStringLiteral("модуль: %1 символов")
                 .arg(m_modulePanel->text().size());
    lines << QStringLiteral("метаданные: %1 символов")
                 .arg(m_metadataEdit->toPlainText().size());

    // Проверяем узлы разделов и элементов на объекте, у которого есть состав.
    const QString preferredTypes[] = {QStringLiteral("Документ"),
                                      QStringLiteral("Справочник"),
                                      QStringLiteral("Регистр сведений")};
    QTreeWidgetItem *richObject = nullptr;
    for (const QString &typeTitle : preferredTypes) {
        for (const MetadataCategory &category : configuration.categories) {
            if (category.typeTitle != typeTitle || category.objects.isEmpty())
                continue;
            const QString guid = category.objects.at(0).guid;
            ensureObjectItemChildren(guid);
            if (QTreeWidgetItem *candidate = findObjectItem(guid)) {
                richObject = candidate;
                break;
            }
        }
        if (richObject)
            break;
    }
    if (!richObject)
        richObject = firstObject;

    int sectionCount = 0;
    int entryCount = 0;
    int moduleCount = 0;
    if (richObject) {
        for (int i = 0; i < richObject->childCount(); ++i) {
            const int kind = richObject->child(i)->data(0, KindRole).toInt();
            if (kind == SectionItem)
                ++sectionCount;
            else if (kind == ModuleNodeItem)
                ++moduleCount;
        }
        QTreeWidgetItem *preferredSection = nullptr;
        for (int i = 0; i < richObject->childCount(); ++i) {
            QTreeWidgetItem *candidate = richObject->child(i);
            if (candidate->data(0, KindRole).toInt() != SectionItem)
                continue;
            if (!preferredSection)
                preferredSection = candidate;
            if (candidate->data(0, SectionKindRole).toInt()
                == static_cast<int>(MetadataSectionKind::TabularSections)) {
                preferredSection = candidate;
                break;
            }
        }
        for (int i = 0; i < richObject->childCount(); ++i) {
            QTreeWidgetItem *child = richObject->child(i);
            if (child != preferredSection)
                continue;
            m_tree->setCurrentItem(child);
            QCoreApplication::processEvents();
            entryCount = child->childCount();
            if (entryCount > 0) {
                QTreeWidgetItem *firstEntry = child->child(0);
                m_tree->setCurrentItem(firstEntry);
                QCoreApplication::processEvents();
                if (firstEntry->childCount() > 0) {
                    m_tree->setCurrentItem(firstEntry->child(0));
                    QCoreApplication::processEvents();
                    lines << QStringLiteral("реквизитов ТЧ: %1")
                                 .arg(firstEntry->childCount());
                }
            }
            break;
        }
        m_tree->setCurrentItem(richObject);
        QCoreApplication::processEvents();
    }

    // Проверяем стандартную пустую ветку (если есть).
    QString emptyCategoryName;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *rootItem = m_tree->topLevelItem(i);
        for (int j = 0; j < rootItem->childCount(); ++j) {
            QTreeWidgetItem *categoryItem = rootItem->child(j);
            if (categoryItem->data(0, KindRole).toInt() == CategoryItem
                && categoryItem->childCount() == 0) {
                m_tree->setCurrentItem(categoryItem);
                QCoreApplication::processEvents();
                emptyCategoryName = categoryItem->text(0);
                break;
            }
        }
        if (!emptyCategoryName.isEmpty())
            break;
    }
    if (!emptyCategoryName.isEmpty()) {
        lines << QStringLiteral("пустая ветка: %1").arg(emptyCategoryName);
        if (firstObject) {
            m_tree->setCurrentItem(firstObject);
            QCoreApplication::processEvents();
        }
    }

    lines << QStringLiteral("узлы объекта: разделов %1, модулей %2")
                 .arg(sectionCount)
                 .arg(moduleCount);
    lines << QStringLiteral("элементов выбранного раздела: %1").arg(entryCount);
    lines << QStringLiteral("состав после выбора: %1")
                 .arg(m_composition->topLevelItemCount());

    // Проверяем иерархию подсистем.
    std::function<QTreeWidgetItem *(QTreeWidgetItem *)> findSubsystemsCategory =
        [&](QTreeWidgetItem *item) -> QTreeWidgetItem * {
        if (item->data(0, KindRole).toInt() == CategoryItem
            && item->text(0) == QStringLiteral("Подсистемы")) {
            return item;
        }
        for (int i = 0; i < item->childCount(); ++i) {
            if (QTreeWidgetItem *found = findSubsystemsCategory(item->child(i)))
                return found;
        }
        return nullptr;
    };

    QTreeWidgetItem *subsystemsItem = nullptr;
    for (int i = 0; i < m_tree->topLevelItemCount() && !subsystemsItem; ++i)
        subsystemsItem = findSubsystemsCategory(m_tree->topLevelItem(i));

    if (subsystemsItem) {
        subsystemsItem->setExpanded(true);
        QCoreApplication::processEvents();
        const int rootCount = subsystemsItem->childCount();
        int nestedCount = 0;
        QTreeWidgetItem *firstWithChildren = nullptr;
        for (int k = 0; k < rootCount; ++k) {
            QTreeWidgetItem *subsystemItem = subsystemsItem->child(k);
            if (subsystemItem->childCount() > 0) {
                firstWithChildren = subsystemItem;
                nestedCount = subsystemItem->childCount();
                break;
            }
        }
        lines << QStringLiteral("подсистемы: верхних %1, вложенных у первой %2")
                     .arg(rootCount)
                     .arg(nestedCount);
        if (firstWithChildren) {
            firstWithChildren->setExpanded(true);
            m_tree->setCurrentItem(firstWithChildren);
            m_tree->scrollToItem(firstWithChildren);
            QCoreApplication::processEvents();
        }
    }

    // Порядок веток группы «Общие» (как в 1С).
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *rootItem = m_tree->topLevelItem(i);
        for (int j = 0; j < rootItem->childCount(); ++j) {
            QTreeWidgetItem *groupItem = rootItem->child(j);
            if (groupItem->data(0, KindRole).toInt() != GroupItem)
                continue;

            QStringList commonTitles;
            for (int k = 0; k < groupItem->childCount() && k < 8; ++k)
                commonTitles << groupItem->child(k)->text(0);
            lines << QStringLiteral("общие: %1")
                         .arg(commonTitles.join(QStringLiteral(", ")));
            break;
        }

        for (int j = 0; j < rootItem->childCount(); ++j) {
            QTreeWidgetItem *categoryItem = rootItem->child(j);
            if (categoryItem->data(0, KindRole).toInt() != CategoryItem)
                continue;
            if (categoryItem->text(0) != QStringLiteral("Документы"))
                continue;

            QStringList subBranches;
            QStringList firstChildren;
            for (int k = 0; k < categoryItem->childCount(); ++k) {
                QTreeWidgetItem *child = categoryItem->child(k);
                if (child->data(0, KindRole).toInt() == CategoryItem)
                    subBranches << child->text(0);
                if (k < 3)
                    firstChildren << child->text(0);
            }
            lines << QStringLiteral("подветки документов: %1")
                         .arg(subBranches.join(QStringLiteral(", ")));
            lines << QStringLiteral("документы: первые узлы — %1")
                         .arg(firstChildren.join(QStringLiteral(", ")));
            break;
        }
    }
    // Предпросмотр табличного документа: первый макет выбранного объекта.
    if (richObject) {
        QTreeWidgetItem *templatesSection = nullptr;
        for (int i = 0; i < richObject->childCount(); ++i) {
            QTreeWidgetItem *child = richObject->child(i);
            if (child->data(0, KindRole).toInt() != SectionItem)
                continue;
            if (child->data(0, SectionKindRole).toInt()
                == static_cast<int>(MetadataSectionKind::Templates)) {
                templatesSection = child;
                break;
            }
        }
        if (templatesSection && templatesSection->childCount() > 0) {
            templatesSection->setExpanded(true);
            QCoreApplication::processEvents();
            QTreeWidgetItem *templateEntry = templatesSection->child(0);
            m_tree->setCurrentItem(templateEntry);
            m_tree->scrollToItem(templateEntry);
            QCoreApplication::processEvents();
            lines << QStringLiteral(
                         "макет: %1, строк %2, колонок %3, областей %4, печать: %5")
                         .arg(m_mxlView && m_mxlView->hasDocument()
                                  ? QStringLiteral("MXL разобран")
                                  : QStringLiteral("не MXL"))
                         .arg(m_mxlView ? m_mxlView->rowCount() : 0)
                         .arg(m_mxlView ? m_mxlView->columnCount() : 0)
                         .arg(m_mxlDocument.namedAreas.size())
                         .arg(m_mxlDocumentValid ? mxlPrintSummary()
                                                 : QStringLiteral("—"));
        }
    }
    return lines.join(QLatin1Char('\n'));
}

void MainWindow::expandCommonBranch()
{
    if (m_tree->topLevelItemCount() == 0)
        return;

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *rootItem = m_tree->topLevelItem(i);
        for (int j = 0; j < rootItem->childCount(); ++j) {
            QTreeWidgetItem *groupItem = rootItem->child(j);
            if (groupItem->data(0, KindRole).toInt() != GroupItem)
                continue;
            groupItem->setExpanded(true);
            m_tree->setCurrentItem(rootItem);
            m_tree->scrollToItem(rootItem);
            QCoreApplication::processEvents();
            return;
        }
    }
}
void MainWindow::expandDocumentsBranch()
{
    if (m_tree->topLevelItemCount() == 0)
        return;

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *rootItem = m_tree->topLevelItem(i);
        for (int j = 0; j < rootItem->childCount(); ++j) {
            QTreeWidgetItem *categoryItem = rootItem->child(j);
            if (categoryItem->data(0, KindRole).toInt() != CategoryItem)
                continue;
            if (categoryItem->text(0) != QStringLiteral("Документы"))
                continue;
            categoryItem->setExpanded(true);
            m_tree->setCurrentItem(categoryItem);
            m_tree->scrollToItem(categoryItem);
            QCoreApplication::processEvents();
            return;
        }
    }
}
void MainWindow::openFileDialog()
{
    const QString filter = QStringLiteral(
        "Файлы 1С (*.cf *.cfe *.epf *.erf);;"
        "Конфигурации (*.cf *.cfe);;"
        "Внешние обработки (*.epf *.erf);;"
        "Все файлы (*)");
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Открыть файл 1С"), m_currentPath, filter);
    if (!path.isEmpty())
        openFile(path);
}

void MainWindow::refreshCurrent()
{
    if (m_currentPath.isEmpty())
        return;

    const QString selectedGuid = m_currentObject ? m_currentObject->guid : QString();
    if (!openFile(m_currentPath) || selectedGuid.isEmpty())
        return;

    if (QTreeWidgetItem *objectItem = findObjectItem(selectedGuid)) {
        ensureObjectItemChildren(selectedGuid);
        m_tree->setCurrentItem(objectItem);
        m_tree->scrollToItem(objectItem);
    }
}

void MainWindow::extractSelected()
{
    if (!m_reader || !m_currentObject)
        return;

    const QString startDirectory = m_currentPath.isEmpty()
        ? QString()
        : QFileInfo(m_currentPath).absolutePath();
    const QString directory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Каталог для сохранения объекта"), startDirectory);
    if (directory.isEmpty())
        return;

    ExtractionReport report;
    QString error;
    ExtractionController controller;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = controller.extractObject(*m_reader, *m_currentObject, directory,
                                             &report, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::critical(this, QStringLiteral("Ошибка извлечения"), error);
        return;
    }

    QMessageBox::information(
        this, QStringLiteral("Извлечение завершено"),
        ExtractionController::objectResultMessage(*m_currentObject, report));
    statusBar()->showMessage(QStringLiteral("Объект сохранён в %1")
                                 .arg(report.outputDirectory));
}

void MainWindow::extractAll()
{
    if (!m_reader)
        return;

    const int objectCount = m_reader->configuration().objectCount();
    if (objectCount == 0) {
        QMessageBox::information(this, QStringLiteral("Извлечение"),
                                 QStringLiteral("В конфигурации нет объектов метаданных."));
        return;
    }

    const auto answer = QMessageBox::question(
        this, QStringLiteral("Извлечение всех объектов"),
        QStringLiteral("Сохранить все объекты метаданных (%1)?\n"
                       "В каталоге будут созданы подкаталоги по категориям.")
            .arg(objectCount),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    const QString startDirectory = m_currentPath.isEmpty()
        ? QString()
        : QFileInfo(m_currentPath).absolutePath();
    const QString directory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Каталог для сохранения"), startDirectory);
    if (directory.isEmpty())
        return;

    ExtractionReport report;
    QString error;
    ExtractionController controller;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = controller.extractAll(*m_reader, directory, &report, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::critical(this, QStringLiteral("Ошибка извлечения"), error);
        return;
    }

    QMessageBox::information(this, QStringLiteral("Извлечение завершено"),
                             ExtractionController::allResultMessage(report));
    statusBar()->showMessage(QStringLiteral("Сохранено в %1").arg(report.outputDirectory));
}

void MainWindow::setModuleHighlighting(bool enabled)
{
    m_highlightEnabled = enabled;
    if (m_modulePanel)
        m_modulePanel->setHighlightEnabled(enabled);
    if (m_formView)
        m_formView->setModuleHighlightEnabled(enabled);
    QSettings settings;
    settings.setValue(settingsKey(QStringLiteral("moduleHighlight")), enabled);
}

void MainWindow::applyTheme(const QString &theme, bool persist)
{
    m_theme = theme.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("dark")
        : QStringLiteral("light");
    m_themeController->apply(m_theme, persist);
}

void MainWindow::showAbout()
{
    const QString version = QCoreApplication::applicationVersion();
    QMessageBox::about(
        this, QStringLiteral("О программе V8 Reader"),
        QStringLiteral(
            "<h3>V8 Reader %1</h3>"
            "<p>Просмотр конфигураций и внешних обработок "
            "1С:Предприятие 8.</p>"
            "<p>Поддерживаемые файлы: <b>.cf</b>, <b>.cfe</b>, "
            "<b>.epf</b>, <b>.erf</b>.</p>"
            "<p>Форматы контейнеров 1C:V8: 1.5 и 1.6.</p>"
            "<p>Qt %2.</p>"
            "<p>Программа работает только на чтение и не изменяет исходные файлы.</p>")
            .arg(version, QString::fromLatin1(qVersion())));
}

void MainWindow::onTreeSelectionChanged()
{
    if (!m_reader)
        return;

    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item) {
        showConfiguration();
        return;
    }

    const int kind = item->data(0, KindRole).toInt();
    if (kind == RootItem) {
        showConfiguration();
        return;
    }

    if (kind == GroupItem) {
        QVector<const MetadataCategory *> commonCategories;
        for (const MetadataCategory &category : m_reader->configuration().categories) {
            if (isCommonCategoryGuid(category.guid))
                commonCategories.append(&category);
        }
        showGroup(QStringLiteral("Общие"), commonCategories);
        return;
    }

    if (kind == CategoryItem) {
        if (MetadataCategory *category = selectedCategory())
            showCategory(*category);
        else
            showPlaceholderCategory(item->data(0, GuidRole).toString());
        return;
    }

    if (kind == RootModuleItem) {
        showRootModule(item->data(0, ModuleIndexRole).toInt());
        return;
    }

    MetadataObject *object = objectForItem(item);
    if (!object) {
        showConfiguration();
        return;
    }

    switch (kind) {
    case ObjectItem:
        ensureObjectItemChildren(object->guid);
        showObject(*object);
        break;
    case SectionItem:
        showSection(*object,
                    static_cast<MetadataSectionKind>(
                        item->data(0, SectionKindRole).toInt()));
        break;
    case EntryItem:
        showEntry(*object,
                  static_cast<MetadataSectionKind>(
                      item->data(0, SectionKindRole).toInt()),
                  item->data(0, EntryIndexRole).toInt(),
                  item->data(0, ChildIndexRole).toInt());
        break;
    case ModuleNodeItem:
        showModuleNode(*object, item->data(0, ModuleIndexRole).toInt());
        break;
    default:
        showConfiguration();
        break;
    }
}

MetadataObject *MainWindow::selectedObject()
{
    if (!m_reader)
        return nullptr;
    const QTreeWidgetItem *item = m_tree->currentItem();
    if (!item || item->data(0, KindRole).toInt() != ObjectItem)
        return nullptr;
    const QString guid = item->data(0, GuidRole).toString();
    return m_reader->configuration().object(guid);
}

MetadataCategory *MainWindow::selectedCategory()
{
    if (!m_reader)
        return nullptr;
    const QTreeWidgetItem *item = m_tree->currentItem();
    if (!item || item->data(0, KindRole).toInt() != CategoryItem)
        return nullptr;
    const QString guid = item->data(0, GuidRole).toString();
    return m_reader->configuration().category(guid);
}

void MainWindow::onContainerDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item)
        return;
    previewElement(item->data(0, IndexRole).toInt());
}

void MainWindow::onContainerContextMenu(const QPoint &position)
{
    QTreeWidgetItem *item = m_containerTree->itemAt(position);
    if (!item)
        return;

    const int index = item->data(0, IndexRole).toInt();
    QMenu menu(this);
    QAction *previewAction = menu.addAction(QStringLiteral("Просмотреть"));
    QAction *saveAction = menu.addAction(QStringLiteral("Сохранить данные…"));
    QAction *chosen = menu.exec(m_containerTree->viewport()->mapToGlobal(position));

    if (chosen == previewAction)
        previewElement(index);
    else if (chosen == saveAction)
        saveElement(index);
}

void MainWindow::previewElement(int index)
{
    if (!m_reader)
        return;

    const V8Element *element = m_reader->container().at(index);
    if (!element)
        return;

    QByteArray payload;
    QString error;
    if (!m_reader->container().elementPayload(*element, &payload, nullptr, &error)) {
        QMessageBox::warning(this, QStringLiteral("Ошибка чтения"), error);
        return;
    }

    auto *dialog = new PreviewDialog(element->name, payload, isProbablyText(payload), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::saveElement(int index)
{
    if (!m_reader)
        return;

    const V8Element *element = m_reader->container().at(index);
    if (!element)
        return;

    QByteArray payload;
    QString error;
    if (!m_reader->container().elementPayload(*element, &payload, nullptr, &error)) {
        QMessageBox::warning(this, QStringLiteral("Ошибка чтения"), error);
        return;
    }

    const bool text = isProbablyText(payload);
    const QString filter = text
        ? QStringLiteral("Текстовые файлы (*.txt);;Все файлы (*)")
        : QStringLiteral("Двоичные файлы (*.bin);;Все файлы (*)");
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Сохранить данные"),
                                                element->name, filter);
    if (path.isEmpty())
        return;

    if (!path.contains(QLatin1Char('.')))
        path += text ? QStringLiteral(".txt") : QStringLiteral(".bin");

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, QStringLiteral("Ошибка сохранения"),
                              file.errorString());
        return;
    }
    file.write(payload);
    statusBar()->showMessage(QStringLiteral("Сохранено: %1").arg(path));
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // Тему применяем после первого показа окна: смена палитры и таблицы
    // стилей до show() не перерисовывает уже созданные виджеты.
    if (!m_themeApplied) {
        m_themeApplied = true;
        applyTheme(m_theme, false);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue(settingsKey(QStringLiteral("geometry")), saveGeometry());
    settings.setValue(settingsKey(QStringLiteral("state")), saveState());
    if (m_recentFilesController)
        m_recentFilesController->save();
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls())
        return;

    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty() || !urls.first().isLocalFile())
        return;

    const QString suffix = QFileInfo(urls.first().toLocalFile()).suffix().toLower();
    if (suffix == QStringLiteral("cf") || suffix == QStringLiteral("cfe")
        || suffix == QStringLiteral("epf") || suffix == QStringLiteral("erf")) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls())
        return;

    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty() || !urls.first().isLocalFile())
        return;

    if (openFile(urls.first().toLocalFile()))
        event->acceptProposedAction();
}

QString MainWindow::settingsKey(const QString &name) const
{
    return QStringLiteral("MainWindow/") + name;
}

} // namespace v8reader
