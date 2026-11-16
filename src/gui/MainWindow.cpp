#include "gui/MainWindow.h"

#include "gui/BslHighlighter.h"
#include "core/ConfigurationReader.h"
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

QIcon metadataIcon(const QString &name)
{
    static QHash<QString, QIcon> cache;
    const auto cached = cache.constFind(name);
    if (cached != cache.constEnd())
        return cached.value();

    QIcon icon;
    const int sizes[] = {16, 24, 32};
    for (int size : sizes) {
        const QString path = QStringLiteral(":/icons/metadata/%1_%2.png")
                                 .arg(name)
                                 .arg(size);
        if (QFile::exists(path))
            icon.addFile(path, QSize(size, size));
    }
    if (icon.isNull())
        icon = QIcon(QStringLiteral(":/icons/metadata/common_16.png"));

    cache.insert(name, icon);
    return icon;
}

// Имя иконки типа объекта метаданных (набор metadata-icons, MIT).
QString categoryIconName(const QString &guid)
{
    static const QHash<QString, QString> map = [] {
        QHash<QString, QString> m;
        const auto add = [&m](const char *categoryGuid, const char *icon) {
            m.insert(QString::fromLatin1(categoryGuid), QString::fromLatin1(icon));
        };
        add("0195e80c-b157-11d4-9435-004095e12fc7", "constant");          // Константы
        add("cf4abea6-37b2-11d4-940f-008048da11f9", "catalog");           // Справочники
        add("061d872a-5787-460e-95ac-ed74ea3a3e84", "document");          // Документы
        add("36a8e346-9aaa-4af9-bdbd-83be3c177977", "documentNumerator"); // Нумераторы
        add("4612bd75-71b7-4a5c-8cc5-2b0b65f9fa0d", "documentJournal");   // Журналы документов
        add("f6a80749-5ad7-400b-8519-39dc5dff2542", "enum");              // Перечисления
        add("631b75a0-29e2-11d6-a3c7-0050bae0a776", "report");            // Отчеты
        add("bf845118-327b-4682-b5c6-285d2a0eb296", "dataProcessor");     // Обработки
        add("857c4a91-e5f4-4fac-86ec-787626f1c108", "exchangePlan");      // Планы обмена
        add("238e7e88-3c5f-48b2-8a3b-81ebbecb20ed", "chartsOfAccount");   // Планы счетов
        add("82a1b659-b220-4d94-a9bd-14d757b95a48", "chartsOfCharacteristicType");
        add("30b100d6-b29f-47ac-aec7-cb8ca8a54767", "chartsOfCalculationType");
        add("13134201-f60b-11d5-a3c7-0050bae0a776", "informationRegister");
        add("b64d9a40-1642-11d6-a3c7-0050bae0a776", "accumulationRegister");
        add("2deed9b8-0056-4ffe-a473-c20a6c32a0bc", "accountingRegister");
        add("f2de87a8-64e5-45eb-a22d-b3aedab050e7", "calculationRegister");
        add("bc587f20-35d9-11d6-a3c7-0050bae0a776", "sequence");          // Последовательности
        add("fcd3404e-1523-48ce-9bc0-ecdb822684a1", "businessProcess");   // Бизнес-процессы
        add("3e63355c-1378-4953-be9b-1deb5fb6bec5", "task");              // Задачи
        add("5274d9fc-9c3a-4a71-8f5e-a0db8ab23de5", "externalDataSource");
        add("0fe48980-252d-11d6-a3c7-0050bae0a776", "commonModule");      // Общие модули
        add("07ee8426-87f1-11d5-b99c-0050bae0a95d", "form");              // Общие формы
        add("0c89c792-16c3-11d5-b96b-0050bae0a95d", "template");          // Общие макеты
        add("7dcd43d9-aca5-4926-b549-1842e6a4e8cf", "picture");           // Общие картинки
        add("2f1a5187-fb0e-4b05-9489-dc5dd6412348", "command");           // Общие команды
        add("1c57eabe-7349-44b3-b1de-ebfeab67b47d", "command");           // Группы команд
        add("15794563-ccec-41f6-a83c-ec5f7b9a5bc1", "attribute");         // Общие реквизиты
        add("09736b02-9cac-4e3f-b4f7-d3e9576ab948", "role");              // Роли
        add("37f2fa9a-b276-11d4-9435-004095e12fc7", "subsystem");         // Подсистемы
        add("24c43748-c938-45d0-8d14-01424a72b11e", "sessionParameter");
        add("af547940-3268-434f-a3e7-e47d6d2638c3", "parameter");         // Функциональные опции
        add("30d554db-541e-4f62-8970-a1c6dcfeb2bc", "parameter");
        add("c045099e-13b9-4fb6-9d50-fca00202971e", "parameter");         // Определяемые типы
        add("3e7bfcc0-067d-11d6-a3c7-0050bae0a776", "filterCriteria");
        add("4e828da6-0f44-4b5b-b1c0-a2b3cfe7bdcc", "eventSubscription");
        add("11bdaf85-d5ad-4d91-bb24-aa0eee139052", "scheduledJob");
        add("46b4cd97-fd13-4eaa-aba2-3bddd7699218", "settingsStorage");   // Хранилища настроек
        add("3e5404af-6ef8-4c73-ad11-91bd2dfac4c8", "style");             // Стили
        add("58848766-36ea-4076-8800-e91eb49590d7", "style");             // Элементы стиля
        add("9cd510ce-abfc-11d4-9434-004095e12fc7", "language");          // Языки
        add("39bddf6a-0c3c-452b-921c-d99cfa1c2f1b", "interfaces");        // Интерфейсы
        add("0fffc09c-8f4c-47cc-b41c-8d5c5a221d79", "http");              // HTTP-сервисы
        add("8657032e-7740-4e1d-a3ba-5dd6e8afb78f", "ws");                // Web-сервисы
        add("d26096fb-7a5d-4df9-af63-47d04771fa9b", "wsLink");            // WS-ссылки
        add("cc9df798-7c94-4616-97d2-7aa0b7bc515e", "urlTemplate");       // XDTO-пакеты
        add("6e6dc072-b7ac-41e7-8f88-278d25b6da2a", "bots");              // Боты
        add("a7641777-7813-45c6-96ef-9d51587a6ac6", "ws");                // WebSocket-клиенты
        add("bf3420b0-f6f9-41a0-b83a-fe9d4ab0b65d", "ws");                // Сервисы интеграции
        return m;
    }();

    const auto it = map.constFind(guid.toLower());
    return it != map.constEnd() ? it.value() : QStringLiteral("common");
}

QString sectionIconName(MetadataSectionKind kind)
{
    switch (kind) {
    case MetadataSectionKind::Templates:
        return QStringLiteral("template");
    case MetadataSectionKind::Commands:
        return QStringLiteral("command");
    case MetadataSectionKind::TabularSections:
        return QStringLiteral("tabularSection");
    case MetadataSectionKind::Attributes:
        return QStringLiteral("column");
    case MetadataSectionKind::Forms:
        return QStringLiteral("form");
    case MetadataSectionKind::TabularAttributes:
        return QStringLiteral("column");
    case MetadataSectionKind::Dimensions:
        return QStringLiteral("dimension");
    case MetadataSectionKind::Resources:
        return QStringLiteral("resource");
    case MetadataSectionKind::AccountingFlags:
        return QStringLiteral("accountingFlag");
    case MetadataSectionKind::SubcontoFlags:
        return QStringLiteral("extDimensionAccountingFlag");
    case MetadataSectionKind::Values:
        return QStringLiteral("enum");
    case MetadataSectionKind::Columns:
    case MetadataSectionKind::JournalColumns:
        return QStringLiteral("column");
    }
    return QStringLiteral("common");
}

QString compositionTypeLabel(const MetadataEntry &entry)
{
    if (entry.typeText.isEmpty())
        return entry.typeTitle;
    return QStringLiteral("%1 · %2").arg(entry.typeTitle, entry.typeText);
}

bool isEventSubscriptionCategory(const QString &guid)
{
    return guid.compare(QStringLiteral("4e828da6-0f44-4b5b-b1c0-a2b3cfe7bdcc"),
                        Qt::CaseInsensitive)
        == 0;
}

// «Posting_ОбработкаПроведения» → «ОбработкаПроведения» (в 1С событие
// показывается по-русски, а в метаданных хранится пара «англ._рус.»).
QString subscriptionEventDisplay(const QString &eventName)
{
    const int separator = eventName.indexOf(QLatin1Char('_'));
    if (separator <= 0)
        return eventName;
    const QString prefix = eventName.left(separator);
    for (const QChar ch : prefix) {
        if (!ch.isLetter() || ch.unicode() > 127)
            return eventName;
    }
    return eventName.mid(separator + 1);
}

// Порядок стандартных веток конфигурации как в 1С:Предприятие 8.3.
const QString &documentsCategoryGuid()
{
    static const QString guid = QStringLiteral("061d872a-5787-460e-95ac-ed74ea3a3e84");
    return guid;
}

const QString &numeratorsCategoryGuid()
{
    static const QString guid = QStringLiteral("36a8e346-9aaa-4af9-bdbd-83be3c177977");
    return guid;
}

const QString &sequencesCategoryGuid()
{
    static const QString guid = QStringLiteral("bc587f20-35d9-11d6-a3c7-0050bae0a776");
    return guid;
}

const QStringList &canonicalTopLevelOrder()
{
    static const QStringList order = {
        QStringLiteral("0195e80c-b157-11d4-9435-004095e12fc7"), // Константы
        QStringLiteral("cf4abea6-37b2-11d4-940f-008048da11f9"), // Справочники
        QStringLiteral("061d872a-5787-460e-95ac-ed74ea3a3e84"), // Документы
        QStringLiteral("4612bd75-71b7-4a5c-8cc5-2b0b65f9fa0d"), // Журналы документов
        QStringLiteral("f6a80749-5ad7-400b-8519-39dc5dff2542"), // Перечисления
        QStringLiteral("631b75a0-29e2-11d6-a3c7-0050bae0a776"), // Отчеты
        QStringLiteral("bf845118-327b-4682-b5c6-285d2a0eb296"), // Обработки
        QStringLiteral("82a1b659-b220-4d94-a9bd-14d757b95a48"), // Планы видов характеристик
        QStringLiteral("238e7e88-3c5f-48b2-8a3b-81ebbecb20ed"), // Планы счетов
        QStringLiteral("30b100d6-b29f-47ac-aec7-cb8ca8a54767"), // Планы видов расчета
        QStringLiteral("13134201-f60b-11d5-a3c7-0050bae0a776"), // Регистры сведений
        QStringLiteral("b64d9a40-1642-11d6-a3c7-0050bae0a776"), // Регистры накопления
        QStringLiteral("2deed9b8-0056-4ffe-a473-c20a6c32a0bc"), // Регистры бухгалтерии
        QStringLiteral("f2de87a8-64e5-45eb-a22d-b3aedab050e7"), // Регистры расчета
        QStringLiteral("fcd3404e-1523-48ce-9bc0-ecdb822684a1"), // Бизнес-процессы
        QStringLiteral("3e63355c-1378-4953-be9b-1deb5fb6bec5"), // Задачи
        QStringLiteral("5274d9fc-9c3a-4a71-8f5e-a0db8ab23de5"), // Внешние источники данных
    };
    return order;
}

// Ветки, которые в 1С показываются даже при отсутствии объектов.
const QStringList &standardVisibleBranches()
{
    return canonicalTopLevelOrder();
}

// Порядок веток внутри группы «Общие» (как в конфигураторе 1С 8.3:
// Руководство разработчика, гл. 5.5 «Ветвь конфигурации "Общие"»).
const QStringList &canonicalCommonOrder()
{
    static const QStringList order = {
        QStringLiteral("37f2fa9a-b276-11d4-9435-004095e12fc7"), // Подсистемы
        QStringLiteral("0fe48980-252d-11d6-a3c7-0050bae0a776"), // Общие модули
        QStringLiteral("24c43748-c938-45d0-8d14-01424a72b11e"), // Параметры сеанса
        QStringLiteral("09736b02-9cac-4e3f-b4f7-d3e9576ab948"), // Роли
        QStringLiteral("15794563-ccec-41f6-a83c-ec5f7b9a5bc1"), // Общие реквизиты
        QStringLiteral("857c4a91-e5f4-4fac-86ec-787626f1c108"), // Планы обмена
        QStringLiteral("3e7bfcc0-067d-11d6-a3c7-0050bae0a776"), // Критерии отбора
        QStringLiteral("4e828da6-0f44-4b5b-b1c0-a2b3cfe7bdcc"), // Подписки на события
        QStringLiteral("11bdaf85-d5ad-4d91-bb24-aa0eee139052"), // Регламентные задания
        QStringLiteral("6e6dc072-b7ac-41e7-8f88-278d25b6da2a"), // Боты
        QStringLiteral("af547940-3268-434f-a3e7-e47d6d2638c3"), // Функциональные опции
        QStringLiteral("30d554db-541e-4f62-8970-a1c6dcfeb2bc"), // Параметры функц. опций
        QStringLiteral("c045099e-13b9-4fb6-9d50-fca00202971e"), // Определяемые типы
        QStringLiteral("46b4cd97-fd13-4eaa-aba2-3bddd7699218"), // Хранилища настроек
        QStringLiteral("2f1a5187-fb0e-4b05-9489-dc5dd6412348"), // Общие команды
        QStringLiteral("1c57eabe-7349-44b3-b1de-ebfeab67b47d"), // Группы команд
        QStringLiteral("07ee8426-87f1-11d5-b99c-0050bae0a95d"), // Общие формы
        QStringLiteral("0c89c792-16c3-11d5-b96b-0050bae0a95d"), // Общие макеты
        QStringLiteral("7dcd43d9-aca5-4926-b549-1842e6a4e8cf"), // Общие картинки
        QStringLiteral("cc9df798-7c94-4616-97d2-7aa0b7bc515e"), // XDTO-пакеты
        QStringLiteral("8657032e-7740-4e1d-a3ba-5dd6e8afb78f"), // Web-сервисы
        QStringLiteral("0fffc09c-8f4c-47cc-b41c-8d5c5a221d79"), // HTTP-сервисы
        QStringLiteral("d26096fb-7a5d-4df9-af63-47d04771fa9b"), // WS-ссылки
        QStringLiteral("a7641777-7813-45c6-96ef-9d51587a6ac6"), // WebSocket-клиенты
        QStringLiteral("bf3420b0-f6f9-41a0-b83a-fe9d4ab0b65d"), // Сервисы интеграции
        QStringLiteral("58848766-36ea-4076-8800-e91eb49590d7"), // Элементы стиля
        QStringLiteral("3e5404af-6ef8-4c73-ad11-91bd2dfac4c8"), // Стили
        QStringLiteral("9cd510ce-abfc-11d4-9434-004095e12fc7"), // Языки
        QStringLiteral("39bddf6a-0c3c-452b-921c-d99cfa1c2f1b"), // Интерфейсы (устар.)
    };
    return order;
}

// Ветки группы «Общие», которые в 1С показываются даже при отсутствии объектов.
const QStringList &standardCommonVisibleBranches()
{
    static const QStringList branches = [] {
        QStringList list = canonicalCommonOrder();
        // «Интерфейсы» — объект старых версий, в 8.3 в дереве не показывается.
        list.removeAll(QStringLiteral("39bddf6a-0c3c-452b-921c-d99cfa1c2f1b"));
        return list;
    }();
    return branches;
}

bool isStandardVisibleBranch(const QString &guid)
{
    return standardVisibleBranches().contains(guid.toLower());
}

bool isCanonicalTopLevel(const QString &guid)
{
    return canonicalTopLevelOrder().contains(guid.toLower());
}

bool isSubsystemCategory(const QString &guid)
{
    return guid.compare(QStringLiteral("37f2fa9a-b276-11d4-9435-004095e12fc7"),
                        Qt::CaseInsensitive) == 0;
}

// Категории, которые в дереве 1С собраны в группу «Общие».
bool isCommonCategoryGuid(const QString &guid)
{
    static const QSet<QString> guids = [] {
        QSet<QString> set;
        for (const QString &value : canonicalCommonOrder())
            set.insert(value);
        return set;
    }();
    return guids.contains(guid.toLower());
}
} // namespace

QPalette darkPalette()
{    QPalette palette;
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_defaultPalette = QApplication::palette();
    m_defaultStyleName = QApplication::style()->objectName();

    m_rootIcon = QIcon();
    for (int size : {16, 24, 32, 48}) {
        const QString path = QStringLiteral(":/icons/app/qtv8reader_%1.png").arg(size);
        if (QFile::exists(path))
            m_rootIcon.addFile(path, QSize(size, size));
    }
    if (m_rootIcon.isNull())
        m_rootIcon = metadataIcon(QStringLiteral("root"));
    setWindowIcon(m_rootIcon);
    m_groupIcon = metadataIcon(QStringLiteral("group"));
    m_categoryIcon = metadataIcon(QStringLiteral("common"));
    m_sectionIcon = metadataIcon(QStringLiteral("tabularSection"));
    m_entryIcon = metadataIcon(QStringLiteral("column"));
    m_moduleIcon = metadataIcon(QStringLiteral("editObject"));

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
    loadRecentFiles();
    updateRecentMenu();
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
    m_highlighter = new BslHighlighter(m_moduleEdit->document());
    m_highlighter->setEnabled(m_highlightEnabled);
    m_modulePage = modulePage;
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

    m_formPage = new QWidget();
    auto *formLayout = new QVBoxLayout(m_formPage);
    formLayout->setContentsMargins(0, 0, 0, 0);
    m_formView = new FormView(m_formPage);
    m_formView->setObjectName(QStringLiteral("formView"));
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

void MainWindow::setModuleHighlighting(bool enabled)
{
    m_highlightEnabled = enabled;
    if (m_highlighter)
        m_highlighter->setEnabled(enabled);
    QSettings settings;
    settings.setValue(settingsKey(QStringLiteral("moduleHighlight")), enabled);
}

void MainWindow::applyTheme(const QString &theme, bool persist)
{
    const bool dark =
        theme.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0;
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
    if (m_lightThemeAction)
        m_lightThemeAction->setChecked(!dark);
    if (m_darkThemeAction)
        m_darkThemeAction->setChecked(dark);

    if (persist) {
        QSettings settings;
        settings.setValue(settingsKey(QStringLiteral("theme")), m_theme);
    }
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

void MainWindow::buildTree()
{
    m_tree->clear();
    if (!m_reader)
        return;

    Configuration &configuration = m_reader->configuration();
    m_tree->setUpdatesEnabled(false);

    // Корень: в 1С показывается имя конфигурации без служебных пометок.
    auto *rootItem = new QTreeWidgetItem(m_tree);
    const bool isConfiguration =
        configuration.kind == ConfigurationKind::Configuration
        || configuration.kind == ConfigurationKind::Extension;
    const QString rootText = configuration.kind == ConfigurationKind::Configuration
        ? configuration.displayName()
        : QStringLiteral("%1 (%2)").arg(configuration.displayName(),
                                        configuration.kindTitle);
    rootItem->setText(0, rootText);
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

    if (isConfiguration)
        addRootModuleItems(rootItem);

    QHash<QString, const MetadataCategory *> byGuid;
    for (MetadataCategory &category : configuration.categories)
        byGuid.insert(category.guid.toLower(), &category);

    const bool showEmptyBranches = isConfiguration;

    // Группа «Общие»: показываем стандартные общие ветки, как в 1C.
    QVector<const MetadataCategory *> commonPresent;
    for (const QString &guid : canonicalCommonOrder()) {
        if (const MetadataCategory *category = byGuid.value(guid.toLower()))
            commonPresent.append(category);
    }

    if (showEmptyBranches || !commonPresent.isEmpty()) {
        auto *groupItem = new QTreeWidgetItem(rootItem);
        groupItem->setText(0, QStringLiteral("Общие"));
        groupItem->setIcon(0, m_groupIcon);
        groupItem->setData(0, KindRole, GroupItem);
        groupItem->setData(0, GuidRole, QStringLiteral("__common__"));
        groupItem->setToolTip(0, QStringLiteral("Общие объекты конфигурации"));
        QFont groupFont = groupItem->font(0);
        groupFont.setBold(true);
        groupItem->setFont(0, groupFont);

        for (const QString &guid : canonicalCommonOrder()) {
            if (const MetadataCategory *category = byGuid.value(guid.toLower())) {
                addCategoryItem(groupItem, *category);
            } else if (showEmptyBranches
                       && standardCommonVisibleBranches().contains(guid)) {
                addPlaceholderCategoryItem(groupItem, guid);
            }
        }
    }

    // Остальные стандартные ветки в порядке 1C.
    QTreeWidgetItem *documentsItem = nullptr;
    for (const QString &guid : canonicalTopLevelOrder()) {
        if (const MetadataCategory *category = byGuid.value(guid.toLower())) {
            QTreeWidgetItem *categoryItem = addCategoryItem(rootItem, *category);
            if (guid.compare(documentsCategoryGuid(), Qt::CaseInsensitive) == 0)
                documentsItem = categoryItem;
        } else if (showEmptyBranches && isStandardVisibleBranch(guid)) {
            addPlaceholderCategoryItem(rootItem, guid);
        }
    }

    // Нумераторы и последовательности — первые подветки «Документов», как в 1C.
    QSet<QString> movedCategories;
    if (documentsItem) {
        int insertIndex = 0;
        for (const QString &guid : {numeratorsCategoryGuid(), sequencesCategoryGuid()}) {
            if (const MetadataCategory *category = byGuid.value(guid.toLower())) {
                QTreeWidgetItem *subItem = addCategoryItem(documentsItem, *category);
                subItem = documentsItem->takeChild(documentsItem->indexOfChild(subItem));
                documentsItem->insertChild(insertIndex++, subItem);
                movedCategories.insert(guid.toLower());
            }
        }
    }

    // Нестандартные/дополнительные категории — в конце, в порядке файла.
    for (const MetadataCategory &category : configuration.categories) {
        if (isCommonCategoryGuid(category.guid)
            || isCanonicalTopLevel(category.guid)
            || movedCategories.contains(category.guid.toLower())) {
            continue;
        }
        addCategoryItem(rootItem, category);
    }

    // По умолчанию дерево выглядит как в 1С: раскрыт только корень.
    rootItem->setExpanded(true);
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

void MainWindow::addRootModuleItems(QTreeWidgetItem *rootItem)
{
    for (int i = 0; i < m_rootModules.size(); ++i) {
        const ModuleInfo &module = m_rootModules.at(i);
        auto *moduleItem = new QTreeWidgetItem(rootItem);
        moduleItem->setText(0, module.name);
        moduleItem->setIcon(0, metadataIcon(module.name.contains(QStringLiteral("сеанса"))
                                                ? QStringLiteral("editManager")
                                                : QStringLiteral("editObject")));
        moduleItem->setData(0, KindRole, RootModuleItem);
        moduleItem->setData(0, ModuleIndexRole, i);
        moduleItem->setToolTip(0, QStringLiteral("%1\nЭлемент: %2\nСимволов: %3")
                                      .arg(module.name, module.elementName)
                                      .arg(module.text.size()));
    }
}

QTreeWidgetItem *MainWindow::addCategoryItem(QTreeWidgetItem *parent,
                                             const MetadataCategory &category)
{
    auto *categoryItem = new QTreeWidgetItem(parent);
    categoryItem->setText(0, category.title);
    categoryItem->setIcon(0, metadataIcon(categoryIconName(category.guid)));
    categoryItem->setData(0, KindRole, CategoryItem);
    categoryItem->setData(0, GuidRole, category.guid);
    categoryItem->setToolTip(0, QStringLiteral("%1\nТип объекта: %2\nGUID: %3\nОбъектов: %4")
                                    .arg(category.title, category.typeTitle, category.guid)
                                    .arg(category.objects.size()));
    QFont font = categoryItem->font(0);
    font.setBold(true);
    categoryItem->setFont(0, font);

    if (isSubsystemCategory(category.guid) && m_reader) {
        MetadataCategory *mutableCategory =
            m_reader->configuration().category(category.guid);
        if (mutableCategory)
            buildSubsystemTree(categoryItem, *mutableCategory);
    } else {
        for (const MetadataObject &object : category.objects)
            addObjectItem(categoryItem, object);
    }

    return categoryItem;
}

void MainWindow::buildSubsystemTree(QTreeWidgetItem *parent, MetadataCategory &category)
{
    if (!m_reader)
        return;

    // Вложенные подсистемы могут не входить в общий список категории,
    // поэтому сначала собираем все GUID, упомянутые как дочерние.
    QSet<QString> childGuids;
    for (MetadataObject &object : category.objects) {
        m_reader->loadSubsystemChildren(object);
        for (const QString &guid : object.childGuids)
            childGuids.insert(guid.toLower());
    }

    for (MetadataObject &object : category.objects) {
        if (childGuids.contains(object.guid.toLower()))
            continue;
        addSubsystemItem(parent, object);
    }
}

QTreeWidgetItem *MainWindow::addSubsystemItem(QTreeWidgetItem *parent,
                                              const MetadataObject &object,
                                              int depth)
{
    if (depth > 64)
        return nullptr;
    auto *item = new QTreeWidgetItem(parent);
    item->setText(0, object.displayName());
    item->setIcon(0, metadataIcon(categoryIconName(object.categoryGuid)));
    item->setData(0, KindRole, ObjectItem);
    item->setData(0, GuidRole, object.guid);
    item->setData(0, LoadedRole, true);

    QString tooltip = QStringLiteral("Подсистема: %1\nGUID: %2")
                          .arg(object.displayName(), object.guid);
    if (!object.synonym.isEmpty())
        tooltip += QStringLiteral("\nСиноним: %1").arg(object.synonym);
    if (!object.comment.isEmpty())
        tooltip += QStringLiteral("\nКомментарий: %1").arg(object.comment);
    item->setToolTip(0, tooltip);

    MetadataObject *mutableObject = m_reader->configuration().object(object.guid);
    if (!mutableObject)
        mutableObject = m_reader->cachedSubsystem(object.guid);
    if (!mutableObject)
        return item;

    m_reader->loadSubsystemChildren(*mutableObject);
    for (const QString &childGuid : mutableObject->childGuids) {
        MetadataObject *child = m_reader->subsystemObject(childGuid);
        if (!child)
            continue;
        addSubsystemItem(item, *child, depth + 1);
    }
    return item;
}

void MainWindow::addSubsystemCompositionItems(QTreeWidgetItem *parent,
                                              const QStringList &guids,
                                              int depth)
{
    if (depth > 64)
        return;
    for (const QString &guid : guids) {
        MetadataObject *child = m_reader->subsystemObject(guid);
        if (!child)
            continue;

        auto *item = new QTreeWidgetItem(parent);
        item->setText(0, QStringLiteral("Подсистема"));
        item->setText(1, child->displayName());
        item->setText(2, child->synonym);
        item->setText(3, child->guid);
        item->setText(4, child->comment);
        item->setIcon(0, metadataIcon(categoryIconName(child->categoryGuid)));

        m_reader->loadSubsystemChildren(*child);
        if (!child->childGuids.isEmpty())
            addSubsystemCompositionItems(item, child->childGuids, depth + 1);
    }
}
QTreeWidgetItem *MainWindow::addPlaceholderCategoryItem(QTreeWidgetItem *parent,
                                                        const QString &guid)
{
    const MetadataCategoryInfo *info = MetadataCatalog::category(guid);
    const QString title = info ? info->title : guid;

    auto *categoryItem = new QTreeWidgetItem(parent);
    categoryItem->setText(0, title);
    categoryItem->setIcon(0, metadataIcon(categoryIconName(guid)));
    categoryItem->setData(0, KindRole, CategoryItem);
    categoryItem->setData(0, GuidRole, guid);
    categoryItem->setToolTip(0, QStringLiteral("%1\nGUID: %2\nОбъектов: 0")
                                    .arg(title, guid));
    QFont font = categoryItem->font(0);
    font.setBold(true);
    categoryItem->setFont(0, font);
    return categoryItem;
}

QTreeWidgetItem *MainWindow::addObjectItem(QTreeWidgetItem *parent,
                                           const MetadataObject &object)
{
    auto *objectItem = new QTreeWidgetItem(parent);
    objectItem->setText(0, object.displayName());
    objectItem->setIcon(0, metadataIcon(categoryIconName(object.categoryGuid)));
    objectItem->setData(0, KindRole, ObjectItem);
    objectItem->setData(0, GuidRole, object.guid);
    objectItem->setData(0, LoadedRole, false);

    QString tooltip = QStringLiteral("%1: %2\nGUID: %3")
                          .arg(object.typeTitle, object.displayName(), object.guid);
    if (!object.typeText.isEmpty())
        tooltip += QStringLiteral("\nТип: %1").arg(object.typeText);
    if (!object.eventName.isEmpty())
        tooltip += QStringLiteral("\nСобытие: %1")
                       .arg(subscriptionEventDisplay(object.eventName));
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

    if (isSubsystemCategory(object.categoryGuid))
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
        const bool isManager = module.name.contains(QStringLiteral("менеджера"),
                                                    Qt::CaseInsensitive);
        const bool isCommand = module.name.contains(QStringLiteral("команды"),
                                                    Qt::CaseInsensitive);
        moduleItem->setIcon(0,
                            metadataIcon(isManager   ? QStringLiteral("editManager")
                                          : isCommand ? QStringLiteral("command")
                                                      : QStringLiteral("editObject")));
        moduleItem->setData(0, KindRole, ModuleNodeItem);
        moduleItem->setData(0, GuidRole, object.guid);
        moduleItem->setData(0, ModuleIndexRole, i);
        moduleItem->setToolTip(0, QStringLiteral("%1\nЭлемент: %2")
                                    .arg(module.name, module.elementName));
    }

    for (const MetadataSection &section : object.sections) {
        auto *sectionItem = new QTreeWidgetItem(item);
        sectionItem->setText(0,
                             section.entries.isEmpty()
                                 ? section.title
                                 : QStringLiteral("%1 (%2)")
                                       .arg(section.title)
                                       .arg(section.entries.size()));
        sectionItem->setIcon(0, metadataIcon(sectionIconName(section.kind)));
        sectionItem->setData(0, KindRole, SectionItem);
        sectionItem->setData(0, GuidRole, object.guid);
        sectionItem->setData(0, SectionKindRole, static_cast<int>(section.kind));
        QString sectionTooltip = QStringLiteral("%1\nGUID раздела: %2")
                                     .arg(section.title, section.guid);
        if (section.entries.isEmpty())
            sectionTooltip += QStringLiteral("\nРаздел пуст");
        sectionItem->setToolTip(0, sectionTooltip);
        QFont sectionFont = sectionItem->font(0);
        sectionFont.setBold(true);
        sectionItem->setFont(0, sectionFont);

        for (int i = 0; i < section.entries.size(); ++i) {
            const MetadataEntry &entry = section.entries.at(i);
            auto *entryItem = new QTreeWidgetItem(sectionItem);
            entryItem->setText(0, entry.displayName());
            entryItem->setIcon(0, metadataIcon(sectionIconName(section.kind)));
            entryItem->setData(0, KindRole, EntryItem);
            entryItem->setData(0, GuidRole, object.guid);
            entryItem->setData(0, SectionKindRole, static_cast<int>(section.kind));
            entryItem->setData(0, EntryIndexRole, i);
            entryItem->setData(0, ChildIndexRole, -1);

            QString entryTooltip = QStringLiteral("%1: %2")
                                       .arg(entry.typeTitle, entry.displayName());
            if (!entry.typeText.isEmpty())
                entryTooltip += QStringLiteral("\nТип: %1").arg(entry.typeText);
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
                childItem->setIcon(0, metadataIcon(QStringLiteral("column")));
                childItem->setData(0, KindRole, EntryItem);
                childItem->setData(0, GuidRole, object.guid);
                childItem->setData(0, SectionKindRole,
                                   static_cast<int>(MetadataSectionKind::TabularAttributes));
                childItem->setData(0, EntryIndexRole, i);
                childItem->setData(0, ChildIndexRole, j);
                QString childTooltip = QStringLiteral("%1: %2")
                                           .arg(child.typeTitle, child.displayName());
                if (!child.typeText.isEmpty())
                    childTooltip += QStringLiteral("\nТип: %1").arg(child.typeText);
                childTooltip += QStringLiteral("\nGUID: %1").arg(child.guid);
                childItem->setToolTip(0, childTooltip);
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

MetadataObject *MainWindow::objectForItem(const QTreeWidgetItem *item)
{
    if (!item || !m_reader)
        return nullptr;
    const int kind = item->data(0, KindRole).toInt();
    if (kind != ObjectItem && kind != SectionItem && kind != EntryItem
        && kind != ModuleNodeItem) {
        return nullptr;
    }
    const QString guid = item->data(0, GuidRole).toString();
    if (MetadataObject *object = m_reader->configuration().object(guid))
        return object;
    return m_reader->cachedSubsystem(guid);
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
        || kind == ObjectItem || kind == SectionItem || kind == EntryItem
        || kind == RootModuleItem) {
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
    if (m_mxlView)
        m_mxlView->clear();
    m_mxlDocument = mxl::Document();
    m_mxlDocumentValid = false;
    updateMxlActions();
    if (m_mxlPage) {
        const int index = m_tabs->indexOf(m_mxlPage);
        if (index >= 0)
            m_tabs->setTabText(index, QStringLiteral("Табличный документ"));
    }
    if (m_formView)
        m_formView->clear();
    m_formDocument = FormDocument();
    m_formDocumentValid = false;
    if (m_formPage) {
        const int index = m_tabs->indexOf(m_formPage);
        if (index >= 0)
            m_tabs->setTabText(index, QStringLiteral("Форма"));
    }
}

void MainWindow::setProperties(const QVector<QPair<QString, QString>> &rows)
{
    m_properties->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QPair<QString, QString> &row = rows.at(i);
        m_properties->setItem(i, 0, readOnlyItem(row.first));
        auto *valueItem = readOnlyItem(row.second);
        if (!row.second.isEmpty())
            valueItem->setToolTip(row.second);
        m_properties->setItem(i, 1, valueItem);
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
    root->setIcon(0, m_groupIcon);
    for (const MetadataObject &object : category.objects) {
        auto *item = new QTreeWidgetItem(root);
        item->setText(0, object.typeTitle);
        item->setText(1, object.displayName());
        item->setText(2, object.synonym);
        item->setText(3, object.guid);
        item->setText(4, object.comment);
        item->setIcon(0, metadataIcon(categoryIconName(category.guid)));
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

void MainWindow::showPlaceholderCategory(const QString &guid)
{
    clearObjectView();

    const MetadataCategoryInfo *info = MetadataCatalog::category(guid);
    const QString title = info ? info->title : guid;
    const QString typeTitle = info ? info->typeTitle : QString();

    QVector<QPair<QString, QString>> rows;
    rows.append({QStringLiteral("Категория"), title});
    rows.append({QStringLiteral("Тип объекта"), typeTitle});
    rows.append({QStringLiteral("GUID категории"), guid});
    rows.append({QStringLiteral("Объектов"), QStringLiteral("0")});
    rows.append({QStringLiteral("Описание"),
                 QStringLiteral("В конфигурации нет объектов этого вида.")});
    setProperties(rows);

    m_composition->clear();
    m_metadataEdit->setPlainText(
        QStringLiteral("Категория «%1» пуста.").arg(title));
    m_moduleEdit->setPlainText(QStringLiteral("Выберите объект метаданных."));

    m_extractAction->setEnabled(false);
    statusBar()->showMessage(QStringLiteral("Категория: %1 (0 объектов)").arg(title));
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
    rows.append({QStringLiteral("Вид"), object.typeTitle});
    rows.append({QStringLiteral("Категория"), object.categoryTitle});
    rows.append({QStringLiteral("Имя"), object.name});
    if (!object.typeText.isEmpty())
        rows.append({QStringLiteral("Тип"), object.typeText});
    if (isEventSubscriptionCategory(object.categoryGuid)) {
        QStringList sourceNames;
        for (const QString &typeGuid : object.sourceTypeGuids) {
            const QString name = m_reader->typeNameForGuid(typeGuid);
            sourceNames << (name.isEmpty() ? typeGuid : name);
        }
        rows.append({QStringLiteral("Источник"),
                     sourceNames.join(QStringLiteral(", "))});
        rows.append({QStringLiteral("Событие"),
                     subscriptionEventDisplay(object.eventName)});
        QString handler = object.handlerProcedure;
        if (!object.handlerModuleGuid.isEmpty()) {
            QString moduleName;
            if (m_reader->readReferenceName(object.handlerModuleGuid, &moduleName,
                                            nullptr, nullptr)
                && !moduleName.isEmpty()) {
                handler = QStringLiteral("%1.%2").arg(moduleName, handler);
            }
        }
        rows.append({QStringLiteral("Обработчик"), handler});
    }
    rows.append({QStringLiteral("Синоним"), object.synonym});
    rows.append({QStringLiteral("Комментарий"), object.comment});
    rows.append({QStringLiteral("GUID"), object.guid});
    rows.append({QStringLiteral("Элемент контейнера"), object.elementName});
    rows.append({QStringLiteral("Элемент найден"),
                 object.elementFound ? QStringLiteral("да") : QStringLiteral("нет")});
    rows.append({QStringLiteral("Описание найдено"),
                 object.descriptorFound ? QStringLiteral("да") : QStringLiteral("нет")});
    rows.append({QStringLiteral("Модулей"), QString::number(m_currentModules.size())});

    if (isSubsystemCategory(object.categoryGuid)) {
        MetadataObject *mutableObject = m_reader->configuration().object(object.guid);
        if (!mutableObject)
            mutableObject = m_reader->cachedSubsystem(object.guid);
        if (mutableObject) {
            m_reader->loadSubsystemChildren(*mutableObject);
            rows.append({QStringLiteral("Вложенных подсистем"),
                         QString::number(mutableObject->childGuids.size())});
        }
    }

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
        categoryItem->setIcon(0, metadataIcon(categoryIconName(category->guid)));
        for (const MetadataObject &object : category->objects) {
            auto *objectItem = new QTreeWidgetItem(categoryItem);
            objectItem->setText(0, object.typeTitle);
            objectItem->setText(1, object.displayName());
            objectItem->setText(2, object.synonym);
            objectItem->setText(3, object.guid);
            objectItem->setText(4, object.comment);
            objectItem->setIcon(0, metadataIcon(categoryIconName(category->guid)));
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

    if (kind == MetadataSectionKind::Templates)
        loadTemplatePreview(*entry);
    if (kind == MetadataSectionKind::Forms)
        loadFormPreview(*entry);

    QVector<QPair<QString, QString>> rows;
    rows.append({QStringLiteral("Вид"), entry->typeTitle});
    rows.append({QStringLiteral("Имя"), entry->name});
    if (!entry->typeText.isEmpty())
        rows.append({QStringLiteral("Тип"), entry->typeText});
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
    if (kind == MetadataSectionKind::Templates) {
        rows.append({QStringLiteral("Именованные области"),
                     QString::number(m_mxlDocument.namedAreas.size())});
        if (!m_mxlDocument.namedAreas.isEmpty()) {
            QStringList names;
            for (const mxl::NamedArea &area : m_mxlDocument.namedAreas)
                names << area.name;
            rows.append({QStringLiteral("Области"), names.join(QStringLiteral(", "))});
        }
        if (m_mxlDocumentValid)
            rows.append({QStringLiteral("Печать"), mxlPrintSummary()});
    }
    if (kind == MetadataSectionKind::Forms) {
        if (m_formDocumentValid) {
            rows.append({QStringLiteral("Элементов формы"),
                         QString::number(m_formDocument.itemCount())});
            rows.append({QStringLiteral("Полей"),
                         QString::number(m_formDocument.fieldCount())});
            rows.append({QStringLiteral("Атрибутов формы"),
                         QString::number(m_formDocument.attributes.size())});
            rows.append({QStringLiteral("Команд формы"),
                         QString::number(m_formDocument.commands.size())});
            rows.append({QStringLiteral("Модуль формы"),
                         QStringLiteral("%1 символов")
                             .arg(m_formDocument.moduleText.size())});
        } else {
            rows.append({QStringLiteral("Форма"),
                         QStringLiteral("не удалось разобрать")});
        }
    }
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

void MainWindow::loadTemplatePreview(const MetadataEntry &entry)
{
    if (!m_reader || !m_mxlView)
        return;

    const V8Container &container = m_reader->container();
    const V8Element *element = nullptr;
    if (!entry.guid.isEmpty())
        element = container.find(entry.guid + QStringLiteral(".0"));
    if (!element)
        element = container.find(entry.guid);
    if (!element && !entry.name.isEmpty())
        element = container.find(entry.name);
    if (!element && !entry.guid.isEmpty()) {
        const QString prefix = entry.guid + QLatin1Char('.');
        for (const V8Element &candidate : container.elements()) {
            if (candidate.name == entry.guid || candidate.name.startsWith(prefix)) {
                element = &candidate;
                break;
            }
        }
    }
    if (!element) {
        m_mxlView->setMessage(
            QStringLiteral("Макет «%1» не найден в контейнере.").arg(entry.displayName()));
        return;
    }

    QByteArray payload;
    QString error;
    if (!container.elementPayload(*element, &payload, nullptr, &error)) {
        m_mxlView->setMessage(
            QStringLiteral("Не удалось прочитать макет «%1»: %2")
                .arg(entry.displayName(), error));
        return;
    }

    if (!MxlReader::looksLikeMxl(payload)) {
        m_mxlView->setMessage(
            QStringLiteral("Макет «%1» не является табличным документом MXL.")
                .arg(entry.displayName()));
        return;
    }

    mxl::Document document;
    if (!MxlReader::parseBytes(payload, &document, &error)) {
        m_mxlView->setMessage(
            QStringLiteral("Не удалось разобрать макет «%1»: %2")
                .arg(entry.displayName(), error));
        return;
    }

    m_mxlView->setDocument(document);
    m_mxlDocument = document;
    m_mxlDocumentValid = true;
    updateMxlActions();
    const int index = m_tabs->indexOf(m_mxlPage);
    if (index >= 0) {
        m_tabs->setTabText(index,
                           QStringLiteral("Табличный документ (%1×%2)")
                               .arg(document.rowCount())
                               .arg(document.maxColumn() + 1));
        m_tabs->setCurrentIndex(index);
    }
}
void MainWindow::loadFormPreview(const MetadataEntry &entry)
{
    if (!m_reader || !m_formView)
        return;

    const V8Container &container = m_reader->container();
    const V8Element *element = nullptr;
    if (!entry.guid.isEmpty())
        element = container.find(entry.guid + QStringLiteral(".0"));
    if (!element)
        element = container.find(entry.guid);
    if (!element && !entry.name.isEmpty())
        element = container.find(entry.name);
    if (!element && !entry.guid.isEmpty()) {
        const QString prefix = entry.guid + QLatin1Char('.');
        for (const V8Element &candidate : container.elements()) {
            if (candidate.name == entry.guid || candidate.name.startsWith(prefix)) {
                element = &candidate;
                break;
            }
        }
    }
    if (!element) {
        m_formView->setMessage(
            QStringLiteral("Форма «%1» не найдена в контейнере.").arg(entry.displayName()));
        return;
    }

    QByteArray payload;
    QString error;
    if (!container.elementPayload(*element, &payload, nullptr, &error)) {
        m_formView->setMessage(
            QStringLiteral("Не удалось прочитать форму «%1»: %2")
                .arg(entry.displayName(), error));
        return;
    }

    FormDocument document;
    if (!FormReader::parseBytes(payload, &document, &error)) {
        m_formView->setMessage(
            QStringLiteral("Не удалось разобрать форму «%1»: %2")
                .arg(entry.displayName(), error));
        return;
    }

    document.name = entry.name;
    document.synonym = entry.synonym;
    {
        bool hasTable = false;
        std::function<bool(const QVector<FormItem> &)> findTable =
            [&](const QVector<FormItem> &items) -> bool {
                for (const FormItem &item : items) {
                    if (item.type == FormItem::Type::Table
                        && (item.name == QStringLiteral("Список")
                            || item.synonym == QStringLiteral("Список"))) {
                        return true;
                    }
                    if (findTable(item.children))
                        return true;
                }
                return false;
            };
        hasTable = findTable(document.items);
        const bool isDocument = m_currentObject
            && m_currentObject->typeTitle.contains(QStringLiteral("Документ"),
                                                   Qt::CaseInsensitive);
        document.runtimeListCommands = isDocument && hasTable;
    }
    if (m_currentObject) {
        QHash<QString, QString> objectTitles;
        for (const MetadataSection &section : m_currentObject->sections) {
            for (const MetadataEntry &sectionEntry : section.entries) {
                if (!sectionEntry.name.isEmpty() && !sectionEntry.synonym.isEmpty())
                    objectTitles.insert(sectionEntry.name, sectionEntry.synonym);
                for (const MetadataEntry &child : sectionEntry.children) {
                    if (!child.name.isEmpty() && !child.synonym.isEmpty())
                        objectTitles.insert(child.name, child.synonym);
                }
            }
        }
        for (auto it = objectTitles.constBegin(); it != objectTitles.constEnd(); ++it)
            document.titles.insert(it.key(), it.value());
    }
    m_formDocument = document;
    m_formDocumentValid = true;
    m_formView->setDocument(document);

    const int index = m_tabs->indexOf(m_formPage);
    if (index >= 0) {
        m_tabs->setTabText(index,
                           QStringLiteral("Форма (%1)").arg(document.itemCount()));
        m_tabs->setCurrentIndex(index);
    }
}

void MainWindow::updateMxlActions()
{
    if (m_printMxlAction)
        m_printMxlAction->setEnabled(m_mxlDocumentValid);
    if (m_pdfMxlAction)
        m_pdfMxlAction->setEnabled(m_mxlDocumentValid);
}

QString MainWindow::mxlPrintSummary() const
{
    if (!m_mxlDocumentValid)
        return QStringLiteral("нет макета");
    const mxl::PrintSettings &settings = m_mxlDocument.printSettings;
    QString pageSize = settings.pageSize;
    if (pageSize.isEmpty())
        pageSize = QStringLiteral("A4");
    const QString orientation = settings.orientation == 2
        ? QStringLiteral("альбомная") : QStringLiteral("книжная");
    const int scale = settings.scale > 0 ? settings.scale : 100;
    return QStringLiteral("%1, %2, масштаб %3%, поля %4/%5/%6/%7 мм")
        .arg(pageSize, orientation)
        .arg(scale)
        .arg(settings.topMarginMm)
        .arg(settings.bottomMarginMm)
        .arg(settings.leftMarginMm)
        .arg(settings.rightMarginMm);
}

void MainWindow::printMxl()
{
    if (!m_mxlDocumentValid)
        return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(QStringLiteral("MXL"));
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(QStringLiteral("Печать табличного документа"));
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = MxlPrinter::print(m_mxlDocument, printer, &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::critical(this, QStringLiteral("Ошибка печати"), error);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Документ отправлен на печать"));
}

void MainWindow::exportMxlPdf()
{
    if (!m_mxlDocumentValid)
        return;

    QString defaultPath = m_currentPath;
    if (!defaultPath.isEmpty()) {
        QFileInfo info(defaultPath);
        defaultPath = info.absolutePath() + QLatin1Char('/') + info.completeBaseName()
            + QStringLiteral("-макет.pdf");
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Экспорт макета в PDF"), defaultPath,
        QStringLiteral("PDF (*.pdf)"));
    if (path.isEmpty())
        return;
    if (!exportCurrentMxlPdf(path))
        return;
    statusBar()->showMessage(QStringLiteral("PDF сохранён: %1").arg(path));
}

bool MainWindow::exportCurrentMxlPdf(const QString &path)
{
    if (!m_mxlDocumentValid)
        return false;

    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = MxlPrinter::exportPdf(m_mxlDocument, path, &error);
    QApplication::restoreOverrideCursor();
    return ok;
}

bool MainWindow::selectFirstTemplate()
{
    if (!m_reader)
        return false;

    Configuration &configuration = m_reader->configuration();
    for (MetadataCategory &category : configuration.categories) {
        for (MetadataObject &object : category.objects) {
            if (selectTemplateForObject(object.guid))
                return true;
        }
    }
    return false;
}

bool MainWindow::selectTemplateForObject(const QString &query)
{
    if (!m_reader)
        return false;

    Configuration &configuration = m_reader->configuration();
    for (MetadataCategory &category : configuration.categories) {
        for (MetadataObject &object : category.objects) {
            if (!query.isEmpty()) {
                const bool matches = object.guid.compare(query, Qt::CaseInsensitive) == 0
                    || object.name.compare(query, Qt::CaseInsensitive) == 0
                    || object.synonym.compare(query, Qt::CaseInsensitive) == 0
                    || object.displayName().compare(query, Qt::CaseInsensitive) == 0;
                if (!matches)
                    continue;
            }

            m_reader->loadSections(object);
            const MetadataSection *section =
                object.section(MetadataSectionKind::Templates);
            if (!section || section->entries.isEmpty())
                continue;

            ensureObjectItemChildren(object.guid);
            QTreeWidgetItem *objectItem = findObjectItem(object.guid);
            if (!objectItem)
                continue;
            QTreeWidgetItem *sectionItem = nullptr;
            for (int i = 0; i < objectItem->childCount(); ++i) {
                QTreeWidgetItem *child = objectItem->child(i);
                if (child->data(0, KindRole).toInt() == SectionItem
                    && child->data(0, SectionKindRole).toInt()
                        == static_cast<int>(MetadataSectionKind::Templates)) {
                    sectionItem = child;
                    break;
                }
            }
            if (!sectionItem)
                continue;
            sectionItem->setExpanded(true);
            if (sectionItem->childCount() > 0) {
                QTreeWidgetItem *entryItem = sectionItem->child(0);
                m_tree->setCurrentItem(entryItem);
                m_tree->scrollToItem(entryItem);
                QCoreApplication::processEvents();
                return m_mxlDocumentValid;
            }
        }
    }
    return false;
}

bool MainWindow::selectFormForObject(const QString &objectQuery,
                                     const QString &formQuery)
{
    if (!m_reader)
        return false;

    Configuration &configuration = m_reader->configuration();
    for (MetadataCategory &category : configuration.categories) {
        for (MetadataObject &object : category.objects) {
            if (!objectQuery.isEmpty()) {
                const bool matches = object.guid.compare(objectQuery, Qt::CaseInsensitive) == 0
                    || object.name.compare(objectQuery, Qt::CaseInsensitive) == 0
                    || object.synonym.compare(objectQuery, Qt::CaseInsensitive) == 0
                    || object.displayName().compare(objectQuery, Qt::CaseInsensitive) == 0;
                if (!matches)
                    continue;
            }

            m_reader->loadSections(object);
            const MetadataSection *section =
                object.section(MetadataSectionKind::Forms);
            if (!section || section->entries.isEmpty())
                continue;

            int entryIndex = -1;
            for (int i = 0; i < section->entries.size(); ++i) {
                const MetadataEntry &entry = section->entries.at(i);
                if (formQuery.isEmpty()
                    || entry.name.compare(formQuery, Qt::CaseInsensitive) == 0
                    || entry.synonym.compare(formQuery, Qt::CaseInsensitive) == 0
                    || entry.displayName().compare(formQuery, Qt::CaseInsensitive) == 0) {
                    entryIndex = i;
                    break;
                }
            }
            if (entryIndex < 0)
                continue;

            ensureObjectItemChildren(object.guid);
            QTreeWidgetItem *objectItem = findObjectItem(object.guid);
            if (!objectItem)
                continue;
            QTreeWidgetItem *sectionItem = nullptr;
            for (int i = 0; i < objectItem->childCount(); ++i) {
                QTreeWidgetItem *child = objectItem->child(i);
                if (child->data(0, KindRole).toInt() == SectionItem
                    && child->data(0, SectionKindRole).toInt()
                        == static_cast<int>(MetadataSectionKind::Forms)) {
                    sectionItem = child;
                    break;
                }
            }
            if (!sectionItem)
                continue;
            sectionItem->setExpanded(true);
            if (entryIndex >= sectionItem->childCount())
                continue;
            QTreeWidgetItem *entryItem = sectionItem->child(entryIndex);
            m_tree->setCurrentItem(entryItem);
            m_tree->scrollToItem(entryItem);
            QCoreApplication::processEvents();
            return m_formDocumentValid;
        }
    }
    return false;
}

void MainWindow::setFormScale(int percent)
{
    if (!m_formView)
        return;
    m_formView->setScalePercent(percent);
}

void MainWindow::setShowNamedAreas(bool show)
{
    if (m_showAreasCheck)
        m_showAreasCheck->setChecked(show);
    if (m_mxlView)
        m_mxlView->setShowNamedAreas(show);
}

bool MainWindow::exportMxlFileToPdf(const QString &mxlPath, const QString &pdfPath)
{
    mxl::Document document;
    QString error;
    if (!MxlReader::parseFile(mxlPath, &document, &error)) {
        QMessageBox::critical(this, QStringLiteral("Ошибка разбора MXL"), error);
        return false;
    }
    m_mxlDocument = document;
    m_mxlDocumentValid = true;
    if (m_mxlView)
        m_mxlView->setDocument(document);
    updateMxlActions();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = MxlPrinter::exportPdf(document, pdfPath, &error);
    QApplication::restoreOverrideCursor();
    return ok;
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

void MainWindow::showRootModule(int index)
{
    if (!m_reader || index < 0 || index >= m_rootModules.size())
        return;

    const ModuleInfo &module = m_rootModules.at(index);
    clearObjectView();
    m_currentObject = nullptr;
    m_currentModules.clear();
    m_moduleCombo->blockSignals(true);
    m_moduleCombo->clear();
    m_moduleCombo->addItem(module.name);
    m_moduleCombo->setEnabled(false);
    m_moduleCombo->blockSignals(false);
    m_moduleEdit->setPlainText(module.text);

    QVector<QPair<QString, QString>> rows;
    rows.append({QStringLiteral("Модуль"), module.name});
    rows.append({QStringLiteral("Элемент контейнера"), module.elementName});
    rows.append({QStringLiteral("Символов"), QString::number(module.text.size())});
    setProperties(rows);

    m_metadataEdit->setPlainText(
        QStringLiteral("Корневой модуль конфигурации «%1».\n"
                       "Элемент контейнера: %2")
            .arg(module.name, module.elementName));
    const int modulePage = m_modulePage ? m_tabs->indexOf(m_modulePage) : -1;
    if (modulePage >= 0)
        m_tabs->setCurrentIndex(modulePage);
    m_extractAction->setEnabled(false);
    statusBar()->showMessage(module.name);
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
    sectionItem->setIcon(0, metadataIcon(sectionIconName(kind)));

    for (const MetadataEntry &entry : section->entries) {
        auto *entryItem = new QTreeWidgetItem(sectionItem);
        entryItem->setText(0, compositionTypeLabel(entry));
        entryItem->setText(1, entry.displayName());
        entryItem->setText(2, entry.synonym);
        entryItem->setText(3, entry.guid);
        entryItem->setText(4, entry.comment);
        entryItem->setIcon(0, metadataIcon(sectionIconName(kind)));
        for (const MetadataEntry &child : entry.children) {
            auto *childItem = new QTreeWidgetItem(entryItem);
            childItem->setText(0, compositionTypeLabel(child));
            childItem->setText(1, child.displayName());
            childItem->setText(2, child.synonym);
            childItem->setText(3, child.guid);
            childItem->setText(4, child.comment);
            childItem->setIcon(0, metadataIcon(QStringLiteral("column")));
        }
    }
    sectionItem->setExpanded(true);
}

void MainWindow::fillEntryComposition(const MetadataEntry &entry)
{
    m_composition->clear();

    auto *entryItem = new QTreeWidgetItem(m_composition);
    entryItem->setText(0, compositionTypeLabel(entry));
    entryItem->setText(1, entry.displayName());
    entryItem->setText(2, entry.synonym);
    entryItem->setText(3, entry.guid);
    entryItem->setText(4, entry.comment);
    entryItem->setIcon(0, metadataIcon(QStringLiteral("column")));

    for (const MetadataEntry &child : entry.children) {
        auto *childItem = new QTreeWidgetItem(entryItem);
        childItem->setText(0, compositionTypeLabel(child));
        childItem->setText(1, child.displayName());
        childItem->setText(2, child.synonym);
        childItem->setText(3, child.guid);
        childItem->setText(4, child.comment);
        childItem->setIcon(0, metadataIcon(QStringLiteral("column")));
    }
    entryItem->setExpanded(true);
}

void MainWindow::fillComposition(const MetadataObject &object)
{
    m_composition->clear();

    if (isSubsystemCategory(object.categoryGuid) && m_reader) {
        MetadataObject *mutableObject = m_reader->configuration().object(object.guid);
        if (!mutableObject)
            mutableObject = m_reader->cachedSubsystem(object.guid);
        if (mutableObject) {
            m_reader->loadSubsystemChildren(*mutableObject);
            auto *root = new QTreeWidgetItem(m_composition);
            root->setText(0, QStringLiteral("Подсистема"));
            root->setText(1, object.displayName());
            root->setText(3, object.guid);
            root->setIcon(0, metadataIcon(categoryIconName(object.categoryGuid)));
            addSubsystemCompositionItems(root, mutableObject->childGuids);
            root->setExpanded(true);
        }
        return;
    }

    for (const MetadataSection &section : object.sections) {
        auto *sectionItem = new QTreeWidgetItem(m_composition);
        sectionItem->setText(0, QStringLiteral("Раздел"));
        sectionItem->setText(1, QStringLiteral("%1 (%2)")
                                   .arg(section.title)
                                   .arg(section.entries.size()));
        sectionItem->setText(3, section.guid);
        sectionItem->setIcon(0, metadataIcon(sectionIconName(section.kind)));
        QFont font = sectionItem->font(1);
        font.setBold(true);
        sectionItem->setFont(1, font);

        for (const MetadataEntry &entry : section.entries) {
            auto *entryItem = new QTreeWidgetItem(sectionItem);
            entryItem->setText(0, compositionTypeLabel(entry));
            entryItem->setText(1, entry.displayName());
            entryItem->setText(2, entry.synonym);
            entryItem->setText(3, entry.guid);
            entryItem->setText(4, entry.comment);
            entryItem->setIcon(0, metadataIcon(sectionIconName(section.kind)));

            for (const MetadataEntry &child : entry.children) {
                auto *childItem = new QTreeWidgetItem(entryItem);
                childItem->setText(0, compositionTypeLabel(child));
                childItem->setText(1, child.displayName());
                childItem->setText(2, child.synonym);
                childItem->setText(3, child.guid);
                childItem->setText(4, child.comment);
                childItem->setIcon(0, metadataIcon(QStringLiteral("column")));
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
