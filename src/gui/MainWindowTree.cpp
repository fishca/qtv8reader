#include "gui/MainWindow.h"

#include "gui/MainWindowSupport.h"
#include "gui/MetadataIcons.h"
#include "core/ConfigurationReader.h"
#include "core/DisplayText.h"
#include "core/V8Text.h"

#include <QDateTime>
#include <QFileInfo>
#include <QLabel>
#include <QSignalBlocker>
#include <QLineEdit>
#include <QSet>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace v8reader {
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
        moduleItem->setIcon(0, MetadataIcons::icon(module.name.contains(QStringLiteral("сеанса"))
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
    categoryItem->setIcon(0, MetadataIcons::icon(MetadataIcons::categoryIconName(category.guid)));
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
    item->setIcon(0, MetadataIcons::icon(MetadataIcons::categoryIconName(object.categoryGuid)));
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
        item->setIcon(0, MetadataIcons::icon(MetadataIcons::categoryIconName(child->categoryGuid)));

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
    categoryItem->setIcon(0, MetadataIcons::icon(MetadataIcons::categoryIconName(guid)));
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
    objectItem->setIcon(0, MetadataIcons::icon(MetadataIcons::categoryIconName(object.categoryGuid)));
    objectItem->setData(0, KindRole, ObjectItem);
    objectItem->setData(0, GuidRole, object.guid);
    objectItem->setData(0, LoadedRole, false);

    QString tooltip = QStringLiteral("%1: %2\nGUID: %3")
                          .arg(object.typeTitle, object.displayName(), object.guid);
    if (!object.typeText.isEmpty())
        tooltip += QStringLiteral("\nТип: %1").arg(object.typeText);
    if (!object.eventName.isEmpty())
        tooltip += QStringLiteral("\nСобытие: %1")
                       .arg(eventDisplayName(object.eventName));
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
                            MetadataIcons::icon(isManager   ? QStringLiteral("editManager")
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
        sectionItem->setIcon(0, MetadataIcons::icon(MetadataIcons::sectionIconName(section.kind)));
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
            entryItem->setIcon(0, MetadataIcons::icon(MetadataIcons::sectionIconName(section.kind)));
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
                childItem->setIcon(0, MetadataIcons::icon(QStringLiteral("column")));
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
    if (!m_containerTree)
        return;

    QSignalBlocker blocker(m_containerTree);
    m_containerTree->setUpdatesEnabled(false);
    m_containerTree->clear();
    m_containerTreeDirty = false;
    if (!m_reader) {
        m_containerTree->setUpdatesEnabled(true);
        return;
    }

    const V8Container &container = m_reader->container();
    for (int i = 0; i < container.count(); ++i) {
        const V8Element *element = container.at(i);
        if (!element)
            continue;

        auto *item = new QTreeWidgetItem(m_containerTree);
        item->setText(0, element->name);
        item->setText(1, dateTimeText(element->creationDate));
        item->setText(2, dateTimeText(element->modificationDate));
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
    m_containerTree->setUpdatesEnabled(true);
}

void MainWindow::ensureContainerTree()
{
    if (m_containerTreeDirty)
        buildContainerTree();
}


} // namespace v8reader