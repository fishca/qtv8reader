#include "core/ConfigurationReader.h"

#include "core/SubscriptionParser.h"

#include <QFileInfo>
#include <QtConcurrent>

#include <algorithm>
#include <functional>

namespace v8reader {
namespace {

using namespace detail;

const QString &subsystemCategoryGuid()
{
    static const QString guid = QStringLiteral("37f2fa9a-b276-11d4-9435-004095e12fc7");
    return guid;
}

constexpr int kMaxTableScanDepth = 10;
ConfigurationKind kindForPath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("epf"))
        return ConfigurationKind::ExternalProcessing;
    if (suffix == QStringLiteral("erf"))
        return ConfigurationKind::ExternalReport;
    if (suffix == QStringLiteral("cfe"))
        return ConfigurationKind::Extension;
    if (suffix == QStringLiteral("cf") || suffix == QStringLiteral("cfu"))
        return ConfigurationKind::Configuration;
    return ConfigurationKind::Unknown;
}

QString kindTitleFor(ConfigurationKind kind)
{
    switch (kind) {
    case ConfigurationKind::Configuration:
        return QStringLiteral("Конфигурация");
    case ConfigurationKind::Extension:
        return QStringLiteral("Расширение конфигурации");
    case ConfigurationKind::ExternalProcessing:
        return QStringLiteral("Внешняя обработка");
    case ConfigurationKind::ExternalReport:
        return QStringLiteral("Внешний отчет");
    case ConfigurationKind::Unknown:
        break;
    }
    return QStringLiteral("Файл 1С");
}

const char *categoryGuidForKind(ConfigurationKind kind)
{
    switch (kind) {
    case ConfigurationKind::ExternalProcessing:
        return "bf845118-327b-4682-b5c6-285d2a0eb296"; // Обработки
    case ConfigurationKind::ExternalReport:
        return "631b75a0-29e2-11d6-a3c7-0050bae0a776"; // Отчеты
    default:
        return nullptr;
    }
}

QString extractVersionNumber(const V8TextNode &root)
{
    if (root.isList()) {
        const V8TextNode *format = root.child(2);
        if (format && format->isList()) {
            const V8TextNode *number = format->child(0);
            if (number && number->isInteger())
                return number->value.trimmed();
        }
    }

    qint64 maximum = -1;
    std::function<void(const V8TextNode &, int)> walk = [&](const V8TextNode &node,
                                                            int depth) {
        if (depth > 16)
            return;
        if (node.isScalar()) {
            bool ok = false;
            const qint64 value = node.toInteger(&ok);
            if (ok && value > maximum)
                maximum = value;
            return;
        }
        for (const V8TextNode &child : node.children)
            walk(child, depth + 1);
    };
    walk(root, 0);

    return maximum > 0 ? QString::number(maximum) : QString();
}

} // namespace

ConfigurationReader::ConfigurationReader()
    : m_sections({[this](const V8TextNode &node) {
                      return m_types.typeTextForItem(node);
                  },
                  [this](const QString &guid) { return referenceName(guid); }})
{
}

void ConfigurationReader::clear()
{
    m_container.clear();
    m_configuration = Configuration();
    m_referenceNames.clear();
    m_types.clear();
    m_rootDescriptorGuid.clear();
    m_subsystemObjects.clear();
    m_subsystemIndex.clear();
}

bool ConfigurationReader::open(const QString &filePath, QString *error)
{
    if (error)
        error->clear();

    clear();

    if (!m_container.open(filePath, error))
        return false;

    m_configuration.sourcePath = filePath;
    m_configuration.formatVersion = m_container.format().version;
    m_configuration.formatLabel = m_container.format().label();
    m_configuration.fileSize = m_container.size();
    m_configuration.kind = kindForPath(filePath);
    m_configuration.kindTitle = kindTitleFor(m_configuration.kind);

    QString rootGuid;
    if (!readRootGuid(&rootGuid)) {
        if (m_configuration.kind == ConfigurationKind::Unknown) {
            m_configuration.kind = ConfigurationKind::Configuration;
            m_configuration.kindTitle = kindTitleFor(m_configuration.kind);
        }
        addWarning(QStringLiteral("В контейнере нет элемента «root» — "
                                  "метаданные не найдены"));
        return true;
    }

    m_configuration.configGuid = rootGuid;
    if (m_configuration.kind == ConfigurationKind::Unknown) {
        m_configuration.kind = ConfigurationKind::Configuration;
        m_configuration.kindTitle = kindTitleFor(m_configuration.kind);
    }

    V8TextNode objectRoot;
    if (!parseElementTree(rootGuid, &objectRoot)) {
        addWarning(QStringLiteral("Не удалось прочитать объект метаданных %1")
                       .arg(rootGuid));
        return true;
    }

    if (m_configuration.kind == ConfigurationKind::ExternalProcessing
        || m_configuration.kind == ConfigurationKind::ExternalReport) {
        buildExternalObject(objectRoot, rootGuid);
        return true;
    }

    buildConfigurationMetadata(objectRoot);
    if (m_configuration.categories.isEmpty()) {
        // Возможно, это внешняя обработка с неверным расширением файла.
        const DescriptorInfo descriptor = detail::descriptorFor(objectRoot);
        if (descriptor.found && !descriptor.name.isEmpty()) {
            m_configuration.kind = ConfigurationKind::ExternalProcessing;
            m_configuration.kindTitle = kindTitleFor(m_configuration.kind);
            buildExternalObject(objectRoot, rootGuid);
            return true;
        }
    }

    const V8TextNode *descriptor = findDescriptorByGuid(objectRoot, rootGuid, 0);
    if (!descriptor)
        descriptor = findFirstDescriptor(objectRoot, 0);
    if (descriptor) {
        m_configuration.name = scalarAt(*descriptor, 2);
        m_configuration.synonym = synonymOf(*descriptor);
        m_configuration.comment = scalarAt(*descriptor, 4);
        QString descriptorGuidText;
        if (descriptorGuid(*descriptor, &descriptorGuidText))
            m_rootDescriptorGuid = descriptorGuidText;
    }

    const V8Element *versionElement = m_container.find(QStringLiteral("version"));
    if (versionElement) {
        QByteArray versionPayload;
        if (m_container.elementPayload(*versionElement, &versionPayload, nullptr, nullptr)) {
            QString versionText;
            if (readPayloadText(versionPayload, &versionText, nullptr)) {
                V8TextNode versionRoot;
                if (V8Text::parse(versionText, versionRoot, nullptr))
                    m_configuration.version = extractVersionNumber(versionRoot);
            }
        }
    }

    if (m_configuration.categories.isEmpty())
        addWarning(QStringLiteral("В объекте конфигурации не найдены таблицы метаданных"));

    return true;
}

bool ConfigurationReader::readRootGuid(QString *rootGuid)
{
    if (rootGuid)
        rootGuid->clear();

    V8TextNode rootText;
    if (m_container.find(QStringLiteral("root"))) {
        if (!parseElementTree(QStringLiteral("root"), &rootText)) {
            addWarning(QStringLiteral("Не удалось разобрать элемент «root»"));
            return false;
        }

        const V8TextNode *second = rootText.child(1);
        if (second && isGuidNode(*second)) {
            const QString guid = second->normalizedGuid();
            if (!guid.isEmpty()) {
                if (rootGuid)
                    *rootGuid = guid;
                return true;
            }
        }

        const QString guid = findFirstGuidDeep(rootText);
        if (guid.isEmpty()) {
            addWarning(QStringLiteral("В элементе «root» не найден идентификатор объекта"));
            return false;
        }
        if (rootGuid)
            *rootGuid = guid;
        return true;
    }

    // Расширения конфигурации (.cfe) хранят ссылку на объект конфигурации
    // в элементе configinfo.
    if (m_container.find(QStringLiteral("configinfo"))) {
        if (!parseElementTree(QStringLiteral("configinfo"), &rootText)) {
            addWarning(QStringLiteral("Не удалось разобрать элемент «configinfo»"));
            return false;
        }
        const QString guid = findFirstGuidDeep(rootText);
        if (guid.isEmpty()) {
            addWarning(QStringLiteral("В элементе «configinfo» не найден "
                                      "идентификатор объекта конфигурации"));
            return false;
        }
        if (rootGuid)
            *rootGuid = guid;
        if (m_configuration.kind == ConfigurationKind::Unknown) {
            m_configuration.kind = ConfigurationKind::Extension;
            m_configuration.kindTitle = kindTitleFor(m_configuration.kind);
        }
        return true;
    }

    return false;
}

bool ConfigurationReader::parseElementTree(const QString &elementName,
                                           V8TextNode *root) const
{
    if (root)
        *root = V8TextNode();

    const V8Element *element = m_container.find(elementName);
    if (!element)
        return false;

    QByteArray payload;
    if (!m_container.elementPayload(*element, &payload, nullptr, nullptr)
        || payload.isEmpty()) {
        return false;
    }

    QString text;
    if (!readPayloadText(payload, &text, nullptr) || text.trimmed().isEmpty())
        return false;

    V8TextNode parsed;
    if (!V8Text::parseSequence(text, parsed, nullptr))
        return false;

    if (root)
        *root = std::move(parsed);
    return true;
}

void ConfigurationReader::buildConfigurationMetadata(const V8TextNode &configRoot)
{
    QHash<QString, QStringList> references;
    std::function<void(const V8TextNode &, int)> scan = [&](const V8TextNode &node,
                                                          int depth) {
        if (depth > kMaxSearchDepth)
            return;

        if (node.isList() && node.children.size() >= 2) {
            const V8TextNode &first = node.children.at(0);
            qint64 count = 0;
            if (first.isScalar() && MetadataCatalog::category(first.value)
                && integerAt(node, 1, &count)) {
                const QString categoryGuid = V8Text::normalizeGuid(first.value);
                const qint64 available = static_cast<qint64>(node.children.size()) - 2;
                if (count > available)
                    count = available;
                QStringList &list = references[categoryGuid];
                for (qint64 i = 0; i < count; ++i) {
                    const V8TextNode &item = node.children.at(static_cast<int>(i) + 2);
                    const QString guid = guidOfNode(item);
                    if (!guid.isEmpty() && !list.contains(guid))
                        list.append(guid);
                }
            }
        }

        for (const V8TextNode &child : node.children)
            scan(child, depth + 1);
    };
    scan(configRoot, 0);

    struct Task
    {
        int categoryIndex = 0;
        QString guid;
        const MetadataCategoryInfo *info = nullptr;
    };

    QVector<Task> tasks;
    QVector<MetadataCategory> categories;

    for (const MetadataCategoryInfo &info : MetadataCatalog::categories()) {
        const QStringList guids = references.value(info.guid);
        if (guids.isEmpty())
            continue;

        MetadataCategory category;
        category.guid = info.guid;
        category.title = info.title;
        category.typeTitle = info.typeTitle;

        QStringList seen;
        for (const QString &guid : guids) {
            if (seen.contains(guid))
                continue;
            seen.append(guid);
            Task task;
            task.categoryIndex = categories.size();
            task.guid = guid;
            task.info = &info;
            tasks.append(task);
        }

        if (seen.isEmpty())
            continue;
        categories.append(category);
    }

    if (tasks.isEmpty())
        return;

    // Объекты независимы друг от друга: разбираем их параллельно.
    const auto buildOne = [this](const Task &task) {
        return buildObject(task.guid, *task.info);
    };
    QVector<MetadataObject> built =
        QtConcurrent::blockingMapped<QVector<MetadataObject>>(tasks, buildOne);

    for (int i = 0; i < tasks.size() && i < built.size(); ++i)
        categories[tasks.at(i).categoryIndex].objects.append(built.at(i));

    for (const MetadataObject &object : built)
        m_types.registerObject(object);

    for (const MetadataCategory &category : categories) {
        if (!category.objects.isEmpty())
            m_configuration.categories.append(category);
    }
}

void ConfigurationReader::buildExternalObject(const V8TextNode &objectRoot,
                                              const QString &rootGuid)
{
    const char *categoryGuid = categoryGuidForKind(m_configuration.kind);
    if (!categoryGuid)
        return;

    const MetadataCategoryInfo *info = MetadataCatalog::category(
        QString::fromLatin1(categoryGuid));
    if (!info)
        return;

    MetadataCategory category;
    category.guid = info->guid;
    category.title = m_configuration.kindTitle;
    category.typeTitle = info->typeTitle;

    MetadataObject object;
    object.categoryGuid = info->guid;
    object.categoryTitle = category.title;
    object.typeTitle = info->typeTitle;
    object.elementName = rootGuid;
    object.elementFound = true;
    object.rootObject = true;

    const DescriptorInfo descriptor = detail::descriptorFor(objectRoot);
    if (descriptor.found) {
        object.descriptorFound = true;
        object.guid = descriptor.guid.isEmpty() ? rootGuid : descriptor.guid;
        object.name = descriptor.name;
        object.synonym = descriptor.synonym;
        object.comment = descriptor.comment;
    } else {
        object.guid = rootGuid;
    }

    const V8TextNode *ownDescriptor = findDescriptorByGuid(objectRoot, object.guid, 0);
    if (!ownDescriptor)
        ownDescriptor = findFirstDescriptor(objectRoot, 0);
    if (ownDescriptor) {
        object.typeIds = m_types.collectTypeIds(objectRoot, ownDescriptor);
        object.typeText = m_types.typeTextForDescriptor(objectRoot, ownDescriptor);
    }

    object.sections = m_sections.byTableGuids(objectRoot, kMaxTableScanDepth);
    if (object.sections.isEmpty())
        object.sections = m_sections.byTableGuids(objectRoot, detail::kMaxSearchDepth);
    if (object.sections.isEmpty())
        object.sections = m_sections.fallback(objectRoot);
    object.sectionsLoaded = true;

    m_configuration.name = object.name;
    m_configuration.synonym = object.synonym;
    m_configuration.comment = object.comment;
    if (m_configuration.configGuid.isEmpty())
        m_configuration.configGuid = object.guid;

    category.objects.append(object);
    m_types.registerObject(object);
    m_configuration.categories.append(category);
}

MetadataObject ConfigurationReader::buildObject(const QString &guid,
                                                const MetadataCategoryInfo &category) const
{
    MetadataObject object;
    object.guid = guid;
    object.elementName = guid;
    object.name = guid;
    object.categoryGuid = category.guid;
    object.categoryTitle = category.title;
    object.typeTitle = category.typeTitle;

    const V8Element *element = m_container.find(guid);
    if (!element)
        return object;

    object.elementFound = true;

    V8TextNode root;
    if (!parseElementTree(guid, &root))
        return object;

    return objectFromRoot(guid, category, root);
}

MetadataObject ConfigurationReader::objectFromRoot(const QString &guid,
                                                   const MetadataCategoryInfo &category,
                                                   const V8TextNode &objectRoot) const
{
    MetadataObject object;
    object.guid = guid;
    object.elementName = guid;
    object.name = guid;
    object.categoryGuid = category.guid;
    object.categoryTitle = category.title;
    object.typeTitle = category.typeTitle;
    object.elementFound = true;

    const V8TextNode *descriptor = findDescriptorByGuid(objectRoot, guid, 0);
    if (!descriptor)
        descriptor = findFirstDescriptor(objectRoot, 0);
    if (descriptor) {
        object.descriptorFound = true;
        const QString name = scalarAt(*descriptor, 2);
        if (!name.isEmpty())
            object.name = name;
        object.synonym = synonymOf(*descriptor);
        object.comment = scalarAt(*descriptor, 4);
        object.typeIds = m_types.collectTypeIds(objectRoot, descriptor);
        // Собственный тип значения: общие реквизиты, константы, параметры сеанса.
        object.typeText = m_types.typeTextForDescriptor(objectRoot, descriptor);
        if (category.guid.compare(QStringLiteral("4e828da6-0f44-4b5b-b1c0-a2b3cfe7bdcc"),
                                  Qt::CaseInsensitive)
            == 0) {
            const SubscriptionInfo subscription =
                SubscriptionParser::parse(objectRoot, descriptor);
            object.sourceTypeGuids = subscription.sourceTypeGuids;
            object.eventName = subscription.eventName;
            object.handlerModuleGuid = subscription.handlerModuleGuid;
            object.handlerProcedure = subscription.handlerProcedure;
            // У подписки «Pattern» — это тип источника, а не собственный тип.
            object.typeText.clear();
        }
    }

    return object;
}

// Подписка на событие: {…, <описатель>, {"Pattern", {"#", тип}…},
//   "<Событие>", <GUID модуля>, "<ИмяОбработчика>"}.
QString ConfigurationReader::typeNameForGuid(const QString &typeGuid) const
{
    return m_types.typeNameForGuid(typeGuid);
}

QVector<ModuleInfo> ConfigurationReader::configurationModules() const
{
    QVector<ModuleInfo> modules;
    if (!m_container.isOpen() || m_rootDescriptorGuid.isEmpty())
        return modules;

    // В формате 8.3 корневые модули лежат в элементах <GUID описателя>.<номер>:
    // .6 — модуль приложения, .7 — модуль сеанса, .5 — модуль внешнего
    // соединения, .0 — модуль обычного приложения (проверено на БП 3.0).
    struct Spec
    {
        int suffix;
        const char *title;
    };
    static const Spec specs[] = {
        {6, "Модуль приложения"},
        {7, "Модуль сеанса"},
        {5, "Модуль внешнего соединения"},
        {0, "Модуль обычного приложения"},
    };

    for (const Spec &spec : specs) {
        const QString elementName =
            QStringLiteral("%1.%2").arg(m_rootDescriptorGuid).arg(spec.suffix);
        const V8Element *element = m_container.find(elementName);
        if (!element)
            continue;

        V8Container nested;
        if (!m_container.openNested(*element, &nested, nullptr))
            continue;

        ModuleInfo info;
        info.name = QString::fromUtf8(spec.title);
        info.elementName = elementName;
        info.fromNestedContainer = true;
        for (const V8Element &child : nested.elements()) {
            if (child.name != QStringLiteral("text")
                && child.name != QStringLiteral("module")) {
                continue;
            }
            QByteArray payload;
            if (!nested.elementPayload(child, &payload, nullptr, nullptr))
                continue;
            QString text;
            if (readPayloadText(payload, &text, nullptr))
                info.text = text;
        }
        modules.append(info);
    }
    return modules;
}

// Внутренние идентификаторы типов объекта стоят в его главном списке перед
// описателем: {<вид>, <typeId…>, {0, <описатель>}, …}. У некоторых видов
// (определяемые типы, перечисления) описатель лежит прямо в этом списке,
// поэтому GUID-ы собираются по всем предкам описателя — те, что стоят перед
// ним на пути от корня.
ConfigurationReader::ReferenceName ConfigurationReader::referenceName(
    const QString &guid) const
{
    const auto cached = m_referenceNames.constFind(guid);
    if (cached != m_referenceNames.constEnd())
        return cached.value();

    ReferenceName result;
    result.name = guid;

    V8TextNode root;
    if (parseElementTree(guid, &root)) {
        const V8TextNode *descriptor = findDescriptorByGuid(root, guid, 0);
        if (!descriptor)
            descriptor = findFirstDescriptor(root, 0);
        if (descriptor) {
            const QString name = scalarAt(*descriptor, 2);
            if (!name.isEmpty())
                result.name = name;
            result.synonym = synonymOf(*descriptor);
        }
    }

    m_referenceNames.insert(guid, result);
    return result;
}

bool ConfigurationReader::loadSubsystemChildren(MetadataObject &subsystem)
{
    if (subsystem.childrenLoaded)
        return true;

    subsystem.childrenLoaded = true;
    subsystem.childGuids.clear();

    const QString elementName = subsystem.elementName.isEmpty() ? subsystem.guid
                                                                : subsystem.elementName;
    V8TextNode root;
    if (!parseElementTree(elementName, &root))
        return false;

    // В элементе подсистемы четвёртый элемент корневого списка — таблица
    // вложенных подсистем: {<GUID таблицы>, <количество>, <GUID>...}.
    const V8TextNode *table = root.child(3);
    if (!table || !table->isList() || table->children.size() < 2)
        return true;

    const QString tableGuid = scalarAt(*table, 0);
    if (!tableGuid.isEmpty() && !V8Text::isGuid(tableGuid))
        return true;

    qint64 count = 0;
    const qint64 available = static_cast<qint64>(table->children.size()) - 2;
    if (!integerAt(*table, 1, &count) || count < 0 || count > available)
        count = available;

    for (qint64 i = 0; i < count; ++i) {
        const V8TextNode &item = table->children.at(static_cast<int>(i) + 2);
        const QString guid = isGuidNode(item) ? item.normalizedGuid()
                                              : findFirstGuidDeep(item);
        if (guid.isEmpty() || guid == MetadataCatalog::zeroGuid())
            continue;
        if (!subsystem.childGuids.contains(guid))
            subsystem.childGuids.append(guid);
    }

    return true;
}

MetadataObject *ConfigurationReader::cachedSubsystem(const QString &guid) const
{
    if (guid.isEmpty())
        return nullptr;
    const auto it = m_subsystemIndex.constFind(guid.toLower());
    return it != m_subsystemIndex.constEnd() ? it.value() : nullptr;
}

MetadataObject *ConfigurationReader::subsystemObject(const QString &guid)
{
    const QString normalized = V8Text::normalizeGuid(guid);
    if (normalized.isEmpty())
        return nullptr;

    // Если подсистема есть в основном списке категории — возвращаем её.
    if (MetadataObject *object = m_configuration.object(normalized))
        return object;
    if (MetadataObject *cached = cachedSubsystem(normalized))
        return cached;

    const MetadataCategoryInfo *info = MetadataCatalog::category(subsystemCategoryGuid());
    if (!info)
        return nullptr;

    V8TextNode root;
    if (!parseElementTree(normalized, &root))
        return nullptr;

    auto object = std::make_unique<MetadataObject>(
        objectFromRoot(normalized, *info, root));
    object->elementName = normalized;

    MetadataObject *pointer = object.get();
    m_subsystemObjects.push_back(std::move(object));
    m_subsystemIndex.insert(normalized, pointer);
    return pointer;
}
bool ConfigurationReader::readReferenceName(const QString &guid, QString *name,
                                            QString *synonym, QString *error) const
{
    if (name)
        name->clear();
    if (synonym)
        synonym->clear();
    if (error)
        error->clear();

    if (guid.isEmpty()) {
        if (error)
            *error = QStringLiteral("Не указан идентификатор объекта");
        return false;
    }

    const ReferenceName reference = referenceName(guid);
    if (name)
        *name = reference.name;
    if (synonym)
        *synonym = reference.synonym;
    return true;
}

bool ConfigurationReader::readElementText(const QString &elementName, QString *text,
                                          QString *error) const
{
    if (text)
        text->clear();
    if (error)
        error->clear();

    const V8Element *element = m_container.find(elementName);
    if (!element) {
        if (error)
            *error = QStringLiteral("Элемент «%1» не найден").arg(elementName);
        return false;
    }

    QByteArray payload;
    if (!m_container.elementPayload(*element, &payload, nullptr, error))
        return false;

    return readPayloadText(payload, text, error);
}

bool ConfigurationReader::readPayloadText(const QByteArray &payload, QString *text,
                                          QString *error) const
{
    if (text)
        text->clear();
    if (error)
        error->clear();

    if (payload.isEmpty())
        return true;

    QString encoding;
    QString decodeError;
    const QString decoded = V8Text::decode(payload, &encoding, &decodeError);
    if (decoded.isEmpty() && !payload.isEmpty()) {
        if (error) {
            *error = decodeError.isEmpty()
                ? QStringLiteral("Пустой текст после декодирования")
                : decodeError;
        }
        return false;
    }

    if (text)
        *text = decoded;
    return true;
}

bool ConfigurationReader::readObjectText(const MetadataObject &object, QString *text,
                                         QString *error) const
{
    const QString name = object.elementName.isEmpty() ? object.guid : object.elementName;
    return readElementText(name, text, error);
}

bool ConfigurationReader::loadSections(MetadataObject &object) const
{
    if (object.sectionsLoaded)
        return true;

    object.sectionsLoaded = true;

    const QString elementName = object.elementName.isEmpty() ? object.guid
                                                             : object.elementName;
    V8TextNode objectRoot;
    if (!parseElementTree(elementName, &objectRoot))
        return false;

    object.sections = m_sections.build(objectRoot, object.categoryGuid);
    return true;
}

void ConfigurationReader::addWarning(const QString &message)
{
    if (!m_configuration.warnings.contains(message))
        m_configuration.warnings.append(message);
}

} // namespace v8reader
