#include "gui/MainWindow.h"

#include "core/ConfigurationReader.h"
#include "core/Extractor.h"
#include "core/V8Container.h"
#include "core/V8Text.h"
#include "gui/PreviewDialog.h"

#include <QAction>
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
#include <QPlainTextEdit>
#include <QPoint>
#include <QSet>
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
#include <functional>

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

QIcon makeFolderIcon(const QColor &color)
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color.darker(140), 1));
    painter.setBrush(QBrush(color));
    painter.drawRoundedRect(QRectF(1.5, 3.0, 13.0, 10.5), 2.0, 2.0);
    painter.setBrush(QBrush(color.lighter(130)));
    painter.drawRoundedRect(QRectF(1.5, 1.8, 6.0, 3.4), 1.4, 1.4);
    painter.end();
    return QIcon(pixmap);
}

QIcon makeDotIcon(const QColor &color)
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color.darker(140), 1));
    painter.setBrush(QBrush(color));
    painter.drawEllipse(QRectF(3.0, 3.0, 10.0, 10.0));
    painter.end();
    return QIcon(pixmap);
}

// Цвет значка для категории объектов метаданных.
QColor categoryColor(const QString &guid)
{
    static const QHash<QString, QColor> colors = [] {
        QHash<QString, QColor> map;
        const auto add = [&map](const char *categoryGuid, const char *color) {
            map.insert(QString::fromLatin1(categoryGuid), QColor(QString::fromLatin1(color)));
        };
        add("0195e80c-b157-11d4-9435-004095e12fc7", "#B8860B"); // Константы
        add("cf4abea6-37b2-11d4-940f-008048da11f9", "#1E88E5"); // Справочники
        add("061d872a-5787-460e-95ac-ed74ea3a3e84", "#43A047"); // Документы
        add("4612bd75-71b7-4a5c-8cc5-2b0b65f9fa0d", "#00897B"); // Журналы документов
        add("f6a80749-5ad7-400b-8519-39dc5dff2542", "#8E24AA"); // Перечисления
        add("631b75a0-29e2-11d6-a3c7-0050bae0a776", "#EF6C00"); // Отчеты
        add("bf845118-327b-4682-b5c6-285d2a0eb296", "#D81B60"); // Обработки
        add("857c4a91-e5f4-4fac-86ec-787626f1c108", "#6D4C41"); // Планы обмена
        add("238e7e88-3c5f-48b2-8a3b-81ebbecb20ed", "#37474F"); // Планы счетов
        add("82a1b659-b220-4d94-a9bd-14d757b95a48", "#5E35B1"); // ПВХ
        add("30b100d6-b29f-47ac-aec7-cb8ca8a54767", "#546E7A"); // ПВР
        add("13134201-f60b-11d5-a3c7-0050bae0a776", "#039BE5"); // Регистры сведений
        add("b64d9a40-1642-11d6-a3c7-0050bae0a776", "#00ACC1"); // Регистры накопления
        add("2deed9b8-0056-4ffe-a473-c20a6c32a0bc", "#3949AB"); // Регистры бухгалтерии
        add("f2de87a8-64e5-45eb-a22d-b3aedab050e7", "#7CB342"); // Регистры расчета
        add("bc587f20-35d9-11d6-a3c7-0050bae0a776", "#8D6E63"); // Последовательности
        add("fcd3404e-1523-48ce-9bc0-ecdb822684a1", "#C0CA33"); // Бизнес-процессы
        add("3e63355c-1378-4953-be9b-1deb5fb6bec5", "#F4511E"); // Задачи
        add("5274d9fc-9c3a-4a71-8f5e-a0db8ab23de5", "#6D4C41"); // Внешние источники
        add("0fe48980-252d-11d6-a3c7-0050bae0a776", "#FB8C00"); // Общие модули
        add("07ee8426-87f1-11d5-b99c-0050bae0a95d", "#29B6F6"); // Общие формы
        add("0c89c792-16c3-11d5-b96b-0050bae0a95d", "#AB47BC"); // Общие макеты
        return map;
    }();
    const auto it = colors.constFind(guid.toLower());
    return it != colors.constEnd() ? it.value() : QColor(QStringLiteral("#78909C"));
}

QColor sectionColor(MetadataSectionKind kind)
{
    switch (kind) {
    case MetadataSectionKind::Templates:
        return QColor(QStringLiteral("#8E24AA"));
    case MetadataSectionKind::Commands:
        return QColor(QStringLiteral("#D81B60"));
    case MetadataSectionKind::TabularSections:
        return QColor(QStringLiteral("#6D4C41"));
    case MetadataSectionKind::Attributes:
        return QColor(QStringLiteral("#2E7D32"));
    case MetadataSectionKind::Forms:
        return QColor(QStringLiteral("#1565C0"));
    case MetadataSectionKind::TabularAttributes:
        return QColor(QStringLiteral("#558B2F"));
    case MetadataSectionKind::Dimensions:
        return QColor(QStringLiteral("#00838F"));
    case MetadataSectionKind::Resources:
        return QColor(QStringLiteral("#0277BD"));
    case MetadataSectionKind::Values:
        return QColor(QStringLiteral("#8E24AA"));
    case MetadataSectionKind::AccountingFlags:
    case MetadataSectionKind::SubcontoFlags:
        return QColor(QStringLiteral("#616161"));
    case MetadataSectionKind::Columns:
    case MetadataSectionKind::JournalColumns:
        return QColor(QStringLiteral("#546E7A"));
    }
    return QColor(QStringLiteral("#546E7A"));
}

// Категории, которые в дереве 1С собраны в группу «Общие».
bool isCommonCategoryGuid(const QString &guid)
{
    static const QSet<QString> guids = [] {
        QSet<QString> set;
        const char *values[] = {
            "0fe48980-252d-11d6-a3c7-0050bae0a776", // Общие модули
            "07ee8426-87f1-11d5-b99c-0050bae0a95d", // Общие формы
            "0c89c792-16c3-11d5-b96b-0050bae0a95d", // Общие макеты
            "7dcd43d9-aca5-4926-b549-1842e6a4e8cf", // Общие картинки
            "2f1a5187-fb0e-4b05-9489-dc5dd6412348", // Общие команды
            "1c57eabe-7349-44b3-b1de-ebfeab67b47d", // Группы команд
            "15794563-ccec-41f6-a83c-ec5f7b9a5bc1", // Общие реквизиты
            "09736b02-9cac-4e3f-b4f7-d3e9576ab948", // Роли
            "37f2fa9a-b276-11d4-9435-004095e12fc7", // Подсистемы
            "24c43748-c938-45d0-8d14-01424a72b11e", // Параметры сеанса
            "af547940-3268-434f-a3e7-e47d6d2638c3", // Функциональные опции
            "30d554db-541e-4f62-8970-a1c6dcfeb2bc", // Параметры функц. опций
            "c045099e-13b9-4fb6-9d50-fca00202971e", // Определяемые типы
            "3e7bfcc0-067d-11d6-a3c7-0050bae0a776", // Критерии отбора
            "4e828da6-0f44-4b5b-b1c0-a2b3cfe7bdcc", // Подписки на события
            "11bdaf85-d5ad-4d91-bb24-aa0eee139052", // Регламентные задания
            "46b4cd97-fd13-4eaa-aba2-3bddd7699218", // Хранилища настроек
            "3e5404af-6ef8-4c73-ad11-91bd2dfac4c8", // Стили
            "58848766-36ea-4076-8800-e91eb49590d7", // Элементы стиля
            "9cd510ce-abfc-11d4-9434-004095e12fc7", // Языки
            "39bddf6a-0c3c-452b-921c-d99cfa1c2f1b", // Интерфейсы
            "0fffc09c-8f4c-47cc-b41c-8d5c5a221d79", // HTTP-сервисы
            "8657032e-7740-4e1d-a3ba-5dd6e8afb78f", // Web-сервисы
            "d26096fb-7a5d-4df9-af63-47d04771fa9b", // WS-ссылки
            "cc9df798-7c94-4616-97d2-7aa0b7bc515e", // XDTO-пакеты
            "6e6dc072-b7ac-41e7-8f88-278d25b6da2a", // Боты
            "a7641777-7813-45c6-96ef-9d51587a6ac6", // WebSocket-клиенты
            "bf3420b0-f6f9-41a0-b83a-fe9d4ab0b65d", // Сервисы интеграции
        };
        for (const char *value : values)
            set.insert(QString::fromLatin1(value));
        return set;
    }();
    return guids.contains(guid.toLower());
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_rootIcon = makeFolderIcon(QColor(QStringLiteral("#1F3A93")));
    m_groupIcon = makeFolderIcon(QColor(QStringLiteral("#546E7A")));
    m_categoryIcon = makeFolderIcon(QColor(QStringLiteral("#607D8B")));
    m_sectionIcon = makeFolderIcon(QColor(QStringLiteral("#6D4C41")));
    m_entryIcon = makeDotIcon(QColor(QStringLiteral("#546E7A")));
    m_moduleIcon = makeDotIcon(QColor(QStringLiteral("#F57C00")));

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
    connect(m_tree, &QTreeWidget::itemExpanded,
            this, &MainWindow::onTreeItemExpanded);
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
                 .arg(m_moduleEdit->toPlainText().size());
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

    lines << QStringLiteral("узлы объекта: разделов %1, модулей %2")
                 .arg(sectionCount)
                 .arg(moduleCount);
    lines << QStringLiteral("элементов выбранного раздела: %1").arg(entryCount);
    lines << QStringLiteral("состав после выбора: %1")
                 .arg(m_composition->topLevelItemCount());
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

    Configuration &configuration = m_reader->configuration();
    m_tree->setUpdatesEnabled(false);

    auto *rootItem = new QTreeWidgetItem(m_tree);
    rootItem->setText(0, QStringLiteral("%1 (%2)")
                             .arg(configuration.displayName(),
                                  configuration.kindTitle));
    rootItem->setIcon(0, m_rootIcon);
    rootItem->setData(0, KindRole, RootItem);
    rootItem->setToolTip(0, QStringLiteral("%1\nФайл: %2\nОбъектов: %3")
                                .arg(configuration.kindTitle,
                                     configuration.sourcePath)
                                .arg(configuration.objectCount()));
    {
        QFont font = rootItem->font(0);
        font.setBold(true);
        rootItem->setFont(0, font);
    }

    QVector<const MetadataCategory *> commonCategories;
    QVector<const MetadataCategory *> mainCategories;
    for (MetadataCategory &category : configuration.categories) {
        if (isCommonCategoryGuid(category.guid))
            commonCategories.append(&category);
        else
            mainCategories.append(&category);
    }

    if (!commonCategories.isEmpty()) {
        int objectCount = 0;
        for (const MetadataCategory *category : commonCategories)
            objectCount += category->objects.size();

        auto *groupItem = new QTreeWidgetItem(rootItem);
        groupItem->setText(0, QStringLiteral("Общие (%1)").arg(objectCount));
        groupItem->setIcon(0, m_groupIcon);
        groupItem->setData(0, KindRole, GroupItem);
        groupItem->setData(0, GuidRole, QStringLiteral("__common__"));
        groupItem->setToolTip(0, QStringLiteral("Общие объекты конфигурации"));
        QFont groupFont = groupItem->font(0);
        groupFont.setBold(true);
        groupItem->setFont(0, groupFont);

        for (const MetadataCategory *category : commonCategories)
            addCategoryItem(groupItem, *category);
    }

    for (const MetadataCategory *category : mainCategories)
        addCategoryItem(rootItem, *category);

    rootItem->setExpanded(true);
    if (rootItem->childCount() > 0)
        rootItem->child(0)->setExpanded(true);

    m_tree->setUpdatesEnabled(true);

    applyTreeFilter(m_treeFilter->text());
    updateTreeHint();

    if (m_tree->topLevelItemCount() == 0) {
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, QStringLiteral("Объекты метаданных не найдены"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setForeground(0, palette().mid());
    }
}

QTreeWidgetItem *MainWindow::addCategoryItem(QTreeWidgetItem *parent,
                                             const MetadataCategory &category)
{
    auto *categoryItem = new QTreeWidgetItem(parent);
    categoryItem->setText(0, QStringLiteral("%1 (%2)")
                                 .arg(category.title)
                                 .arg(category.objects.size()));
    categoryItem->setIcon(0, makeFolderIcon(categoryColor(category.guid)));
    categoryItem->setData(0, KindRole, CategoryItem);
    categoryItem->setData(0, GuidRole, category.guid);
    categoryItem->setToolTip(0, QStringLiteral("%1\nТип объекта: %2\nGUID: %3\nОбъектов: %4")
                                    .arg(category.title, category.typeTitle, category.guid)
                                    .arg(category.objects.size()));
    QFont font = categoryItem->font(0);
    font.setBold(true);
    categoryItem->setFont(0, font);

    for (const MetadataObject &object : category.objects)
        addObjectItem(categoryItem, object);

    categoryItem->setExpanded(category.objects.size() <= 30);
    return categoryItem;
}

QTreeWidgetItem *MainWindow::addObjectItem(QTreeWidgetItem *parent,
                                           const MetadataObject &object)
{
    auto *objectItem = new QTreeWidgetItem(parent);
    objectItem->setText(0, object.displayName());
    objectItem->setIcon(0, makeDotIcon(categoryColor(object.categoryGuid)));
    objectItem->setData(0, KindRole, ObjectItem);
    objectItem->setData(0, GuidRole, object.guid);
    objectItem->setData(0, LoadedRole, false);

    QString tooltip = QStringLiteral("%1: %2\nGUID: %3")
                          .arg(object.typeTitle, object.displayName(), object.guid);
    if (!object.synonym.isEmpty())
        tooltip += QStringLiteral("\nСиноним: %1").arg(object.synonym);
    if (!object.comment.isEmpty())
        tooltip += QStringLiteral("\nКомментарий: %1").arg(object.comment);
    if (!object.elementFound)
        tooltip += QStringLiteral("\nЭлемент контейнера не найден");
    objectItem->setToolTip(0, tooltip);

    if (object.elementFound) {
        // Заглушка, чтобы у объекта был раскрыватель; заменяется при раскрытии.
        auto *placeholder = new QTreeWidgetItem(objectItem);
        placeholder->setText(0, QStringLiteral("…"));
        placeholder->setFlags(placeholder->flags() & ~Qt::ItemIsSelectable);
    }

    return objectItem;
}

void MainWindow::populateObjectItem(QTreeWidgetItem *item, MetadataObject &object)
{
    if (!item || !m_reader)
        return;

    item->setData(0, LoadedRole, true);
    while (item->childCount() > 0)
        delete item->takeChild(0);

    m_reader->loadSections(object);

    if (!m_moduleCache.contains(object.guid)) {
        ModuleExtractor extractor(m_reader->container());
        m_moduleCache.insert(object.guid, extractor.extractForObject(object));
    }
    const QVector<ModuleInfo> modules = m_moduleCache.value(object.guid);
    for (int i = 0; i < modules.size(); ++i) {
        const ModuleInfo &module = modules.at(i);
        auto *moduleItem = new QTreeWidgetItem(item);
        moduleItem->setText(0, module.name.isEmpty() ? QStringLiteral("Модуль")
                                                     : module.name);
        moduleItem->setIcon(0, m_moduleIcon);
        moduleItem->setData(0, KindRole, ModuleNodeItem);
        moduleItem->setData(0, GuidRole, object.guid);
        moduleItem->setData(0, ModuleIndexRole, i);
        moduleItem->setToolTip(0, QStringLiteral("%1\nЭлемент: %2")
                                    .arg(module.name, module.elementName));
    }

    for (const MetadataSection &section : object.sections) {
        if (section.entries.isEmpty())
            continue;

        auto *sectionItem = new QTreeWidgetItem(item);
        sectionItem->setText(0, QStringLiteral("%1 (%2)")
                                    .arg(section.title)
                                    .arg(section.entries.size()));
        sectionItem->setIcon(0, makeFolderIcon(sectionColor(section.kind)));
        sectionItem->setData(0, KindRole, SectionItem);
        sectionItem->setData(0, GuidRole, object.guid);
        sectionItem->setData(0, SectionKindRole, static_cast<int>(section.kind));
        sectionItem->setToolTip(0, QStringLiteral("%1\nGUID раздела: %2")
                                    .arg(section.title, section.guid));
        QFont sectionFont = sectionItem->font(0);
        sectionFont.setBold(true);
        sectionItem->setFont(0, sectionFont);

        for (int i = 0; i < section.entries.size(); ++i) {
            const MetadataEntry &entry = section.entries.at(i);
            auto *entryItem = new QTreeWidgetItem(sectionItem);
            entryItem->setText(0, entry.displayName());
            entryItem->setIcon(0, makeDotIcon(sectionColor(section.kind)));
            entryItem->setData(0, KindRole, EntryItem);
            entryItem->setData(0, GuidRole, object.guid);
            entryItem->setData(0, SectionKindRole, static_cast<int>(section.kind));
            entryItem->setData(0, EntryIndexRole, i);
            entryItem->setData(0, ChildIndexRole, -1);

            QString entryTooltip = QStringLiteral("%1: %2")
                                       .arg(entry.typeTitle, entry.displayName());
            if (!entry.guid.isEmpty())
                entryTooltip += QStringLiteral("\nGUID: %1").arg(entry.guid);
            if (!entry.synonym.isEmpty())
                entryTooltip += QStringLiteral("\nСиноним: %1").arg(entry.synonym);
            if (!entry.comment.isEmpty())
                entryTooltip += QStringLiteral("\nКомментарий: %1").arg(entry.comment);
            entryItem->setToolTip(0, entryTooltip);

            for (int j = 0; j < entry.children.size(); ++j) {
                const MetadataEntry &child = entry.children.at(j);
                auto *childItem = new QTreeWidgetItem(entryItem);
                childItem->setText(0, child.displayName());
                childItem->setIcon(0, makeDotIcon(
                    sectionColor(MetadataSectionKind::TabularAttributes)));
                childItem->setData(0, KindRole, EntryItem);
                childItem->setData(0, GuidRole, object.guid);
                childItem->setData(0, SectionKindRole,
                                   static_cast<int>(MetadataSectionKind::TabularAttributes));
                childItem->setData(0, EntryIndexRole, i);
                childItem->setData(0, ChildIndexRole, j);
                childItem->setToolTip(0, QStringLiteral("%1: %2\nGUID: %3")
                                             .arg(child.typeTitle,
                                                  child.displayName(),
                                                  child.guid));
            }
        }
        sectionItem->setExpanded(true);
    }

    if (item->childCount() > 0) {
        item->setExpanded(true);
    } else {
        auto *emptyItem = new QTreeWidgetItem(item);
        emptyItem->setText(0, QStringLiteral("(состав и модули не найдены)"));
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        emptyItem->setForeground(0, palette().mid());
    }
}

void MainWindow::onTreeItemExpanded(QTreeWidgetItem *item)
{
    if (!item || !m_reader)
        return;
    if (item->data(0, KindRole).toInt() != ObjectItem)
        return;
    if (item->data(0, LoadedRole).toBool())
        return;

    MetadataObject *object = objectForItem(item);
    if (!object)
        return;

    populateObjectItem(item, *object);
    applyTreeFilter(m_treeFilter->text());
}

void MainWindow::ensureObjectItemChildren(const QString &guid)
{
    QTreeWidgetItem *item = findObjectItem(guid);
    if (!item || !m_reader)
        return;
    MetadataObject *object = objectForItem(item);
    if (object && !item->data(0, LoadedRole).toBool()) {
        populateObjectItem(item, *object);
        applyTreeFilter(m_treeFilter->text());
    }
}

MetadataObject *MainWindow::objectForItem(const QTreeWidgetItem *item) const
{
    if (!item || !m_reader)
        return nullptr;
    const int kind = item->data(0, KindRole).toInt();
    if (kind != ObjectItem && kind != SectionItem && kind != EntryItem
        && kind != ModuleNodeItem) {
        return nullptr;
    }
    const QString guid = item->data(0, GuidRole).toString();
    return m_reader->configuration().object(guid);
}

QTreeWidgetItem *MainWindow::findObjectItem(const QString &guid) const
{
    if (!m_tree)
        return nullptr;

    std::function<QTreeWidgetItem *(QTreeWidgetItem *)> find =
        [&](QTreeWidgetItem *item) -> QTreeWidgetItem * {
        if (item->data(0, KindRole).toInt() == ObjectItem
            && item->data(0, GuidRole).toString() == guid) {
            return item;
        }
        for (int i = 0; i < item->childCount(); ++i) {
            if (QTreeWidgetItem *found = find(item->child(i)))
                return found;
        }
        return nullptr;
    };

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        if (QTreeWidgetItem *found = find(m_tree->topLevelItem(i)))
            return found;
    }
    return nullptr;
}

void MainWindow::applyTreeFilter(const QString &text)
{
    const QString needle = text.trimmed();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        filterTreeItem(m_tree->topLevelItem(i), needle);
}

bool MainWindow::filterTreeItem(QTreeWidgetItem *item, const QString &needle)
{
    const int kind = item->data(0, KindRole).toInt();

    if (needle.isEmpty()) {
        item->setHidden(false);
        for (int i = 0; i < item->childCount(); ++i)
            filterTreeItem(item->child(i), needle);
        return true;
    }

    bool selfMatch = false;
    if (kind == RootItem || kind == GroupItem || kind == CategoryItem
        || kind == ObjectItem || kind == SectionItem || kind == EntryItem) {
        selfMatch = item->text(0).contains(needle, Qt::CaseInsensitive)
            || item->data(0, GuidRole).toString().contains(needle, Qt::CaseInsensitive);
    }

    bool childMatch = false;
    for (int i = 0; i < item->childCount(); ++i) {
        if (filterTreeItem(item->child(i), needle))
            childMatch = true;
    }

    const bool visible = selfMatch || childMatch;
    item->setHidden(!visible);

    if (visible && (childMatch || kind == CategoryItem || kind == GroupItem))
        item->setExpanded(true);

    return visible;
}

void MainWindow::updateTreeHint()
{
    if (!m_reader) {
        m_treeHint->setText(QStringLiteral("Файл не открыт"));
        return;
    }

    const Configuration &configuration = m_reader->configuration();
    QString text = QStringLiteral("%1\nОбъектов: %2 · категорий: %3")
                       .arg(configuration.displayName())
                       .arg(configuration.objectCount())
                       .arg(configuration.categories.size());
    if (!configuration.warnings.isEmpty()) {
        text += QStringLiteral("\nПредупреждений: %1")
                    .arg(configuration.warnings.size());
    }
    m_treeHint->setText(text);
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
    constexpr int kNameLimit = 300;
    for (const MetadataObject &object : category.objects) {
        if (names.size() >= kNameLimit) {
            names.append(QStringLiteral("… ещё %1")
                             .arg(category.objects.size() - kNameLimit));
            break;
        }
        names.append(object.displayName());
    }
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
    ensureObjectItemChildren(object.guid);
    statusBar()->showMessage(QStringLiteral("%1: %2")
                                 .arg(object.typeTitle, object.displayName()));
}

void MainWindow::showGroup(const QString &title,
                           const QVector<const MetadataCategory *> &categories)
{
    clearObjectView();

    QVector<QPair<QString, QString>> rows;
    rows.append({QStringLiteral("Группа"), title});

    int objectCount = 0;
    QStringList categorySummary;
    for (const MetadataCategory *category : categories) {
        objectCount += category->objects.size();
        categorySummary.append(QStringLiteral("%1: %2")
                                   .arg(category->title)
                                   .arg(category->objects.size()));
    }
    rows.append({QStringLiteral("Категорий"), QString::number(categories.size())});
    rows.append({QStringLiteral("Объектов"), QString::number(objectCount)});
    rows.append({QStringLiteral("Состав группы"),
                 categorySummary.join(QStringLiteral("\n"))});
    setProperties(rows);

    m_composition->clear();
    auto *root = new QTreeWidgetItem(m_composition);
    root->setText(0, QStringLiteral("Категория"));
    root->setText(1, title);
    root->setIcon(0, m_groupIcon);
    for (const MetadataCategory *category : categories) {
        auto *categoryItem = new QTreeWidgetItem(root);
        categoryItem->setText(0, QStringLiteral("Категория"));
        categoryItem->setText(1, QStringLiteral("%1 (%2)")
                                     .arg(category->title)
                                     .arg(category->objects.size()));
        categoryItem->setText(3, category->guid);
        categoryItem->setIcon(0, makeFolderIcon(categoryColor(category->guid)));
        for (const MetadataObject &object : category->objects) {
            auto *objectItem = new QTreeWidgetItem(categoryItem);
            objectItem->setText(0, object.typeTitle);
            objectItem->setText(1, object.displayName());
            objectItem->setText(2, object.synonym);
            objectItem->setText(3, object.guid);
            objectItem->setText(4, object.comment);
            objectItem->setIcon(0, makeDotIcon(categoryColor(category->guid)));
        }
    }
    root->setExpanded(true);

    m_metadataEdit->setPlainText(
        QStringLiteral("Группа «%1». Выберите категорию или объект метаданных.")
            .arg(title));
    m_moduleEdit->setPlainText(QStringLiteral("Выберите объект метаданных."));
    m_extractAction->setEnabled(false);
    statusBar()->showMessage(QStringLiteral("Группа: %1 (%2 объектов)")
                                 .arg(title)
                                 .arg(objectCount));
}

void MainWindow::showSection(MetadataObject &object, MetadataSectionKind kind)
{
    if (!m_reader)
        return;

    m_reader->loadSections(object);
    const MetadataSection *section = object.section(kind);
    if (!section) {
        showObject(object);
        return;
    }

    clearObjectView();
    m_currentObject = &object;
    fillModules(object);
    fillSectionComposition(object, kind);

    QVector<QPair<QString, QString>> rows;
    rows.append({QStringLiteral("Раздел"), section->title});
    rows.append({QStringLiteral("Объект"), object.displayName()});
    rows.append({QStringLiteral("Тип объекта"), object.typeTitle});
    rows.append({QStringLiteral("Категория"), object.categoryTitle});
    rows.append({QStringLiteral("Элементов"), QString::number(section->entries.size())});
    rows.append({QStringLiteral("GUID раздела"), section->guid});
    rows.append({QStringLiteral("GUID объекта"), object.guid});
    setProperties(rows);

    m_metadataEdit->setPlainText(
        QStringLiteral("Раздел «%1» объекта «%2»: %3 элементов.\n"
                       "Выберите элемент для просмотра свойств.")
            .arg(section->title, object.displayName())
            .arg(section->entries.size()));

    m_extractAction->setEnabled(true);
    statusBar()->showMessage(QStringLiteral("%1: %2 (%3)")
                                 .arg(object.displayName(),
                                      section->title)
                                 .arg(section->entries.size()));
}

void MainWindow::showEntry(MetadataObject &object, MetadataSectionKind kind,
                           int entryIndex, int childIndex)
{
    if (!m_reader)
        return;

    m_reader->loadSections(object);
    const MetadataSection *section = object.section(kind);
    const MetadataSection *attributesSection = nullptr;
    if (!section && kind == MetadataSectionKind::TabularAttributes)
        attributesSection = object.section(MetadataSectionKind::TabularSections);
    if (!section && !attributesSection) {
        showObject(object);
        return;
    }

    QVector<MetadataEntry> emptyEntries;
    const QVector<MetadataEntry> &rootEntries =
        section ? section->entries : attributesSection->entries;
    if (entryIndex < 0 || entryIndex >= rootEntries.size()) {
        showObject(object);
        return;
    }

    const MetadataEntry &parentEntry = rootEntries.at(entryIndex);
    const MetadataEntry *entry = &parentEntry;
    if (childIndex >= 0 && childIndex < parentEntry.children.size())
        entry = &parentEntry.children.at(childIndex);

    clearObjectView();
    m_currentObject = &object;
    fillModules(object);
    fillEntryComposition(*entry);

    QVector<QPair<QString, QString>> rows;
    rows.append({QStringLiteral("Вид"), entry->typeTitle});
    rows.append({QStringLiteral("Имя"), entry->name});
    rows.append({QStringLiteral("Синоним"), entry->synonym});
    rows.append({QStringLiteral("Комментарий"), entry->comment});
    rows.append({QStringLiteral("GUID"), entry->guid});
    rows.append({QStringLiteral("Раздел"),
                 section ? section->title
                         : MetadataCatalog::sectionTitle(
                               MetadataSectionKind::TabularSections)});
    if (childIndex >= 0) {
        rows.append({QStringLiteral("Входит в"), parentEntry.displayName()});
        rows.append({QStringLiteral("Табличная часть"), parentEntry.guid});
    }
    rows.append({QStringLiteral("Объект"), object.displayName()});
    rows.append({QStringLiteral("Тип объекта"), object.typeTitle});
    setProperties(rows);

    m_metadataEdit->clear();
    if (!entry->guid.isEmpty()) {
        QString text;
        QString readError;
        if (m_reader->readElementText(entry->guid, &text, &readError)) {
            constexpr qsizetype kLimit = 1024 * 1024;
            if (text.size() > kLimit)
                text = text.left(kLimit) + QStringLiteral("\n… (обрезано)");
            m_metadataEdit->setPlainText(text);
        } else {
            m_metadataEdit->setPlainText(
                QStringLiteral("Не удалось прочитать элемент «%1»:\n%2")
                    .arg(entry->guid, readError));
        }
    }
    if (m_metadataEdit->toPlainText().isEmpty()) {
        m_metadataEdit->setPlainText(
            QStringLiteral("Для элемента нет отдельного текста метаданных в контейнере."));
    }

    // Для формы сразу открываем её модуль, если он найден.
    if (kind == MetadataSectionKind::Forms) {
        for (int i = 0; i < m_currentModules.size(); ++i) {
            if (m_currentModules.at(i).name.contains(entry->displayName(),
                                                     Qt::CaseInsensitive)) {
                m_moduleCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    m_extractAction->setEnabled(true);
    statusBar()->showMessage(QStringLiteral("%1: %2")
                                 .arg(entry->typeTitle, entry->displayName()));
}

void MainWindow::showModuleNode(MetadataObject &object, int moduleIndex)
{
    if (!m_reader)
        return;

    clearObjectView();
    m_currentObject = &object;
    fillModules(object);

    QVector<QPair<QString, QString>> rows;
    rows.append({QStringLiteral("Объект"), object.displayName()});
    rows.append({QStringLiteral("Тип объекта"), object.typeTitle});
    rows.append({QStringLiteral("GUID объекта"), object.guid});

    if (moduleIndex >= 0 && moduleIndex < m_currentModules.size()) {
        const ModuleInfo &module = m_currentModules.at(moduleIndex);
        rows.append({QStringLiteral("Модуль"), module.name});
        rows.append({QStringLiteral("Элемент контейнера"), module.elementName});
        rows.append({QStringLiteral("Источник"), module.source});
        rows.append({QStringLiteral("Символов"), QString::number(module.text.size())});
        m_moduleCombo->setCurrentIndex(moduleIndex);
        m_composition->clear();
        m_metadataEdit->setPlainText(
            QStringLiteral("Модуль «%1» объекта «%2».\nЭлемент контейнера: %3")
                .arg(module.name, object.displayName(), module.elementName));
    } else {
        rows.append({QStringLiteral("Модуль"), QStringLiteral("не найден")});
    }

    setProperties(rows);
    m_extractAction->setEnabled(true);
    statusBar()->showMessage(QStringLiteral("%1 — %2")
                                 .arg(object.displayName(),
                                      moduleIndex >= 0 && moduleIndex < m_currentModules.size()
                                          ? m_currentModules.at(moduleIndex).name
                                          : QStringLiteral("модуль")));
}

void MainWindow::fillSectionComposition(MetadataObject &object,
                                        MetadataSectionKind kind)
{
    m_composition->clear();
    const MetadataSection *section = object.section(kind);
    if (!section)
        return;

    auto *sectionItem = new QTreeWidgetItem(m_composition);
    sectionItem->setText(0, QStringLiteral("Раздел"));
    sectionItem->setText(1, QStringLiteral("%1 (%2)")
                               .arg(section->title)
                               .arg(section->entries.size()));
    sectionItem->setText(3, section->guid);
    sectionItem->setIcon(0, makeFolderIcon(sectionColor(kind)));

    for (const MetadataEntry &entry : section->entries) {
        auto *entryItem = new QTreeWidgetItem(sectionItem);
        entryItem->setText(0, entry.typeTitle);
        entryItem->setText(1, entry.displayName());
        entryItem->setText(2, entry.synonym);
        entryItem->setText(3, entry.guid);
        entryItem->setText(4, entry.comment);
        entryItem->setIcon(0, makeDotIcon(sectionColor(kind)));
        for (const MetadataEntry &child : entry.children) {
            auto *childItem = new QTreeWidgetItem(entryItem);
            childItem->setText(0, child.typeTitle);
            childItem->setText(1, child.displayName());
            childItem->setText(2, child.synonym);
            childItem->setText(3, child.guid);
            childItem->setText(4, child.comment);
            childItem->setIcon(0, makeDotIcon(
                sectionColor(MetadataSectionKind::TabularAttributes)));
        }
    }
    sectionItem->setExpanded(true);
}

void MainWindow::fillEntryComposition(const MetadataEntry &entry)
{
    m_composition->clear();

    auto *entryItem = new QTreeWidgetItem(m_composition);
    entryItem->setText(0, entry.typeTitle);
    entryItem->setText(1, entry.displayName());
    entryItem->setText(2, entry.synonym);
    entryItem->setText(3, entry.guid);
    entryItem->setText(4, entry.comment);
    entryItem->setIcon(0, makeDotIcon(sectionColor(MetadataSectionKind::Attributes)));

    for (const MetadataEntry &child : entry.children) {
        auto *childItem = new QTreeWidgetItem(entryItem);
        childItem->setText(0, child.typeTitle);
        childItem->setText(1, child.displayName());
        childItem->setText(2, child.synonym);
        childItem->setText(3, child.guid);
        childItem->setText(4, child.comment);
        childItem->setIcon(0, makeDotIcon(
            sectionColor(MetadataSectionKind::TabularAttributes)));
    }
    entryItem->setExpanded(true);
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
