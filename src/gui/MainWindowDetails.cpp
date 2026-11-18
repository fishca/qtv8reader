#include "gui/MainWindow.h"

#include "gui/FormView.h"

#include "gui/MainWindowSupport.h"
#include "gui/MetadataIcons.h"
#include "gui/ModulePanel.h"
#include "gui/MxlView.h"
#include "gui/PropertiesPanel.h"
#include "core/ConfigurationReader.h"
#include "core/DisplayText.h"
#include "core/MetadataCatalog.h"
#include "core/ModuleExtractor.h"
#include "core/V8Text.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace v8reader {
void MainWindow::clearObjectView()
{
    m_currentObject = nullptr;
    m_currentModules.clear();
    m_modulePanel->setPlaceholder(QString());
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
    m_properties->setRows(rows);
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
        item->setIcon(0, MetadataIcons::icon(MetadataIcons::categoryIconName(category.guid)));
    }
    root->setExpanded(true);

    m_metadataEdit->setPlainText(
        QStringLiteral("Выбрана категория «%1». Выберите объект для просмотра "
                       "метаданных и модулей.")
            .arg(category.title));
    m_modulePanel->setPlaceholder(QStringLiteral("Выберите объект метаданных."));

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
    m_modulePanel->setPlaceholder(QStringLiteral("Выберите объект метаданных."));

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
                     eventDisplayName(object.eventName)});
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
        categoryItem->setIcon(0, MetadataIcons::icon(MetadataIcons::categoryIconName(category->guid)));
        for (const MetadataObject &object : category->objects) {
            auto *objectItem = new QTreeWidgetItem(categoryItem);
            objectItem->setText(0, object.typeTitle);
            objectItem->setText(1, object.displayName());
            objectItem->setText(2, object.synonym);
            objectItem->setText(3, object.guid);
            objectItem->setText(4, object.comment);
            objectItem->setIcon(0, MetadataIcons::icon(MetadataIcons::categoryIconName(category->guid)));
        }
    }
    root->setExpanded(true);

    m_metadataEdit->setPlainText(
        QStringLiteral("Группа «%1». Выберите категорию или объект метаданных.")
            .arg(title));
    m_modulePanel->setPlaceholder(QStringLiteral("Выберите объект метаданных."));
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
                m_modulePanel->setCurrentIndex(i);
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
        m_modulePanel->setCurrentIndex(moduleIndex);
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
    m_modulePanel->showSingleModule(module);

    QVector<QPair<QString, QString>> rows;
    rows.append({QStringLiteral("Модуль"), module.name});
    rows.append({QStringLiteral("Элемент контейнера"), module.elementName});
    rows.append({QStringLiteral("Символов"), QString::number(module.text.size())});
    setProperties(rows);

    m_metadataEdit->setPlainText(
        QStringLiteral("Корневой модуль конфигурации «%1».\n"
                       "Элемент контейнера: %2")
            .arg(module.name, module.elementName));
    const int modulePage = m_modulePanel ? m_tabs->indexOf(m_modulePanel) : -1;
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
    sectionItem->setIcon(0, MetadataIcons::icon(MetadataIcons::sectionIconName(kind)));

    for (const MetadataEntry &entry : section->entries) {
        auto *entryItem = new QTreeWidgetItem(sectionItem);
        entryItem->setText(0, compositionTypeLabel(entry));
        entryItem->setText(1, entry.displayName());
        entryItem->setText(2, entry.synonym);
        entryItem->setText(3, entry.guid);
        entryItem->setText(4, entry.comment);
        entryItem->setIcon(0, MetadataIcons::icon(MetadataIcons::sectionIconName(kind)));
        for (const MetadataEntry &child : entry.children) {
            auto *childItem = new QTreeWidgetItem(entryItem);
            childItem->setText(0, compositionTypeLabel(child));
            childItem->setText(1, child.displayName());
            childItem->setText(2, child.synonym);
            childItem->setText(3, child.guid);
            childItem->setText(4, child.comment);
            childItem->setIcon(0, MetadataIcons::icon(QStringLiteral("column")));
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
    entryItem->setIcon(0, MetadataIcons::icon(QStringLiteral("column")));

    for (const MetadataEntry &child : entry.children) {
        auto *childItem = new QTreeWidgetItem(entryItem);
        childItem->setText(0, compositionTypeLabel(child));
        childItem->setText(1, child.displayName());
        childItem->setText(2, child.synonym);
        childItem->setText(3, child.guid);
        childItem->setText(4, child.comment);
        childItem->setIcon(0, MetadataIcons::icon(QStringLiteral("column")));
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
            root->setIcon(0, MetadataIcons::icon(MetadataIcons::categoryIconName(object.categoryGuid)));
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
        sectionItem->setIcon(0, MetadataIcons::icon(MetadataIcons::sectionIconName(section.kind)));
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
            entryItem->setIcon(0, MetadataIcons::icon(MetadataIcons::sectionIconName(section.kind)));

            for (const MetadataEntry &child : entry.children) {
                auto *childItem = new QTreeWidgetItem(entryItem);
                childItem->setText(0, compositionTypeLabel(child));
                childItem->setText(1, child.displayName());
                childItem->setText(2, child.synonym);
                childItem->setText(3, child.guid);
                childItem->setText(4, child.comment);
                childItem->setIcon(0, MetadataIcons::icon(QStringLiteral("column")));
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
    if (!m_moduleCache.contains(object.guid)) {
        ModuleExtractor extractor(m_reader->container());
        m_moduleCache.insert(object.guid, extractor.extractForObject(object));
    }
    m_currentModules = m_moduleCache.value(object.guid);
    m_modulePanel->setModules(m_currentModules);
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


} // namespace v8reader