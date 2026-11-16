#include "gui/MainWindow.h"

#include "core/ConfigurationReader.h"
#include "core/Extractor.h"
#include "core/V8Container.h"
#include "core/V8Text.h"
#include "gui/PreviewDialog.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
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
#include <QShortcut>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPoint>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
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

namespace v8reader {
namespace {

bool isProbablyText(const QByteArray &data)
{
    if (data.isEmpty())
        return false;
    if (data.startsWith("\xef\xbb\xbf")
        || data.startsWith("\xff\xfe")
        || data.startsWith("\xfe\xff")) {
        return true;
    }

    const int sampleSize = static_cast<int>(std::min<qsizetype>(data.size(), 8192));
    int zeros = 0;
    int controls = 0;
    int printable = 0;
    for (int i = 0; i < sampleSize; ++i) {
        const auto byte = static_cast<unsigned char>(data.at(i));
        if (byte == 0)
            ++zeros;
        else if (byte < 0x09 || (byte > 0x0d && byte < 0x20) || byte == 0x7f)
            ++controls;
        else if (byte >= 0x20)
            ++printable;
    }
    return zeros * 20 <= sampleSize
        && controls * 20 <= sampleSize
        && printable * 10 >= sampleSize * 9;
}

QString sizeText(qint64 size)
{
    if (size < 0)
        return QStringLiteral("—");
    if (size < 1024)
        return QStringLiteral("%1 байт").arg(size);
    if (size < 1024 * 1024)
        return QStringLiteral("%1 КБ").arg(size / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 МБ").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
}

QString formatDate(const QDateTime &dateTime)
{
    return dateTime.isValid()
        ? dateTime.toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"))
        : QStringLiteral("—");
}

QTableWidgetItem *readOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setToolTip(text);
    return item;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenus();
    setupToolBar();
    setupTree();
    setupTabs();
    loadRecentFiles();
    updateRecentMenu();

    QSettings settings;
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
    m_treeFilter->setPlaceholderText(QStringLiteral("Поиск объекта (Ctrl+F)…"));
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

    m_recentMenu = fileMenu->addMenu(QStringLiteral("Недавние файлы"));
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
}

void MainWindow::setupTabs()
{
    m_properties = new QTableWidget(0, 2);
    m_properties->setObjectName(QStringLiteral("propertiesTable"));
    m_properties->setHorizontalHeaderLabels(
        {QStringLiteral("Свойство"), QStringLiteral("Значение")});
    m_properties->verticalHeader()->setVisible(false);
    m_properties->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_properties->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_properties->setSelectionMode(QAbstractItemView::SingleSelection);
    m_properties->setWordWrap(true);
    m_properties->setAlternatingRowColors(true);
    m_properties->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_properties->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
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

    auto *modulePage = new QWidget();
    auto *moduleLayout = new QVBoxLayout(modulePage);
    moduleLayout->setContentsMargins(0, 4, 0, 0);
    m_moduleCombo = new QComboBox(modulePage);
    m_moduleCombo->setObjectName(QStringLiteral("moduleCombo"));
    m_moduleCombo->setEnabled(false);
    connect(m_moduleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModuleChanged);
    moduleLayout->addWidget(m_moduleCombo);

    m_moduleEdit = new QPlainTextEdit(modulePage);
    m_moduleEdit->setObjectName(QStringLiteral("moduleEdit"));
    m_moduleEdit->setReadOnly(true);
    m_moduleEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_moduleEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_moduleEdit->setPlaceholderText(QStringLiteral("Выберите объект метаданных"));
    moduleLayout->addWidget(m_moduleEdit, 1);
    m_tabs->addTab(modulePage, QStringLiteral("Модуль"));

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

    addRecentFile(m_currentPath);
    buildTree();
    buildContainerTree();
    showConfiguration();

    const Configuration &configuration = m_reader->configuration();
    m_treeHint->setText(QStringLiteral("%1\nОбъектов: %2")
                            .arg(QFileInfo(path).fileName())
                            .arg(configuration.objectCount()));
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

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *categoryItem = m_tree->topLevelItem(i);
        if (categoryItem->childCount() > 0) {
            m_tree->setCurrentItem(categoryItem->child(0));
            break;
        }
    }
    QCoreApplication::processEvents();

    lines << QStringLiteral("состав: %1 разделов").arg(m_composition->topLevelItemCount());
    lines << QStringLiteral("свойства: %1 строк").arg(m_properties->rowCount());
    lines << QStringLiteral("модуль: %1 символов")
                 .arg(m_moduleEdit->toPlainText().size());
    lines << QStringLiteral("метаданные: %1 символов")
                 .arg(m_metadataEdit->toPlainText().size());
    return lines.join(QLatin1Char('\n'));
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

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *categoryItem = m_tree->topLevelItem(i);
        for (int j = 0; j < categoryItem->childCount(); ++j) {
            QTreeWidgetItem *objectItem = categoryItem->child(j);
            if (objectItem->data(0, GuidRole).toString() == selectedGuid) {
                m_tree->setCurrentItem(objectItem);
                return;
            }
        }
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

    ExtractionOptions options;
    ExtractionReport report;
    QString error;
    Extractor extractor(*m_reader);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = extractor.extractObject(*m_currentObject, directory, options,
                                            &report, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::critical(this, QStringLiteral("Ошибка извлечения"), error);
        return;
    }

    QString message = QStringLiteral(
        "Объект «%1» сохранён.\n\n"
        "Файлов: %2\nМодулей: %3\nФорм: %4\nМакетов: %5\n\n"
        "Каталог:\n%6")
        .arg(m_currentObject->displayName())
        .arg(report.files.size())
        .arg(report.moduleCount)
        .arg(report.formCount)
        .arg(report.templateCount)
        .arg(report.outputDirectory);

    if (!report.warnings.isEmpty()) {
        message += QStringLiteral("\n\nПредупреждения:\n");
        const int count = std::min<int>(report.warnings.size(), 10);
        for (int i = 0; i < count; ++i)
            message += QStringLiteral("• %1\n").arg(report.warnings.at(i));
        if (report.warnings.size() > count)
            message += QStringLiteral("…\n");
    }

    QMessageBox::information(this, QStringLiteral("Извлечение завершено"), message);
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

    ExtractionOptions options;
    ExtractionReport report;
    QString error;
    Extractor extractor(*m_reader);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = extractor.extractAll(directory, options, &report, &error);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::critical(this, QStringLiteral("Ошибка извлечения"), error);
        return;
    }

    QString message = QStringLiteral(
        "Сохранено файлов: %1\nМодулей: %2\nФорм: %3\nМакетов: %4\n\nКаталог:\n%5")
        .arg(report.files.size())
        .arg(report.moduleCount)
        .arg(report.formCount)
        .arg(report.templateCount)
        .arg(report.outputDirectory);

    if (!report.warnings.isEmpty()) {
        message += QStringLiteral("\n\nПредупреждения:\n");
        const int count = std::min<int>(report.warnings.size(), 10);
        for (int i = 0; i < count; ++i)
            message += QStringLiteral("• %1\n").arg(report.warnings.at(i));
    }

    QMessageBox::information(this, QStringLiteral("Извлечение завершено"), message);
    statusBar()->showMessage(QStringLiteral("Сохранено в %1").arg(report.outputDirectory));
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, QStringLiteral("О программе V8 Reader"),
        QStringLiteral(
            "<h3>V8 Reader 0.1.0</h3>"
            "<p>Просмотр конфигураций и внешних обработок "
            "1С:Предприятие 8.</p>"
            "<p>Поддерживаемые файлы: <b>.cf</b>, <b>.cfe</b>, "
            "<b>.epf</b>, <b>.erf</b>.</p>"
            "<p>Форматы контейнеров 1C:V8: 1.5 и 1.6.</p>"
            "<p>Программа работает только на чтение и не изменяет исходные файлы.</p>"));
}

void MainWindow::buildTree()
{
    m_tree->clear();
    if (!m_reader)
        return;

    const Configuration &configuration = m_reader->configuration();
    const QIcon categoryIcon = style()->standardIcon(QStyle::SP_DirIcon);
    const QIcon objectIcon = style()->standardIcon(QStyle::SP_FileIcon);

    for (const MetadataCategory &category : configuration.categories) {
        auto *categoryItem = new QTreeWidgetItem(m_tree);
        categoryItem->setText(0, QStringLiteral("%1 (%2)")
                                     .arg(category.title)
                                     .arg(category.objects.size()));
        categoryItem->setIcon(0, categoryIcon);
        categoryItem->setData(0, KindRole, CategoryItem);
        categoryItem->setData(0, GuidRole, category.guid);
        categoryItem->setToolTip(0, QStringLiteral("%1\nGUID: %2")
                                        .arg(category.title, category.guid));
        QFont font = categoryItem->font(0);
        font.setBold(true);
        categoryItem->setFont(0, font);

        for (const MetadataObject &object : category.objects) {
            auto *objectItem = new QTreeWidgetItem(categoryItem);
            objectItem->setText(0, object.displayName());
            objectItem->setIcon(0, objectIcon);
            objectItem->setData(0, KindRole, ObjectItem);
            objectItem->setData(0, GuidRole, object.guid);

            QString tooltip = QStringLiteral("%1: %2\nGUID: %3")
                                  .arg(object.typeTitle,
                                       object.displayName(),
                                       object.guid);
            if (!object.synonym.isEmpty())
                tooltip += QStringLiteral("\nСиноним: %1").arg(object.synonym);
            if (!object.elementFound)
                tooltip += QStringLiteral("\nЭлемент контейнера не найден");
            objectItem->setToolTip(0, tooltip);
        }

        categoryItem->setExpanded(category.objects.size() <= 50);
    }

    applyTreeFilter(m_treeFilter->text());

    if (m_tree->topLevelItemCount() == 0) {
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, QStringLiteral("Объекты метаданных не найдены"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setForeground(0, palette().mid());
    }
}

void MainWindow::applyTreeFilter(const QString &text)
{
    const QString needle = text.trimmed();

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *categoryItem = m_tree->topLevelItem(i);
        if (categoryItem->data(0, KindRole).toInt() != CategoryItem)
            continue;

        int visible = 0;
        for (int j = 0; j < categoryItem->childCount(); ++j) {
            QTreeWidgetItem *objectItem = categoryItem->child(j);
            const QString name = objectItem->text(0);
            const QString guid = objectItem->data(0, GuidRole).toString();
            const bool match = needle.isEmpty()
                || name.contains(needle, Qt::CaseInsensitive)
                || guid.contains(needle, Qt::CaseInsensitive);
            objectItem->setHidden(!match);
            if (match)
                ++visible;
        }

        categoryItem->setHidden(!needle.isEmpty() && visible == 0);
        if (!needle.isEmpty() && visible > 0)
            categoryItem->setExpanded(true);
    }
}

void MainWindow::buildContainerTree()
{
    m_containerTree->clear();
    if (!m_reader)
        return;

    const V8Container &container = m_reader->container();
    for (int i = 0; i < container.count(); ++i) {
        const V8Element *element = container.at(i);
        if (!element)
            continue;

        auto *item = new QTreeWidgetItem(m_containerTree);
        item->setText(0, element->name);
        item->setText(1, formatDate(element->creationDate));
        item->setText(2, formatDate(element->modificationDate));
        item->setText(3, element->hasData() ? sizeText(element->dataSize)
                                            : QStringLiteral("нет данных"));
        item->setText(4, element->hasData()
                             ? QStringLiteral("0x%1")
                                   .arg(static_cast<qulonglong>(element->dataAddress), 0, 16)
                             : QStringLiteral("—"));
        item->setData(0, IndexRole, i);

        QString tooltip = element->name;
        if (!element->hasData())
            tooltip += QStringLiteral("\nЭлемент без данных");
        item->setToolTip(0, tooltip);
    }
}

void MainWindow::clearObjectView()
{
    m_currentObject = nullptr;
    m_currentModules.clear();
    m_moduleCombo->blockSignals(true);
    m_moduleCombo->clear();
    m_moduleCombo->setEnabled(false);
    m_moduleCombo->blockSignals(false);
    m_moduleEdit->clear();
    m_metadataEdit->clear();
    m_composition->clear();
}

void MainWindow::setProperties(const QVector<QPair<QString, QString>> &rows)
{
    m_properties->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QPair<QString, QString> &row = rows.at(i);
        m_properties->setItem(i, 0, readOnlyItem(row.first));
        m_properties->setItem(i, 1, readOnlyItem(row.second));
    }
    m_properties->resizeColumnToContents(0);
    m_properties->resizeRowsToContents();
}

void MainWindow::showConfiguration()
{
    clearObjectView();

    QVector<QPair<QString, QString>> rows;
    if (!m_reader) {
        rows.append({QStringLiteral("Файл"), QStringLiteral("не открыт")});
        rows.append({QStringLiteral("Подсказка"),
                     QStringLiteral("Откройте файл .cf, .cfe, .epf или .erf "
                                    "командой «Файл → Открыть…».")});
        setProperties(rows);
        return;
    }

    const Configuration &configuration = m_reader->configuration();
    rows.append({QStringLiteral("Имя"), configuration.name});
    rows.append({QStringLiteral("Синоним"), configuration.synonym});
    rows.append({QStringLiteral("Комментарий"), configuration.comment});
    rows.append({QStringLiteral("Тип файла"), configuration.kindTitle});
    rows.append({QStringLiteral("Версия"), configuration.version});
    rows.append({QStringLiteral("Формат"), configuration.formatLabel});
    rows.append({QStringLiteral("Файл"), configuration.sourcePath});
    rows.append({QStringLiteral("Размер"), sizeText(configuration.fileSize)});
    rows.append({QStringLiteral("GUID конфигурации"), configuration.configGuid});
    rows.append({QStringLiteral("Категорий"), QString::number(configuration.categories.size())});
    rows.append({QStringLiteral("Объектов"), QString::number(configuration.objectCount())});
    rows.append({QStringLiteral("Элементов контейнера"),
                 QString::number(m_reader->container().count())});

    QStringList categorySummary;
    for (const MetadataCategory &category : configuration.categories)
        categorySummary.append(QStringLiteral("%1: %2").arg(category.title)
                                   .arg(category.objects.size()));
    rows.append({QStringLiteral("Состав по категориям"),
                 categorySummary.join(QStringLiteral("\n"))});

    if (!configuration.warnings.isEmpty())
        rows.append({QStringLiteral("Предупреждения"),
                     configuration.warnings.join(QStringLiteral("\n"))});

    setProperties(rows);
}

void MainWindow::showCategory(const MetadataCategory &category)
{
    clearObjectView();

    QVector<QPair<QString, QString>> rows;
    rows.append({QStringLiteral("Категория"), category.title});
    rows.append({QStringLiteral("Тип объекта"), category.typeTitle});
    rows.append({QStringLiteral("GUID категории"), category.guid});
    rows.append({QStringLiteral("Объектов"), QString::number(category.objects.size())});

    QStringList names;
    for (const MetadataObject &object : category.objects)
        names.append(object.displayName());
    rows.append({QStringLiteral("Объекты"), names.join(QStringLiteral("\n"))});
    setProperties(rows);

    m_composition->clear();
    auto *root = new QTreeWidgetItem(m_composition);
    root->setText(0, QStringLiteral("Объекты"));
    root->setText(1, category.title);
    root->setText(3, category.guid);
    root->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    for (const MetadataObject &object : category.objects) {
        auto *item = new QTreeWidgetItem(root);
        item->setText(0, object.typeTitle);
        item->setText(1, object.displayName());
        item->setText(2, object.synonym);
        item->setText(3, object.guid);
        item->setText(4, object.comment);
        item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    }
    root->setExpanded(true);

    m_metadataEdit->setPlainText(
        QStringLiteral("Выбрана категория «%1». Выберите объект для просмотра "
                       "метаданных и модулей.")
            .arg(category.title));
    m_moduleEdit->setPlainText(QStringLiteral("Выберите объект метаданных."));

    m_extractAction->setEnabled(false);
    statusBar()->showMessage(QStringLiteral("Категория: %1 (%2 объектов)")
                                 .arg(category.title)
                                 .arg(category.objects.size()));
}

void MainWindow::showObject(MetadataObject &object)
{
    if (!m_reader)
        return;

    m_reader->loadSections(object);
    m_currentObject = &object;
    fillModules(object);
    fillComposition(object);
    fillMetadata(object);

    QVector<QPair<QString, QString>> rows;
    rows.append({QStringLiteral("Тип"), object.typeTitle});
    rows.append({QStringLiteral("Категория"), object.categoryTitle});
    rows.append({QStringLiteral("Имя"), object.name});
    rows.append({QStringLiteral("Синоним"), object.synonym});
    rows.append({QStringLiteral("Комментарий"), object.comment});
    rows.append({QStringLiteral("GUID"), object.guid});
    rows.append({QStringLiteral("Элемент контейнера"), object.elementName});
    rows.append({QStringLiteral("Элемент найден"),
                 object.elementFound ? QStringLiteral("да") : QStringLiteral("нет")});
    rows.append({QStringLiteral("Описание найдено"),
                 object.descriptorFound ? QStringLiteral("да") : QStringLiteral("нет")});
    rows.append({QStringLiteral("Модулей"), QString::number(m_currentModules.size())});

    for (const MetadataSection &section : object.sections) {
        rows.append({QStringLiteral("Состав: %1").arg(section.title),
                     QString::number(section.entries.size())});
    }

    setProperties(rows);
    m_extractAction->setEnabled(true);
    statusBar()->showMessage(QStringLiteral("%1: %2")
                                 .arg(object.typeTitle, object.displayName()));
}

void MainWindow::fillComposition(const MetadataObject &object)
{
    m_composition->clear();

    const QIcon sectionIcon = style()->standardIcon(QStyle::SP_DirIcon);
    const QIcon entryIcon = style()->standardIcon(QStyle::SP_FileIcon);

    for (const MetadataSection &section : object.sections) {
        auto *sectionItem = new QTreeWidgetItem(m_composition);
        sectionItem->setText(0, QStringLiteral("Раздел"));
        sectionItem->setText(1, QStringLiteral("%1 (%2)")
                                   .arg(section.title)
                                   .arg(section.entries.size()));
        sectionItem->setText(3, section.guid);
        sectionItem->setIcon(0, sectionIcon);
        QFont font = sectionItem->font(1);
        font.setBold(true);
        sectionItem->setFont(1, font);

        for (const MetadataEntry &entry : section.entries) {
            auto *entryItem = new QTreeWidgetItem(sectionItem);
            entryItem->setText(0, entry.typeTitle);
            entryItem->setText(1, entry.displayName());
            entryItem->setText(2, entry.synonym);
            entryItem->setText(3, entry.guid);
            entryItem->setText(4, entry.comment);
            entryItem->setIcon(0, entryIcon);

            for (const MetadataEntry &child : entry.children) {
                auto *childItem = new QTreeWidgetItem(entryItem);
                childItem->setText(0, child.typeTitle);
                childItem->setText(1, child.displayName());
                childItem->setText(2, child.synonym);
                childItem->setText(3, child.guid);
                childItem->setText(4, child.comment);
                childItem->setIcon(0, entryIcon);
            }
        }

        sectionItem->setExpanded(true);
    }

    if (m_composition->topLevelItemCount() == 0) {
        auto *item = new QTreeWidgetItem(m_composition);
        item->setText(0, QStringLiteral("Состав объекта не найден"));
    }
}

void MainWindow::fillModules(const MetadataObject &object)
{
    m_moduleCombo->blockSignals(true);
    m_moduleCombo->clear();

    if (!m_moduleCache.contains(object.guid)) {
        ModuleExtractor extractor(m_reader->container());
        m_moduleCache.insert(object.guid, extractor.extractForObject(object));
    }
    m_currentModules = m_moduleCache.value(object.guid);

    for (const ModuleInfo &module : m_currentModules) {
        QString title = module.name;
        if (title.isEmpty())
            title = module.elementName;
        if (!module.elementName.isEmpty() && !title.contains(module.elementName))
            title += QStringLiteral(" — %1").arg(module.elementName);
        m_moduleCombo->addItem(title);
    }

    const bool hasModules = !m_currentModules.isEmpty();
    m_moduleCombo->setEnabled(hasModules);
    m_moduleCombo->blockSignals(false);

    if (hasModules) {
        m_moduleCombo->setCurrentIndex(0);
        onModuleChanged(0);
    } else {
        m_moduleEdit->setPlainText(QStringLiteral("Модули объекта не найдены."));
    }
}

void MainWindow::fillMetadata(const MetadataObject &object)
{
    QString text;
    QString error;
    if (!m_reader->readObjectText(object, &text, &error)) {
        m_metadataEdit->setPlainText(
            QStringLiteral("Не удалось прочитать метаданные объекта:\n%1").arg(error));
        return;
    }

    if (text.isEmpty()) {
        m_metadataEdit->setPlainText(QStringLiteral("Метаданные объекта пусты."));
        return;
    }
    m_metadataEdit->setPlainText(text);
}

void MainWindow::onModuleChanged(int index)
{
    if (index < 0 || index >= m_currentModules.size()) {
        m_moduleEdit->clear();
        return;
    }
    m_moduleEdit->setPlainText(m_currentModules.at(index).text);
}

void MainWindow::onTreeSelectionChanged()
{
    if (!m_reader)
        return;

    if (MetadataObject *object = selectedObject()) {
        showObject(*object);
        return;
    }

    if (MetadataCategory *category = selectedCategory()) {
        showCategory(*category);
        return;
    }

    showConfiguration();
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

void MainWindow::loadRecentFiles()
{
    QSettings settings;
    const QStringList stored = settings.value(
        settingsKey(QStringLiteral("recentFiles"))).toStringList();

    m_recentFiles.clear();
    for (const QString &path : stored) {
        if (!path.trimmed().isEmpty()
            && !m_recentFiles.contains(path, Qt::CaseInsensitive)) {
            m_recentFiles.append(path);
        }
        if (m_recentFiles.size() >= 10)
            break;
    }
}

void MainWindow::saveRecentFiles()
{
    QSettings settings;
    settings.setValue(settingsKey(QStringLiteral("recentFiles")), m_recentFiles);
}

void MainWindow::addRecentFile(const QString &path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();

    QStringList filtered;
    filtered.append(absolute);
    for (const QString &existing : m_recentFiles) {
        if (existing.compare(absolute, Qt::CaseInsensitive) == 0)
            continue;
        filtered.append(existing);
        if (filtered.size() >= 10)
            break;
    }
    m_recentFiles = filtered;

    saveRecentFiles();
    updateRecentMenu();
}

void MainWindow::updateRecentMenu()
{
    m_recentMenu->clear();

    if (m_recentFiles.isEmpty()) {
        QAction *emptyAction = m_recentMenu->addAction(
            QStringLiteral("Нет недавних файлов"));
        emptyAction->setEnabled(false);
        return;
    }

    for (const QString &path : m_recentFiles) {
        const QFileInfo info(path);
        auto *action = m_recentMenu->addAction(info.fileName());
        action->setToolTip(path);
        action->setData(path);
        connect(action, &QAction::triggered, this, &MainWindow::openRecentFile);
    }

    m_recentMenu->addSeparator();
    QAction *clearAction = m_recentMenu->addAction(QStringLiteral("Очистить список"));
    connect(clearAction, &QAction::triggered, this, [this] {
        m_recentFiles.clear();
        saveRecentFiles();
        updateRecentMenu();
    });
}

void MainWindow::openRecentFile()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;

    const QString path = action->data().toString();
    if (path.isEmpty())
        return;

    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, QStringLiteral("Файл не найден"),
                             QStringLiteral("Файл «%1» больше не существует.").arg(path));
        m_recentFiles.removeAll(path);
        saveRecentFiles();
        updateRecentMenu();
        return;
    }

    openFile(path);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue(settingsKey(QStringLiteral("geometry")), saveGeometry());
    settings.setValue(settingsKey(QStringLiteral("state")), saveState());
    saveRecentFiles();
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
