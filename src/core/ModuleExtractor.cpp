#include "core/ModuleExtractor.h"

#include <QSet>

#include <algorithm>

namespace v8reader {
namespace {

bool isMetadataTree(const QString &text)
{
    const QString trimmed = text.trimmed();
    return trimmed.startsWith(QLatin1Char('{'));
}

// "Общие команды": у объекта-команды модуль лежит в элементах <GUID>.0…5
// и называется «Модуль команды» (а не «Модуль объекта»).
bool isCommonCommandCategory(const QString &categoryGuid)
{
    return categoryGuid.compare(QStringLiteral("2f1a5187-fb0e-4b05-9489-dc5dd6412348"),
                                Qt::CaseInsensitive)
        == 0;
}

int numericSuffix(const QString &elementName, const QString &guid)
{
    if (!elementName.startsWith(guid + QLatin1Char('.'), Qt::CaseInsensitive))
        return -1;
    const QString suffix = elementName.mid(guid.size() + 1);
    bool ok = false;
    const int value = suffix.toInt(&ok);
    return ok ? value : -1;
}

} // namespace

ModuleExtractor::ModuleExtractor(const V8Container &container)
    : m_container(container)
{
}

QVector<ModuleInfo> ModuleExtractor::extractForObject(const MetadataObject &object) const
{
    QStringList guids;
    if (!object.guid.isEmpty())
        guids.append(object.guid);
    if (!object.elementName.isEmpty() && !guids.contains(object.elementName))
        guids.append(object.elementName);

    QVector<ModuleInfo> modules;
    QSet<QString> seenTexts;

    const auto appendUnique = [&](const QVector<ModuleInfo> &found,
                                  const QString &labelOverride) {
        for (ModuleInfo module : found) {
            const QString key = module.text.trimmed();
            if (key.isEmpty() || seenTexts.contains(key))
                continue;
            seenTexts.insert(key);
            if (!labelOverride.isEmpty())
                module.name = labelOverride;
            if (module.name.isEmpty())
                module.name = module.elementName;
            modules.append(module);
        }
    };

    // Порядок проверки элементов повторяет логику эталонного v8_reader:
    // модуль объекта — <GUID>.0 .. .5, модуль менеджера — .3, .1, .2, .4, .5, .0.
    if (isCommonCommandCategory(object.categoryGuid)) {
        for (const QString &guid : guids) {
            appendUnique(extractPrimary(guid, {0, 1, 2, 3, 4, 5},
                                        QStringLiteral("Модуль команды")),
                         QStringLiteral("Модуль команды"));
        }
    } else {
        for (const QString &guid : guids) {
            appendUnique(extractPrimary(guid, {0, 1, 2, 3, 4, 5},
                                        QStringLiteral("Модуль объекта")),
                         QStringLiteral("Модуль объекта"));
            appendUnique(extractPrimary(guid, {3, 1, 2, 4, 5, 0},
                                        QStringLiteral("Модуль менеджера")),
                         QStringLiteral("Модуль менеджера"));
        }
    }

    // Модули команд объекта: <GUID команды>.0 .. .5.
    for (const MetadataSection &section : object.sections) {
        if (section.kind != MetadataSectionKind::Commands)
            continue;
        for (const MetadataEntry &entry : section.entries) {
            if (entry.guid.isEmpty())
                continue;
            appendUnique(extractPrimary(entry.guid, {0, 1, 2, 3, 4, 5},
                                        QStringLiteral("Модуль команды")),
                         QStringLiteral("Модуль команды «%1»").arg(entry.displayName()));
        }
    }

    // Модули форм объекта: у формы модуль хранится в её собственных элементах
    // контейнера (<GUID формы>.<N>).
    for (const MetadataSection &section : object.sections) {
        if (section.kind != MetadataSectionKind::Forms)
            continue;
        for (const MetadataEntry &entry : section.entries) {
            if (entry.guid.isEmpty())
                continue;
            appendUnique(extractPrimary(entry.guid, {0, 1, 2, 3, 4, 5},
                                        QStringLiteral("Модуль формы")),
                         QStringLiteral("Модуль формы «%1»").arg(entry.displayName()));
        }
    }

    return modules;
}

QVector<ModuleInfo> ModuleExtractor::extractPrimary(const QString &guid,
                                                    const QVector<int> &suffixOrder,
                                                    const QString &label) const
{
    QVector<ModuleInfo> result;
    if (guid.isEmpty() || !m_container.isOpen())
        return result;

    for (int suffix : suffixOrder) {
        const QString elementName = guid + QLatin1Char('.') + QString::number(suffix);
        if (!m_container.find(elementName))
            continue;

        QVector<ModuleInfo> found;
        searchElement(elementName, label, &found);
        if (!found.isEmpty())
            return found;
    }
    return result;
}

QVector<ModuleInfo> ModuleExtractor::extractForGuid(const QString &guid) const
{
    QVector<ModuleInfo> modules;
    if (guid.isEmpty() || !m_container.isOpen())
        return modules;

    QStringList candidates;
    candidates.append(guid);

    const QString prefix = guid + QLatin1Char('.');
    for (const V8Element &element : m_container.elements()) {
        if (element.name.compare(guid, Qt::CaseInsensitive) == 0)
            continue;
        if (!element.name.startsWith(prefix, Qt::CaseInsensitive))
            continue;
        if (!candidates.contains(element.name, Qt::CaseInsensitive))
            candidates.append(element.name);
    }

    // Наиболее вероятные элементы модулей проверяем первыми.
    std::stable_sort(candidates.begin(), candidates.end(),
                     [&](const QString &left, const QString &right) {
                         const auto rank = [&](const QString &name) {
                             const int suffix = numericSuffix(name, guid);
                             return suffix < 0 ? 1000 : suffix;
                         };
                         return rank(left) < rank(right);
                     });

    QSet<QString> seenTexts;
    for (const QString &candidate : candidates) {
        QString label = moduleLabelForSuffix(numericSuffix(candidate, guid));
        if (label.isEmpty())
            label = QStringLiteral("Модуль (%1)").arg(candidate);

        const int before = modules.size();
        searchElement(candidate, label, &modules);

        // Убираем повторно найденный текст.
        for (int i = before; i < modules.size(); ++i) {
            const QString key = modules.at(i).text.trimmed();
            if (seenTexts.contains(key)) {
                modules.removeAt(i);
                --i;
                continue;
            }
            seenTexts.insert(key);
        }
    }

    return modules;
}

QVector<ModuleInfo> ModuleExtractor::extractFromElement(const QString &elementName) const
{
    QVector<ModuleInfo> modules;
    if (elementName.isEmpty() || !m_container.isOpen())
        return modules;

    searchElement(elementName, QStringLiteral("Модуль (%1)").arg(elementName), &modules);
    return modules;
}

bool ModuleExtractor::looksLikeModule(const QString &text)
{
    if (text.trimmed().isEmpty() || text.size() > 64 * 1024 * 1024)
        return false;

    const QString trimmed = text.trimmed();
    if (trimmed.startsWith(QLatin1Char('{')) || trimmed.startsWith(QLatin1Char('<')))
        return false;

    // Описания объектов (HTML-справка) модулями не являются.
    if (trimmed.contains(QStringLiteral("<!DOCTYPE html"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("<html"), Qt::CaseInsensitive)) {
        return false;
    }

    // Модуль может целиком состоять из комментариев.
    bool commentOnly = true;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString lineTrimmed = line.trimmed();
        if (lineTrimmed.isEmpty())
            continue;
        if (!lineTrimmed.startsWith(QLatin1String("//"))) {
            commentOnly = false;
            break;
        }
    }
    if (commentOnly)
        return true;

    static const QStringList markers = {
        QStringLiteral("Процедура"),
        QStringLiteral("Функция"),
        QStringLiteral("КонецПроцедуры"),
        QStringLiteral("КонецФункции"),
        QStringLiteral("Procedure"),
        QStringLiteral("Function"),
        QStringLiteral("EndProcedure"),
        QStringLiteral("EndFunction"),
        QStringLiteral("&НаКлиенте"),
        QStringLiteral("&НаСервере"),
        QStringLiteral("&AtClient"),
        QStringLiteral("&AtServer"),
        QStringLiteral("#Если"),
        QStringLiteral("#Область"),
        QStringLiteral("#КонецОбласти"),
        QStringLiteral("#Region"),
        QStringLiteral("#EndRegion"),
        QStringLiteral("Перем "),
    };

    for (const QString &marker : markers) {
        if (text.contains(marker, Qt::CaseInsensitive))
            return true;
    }

    return false;
}

QString ModuleExtractor::moduleLabelForSuffix(int suffix)
{
    switch (suffix) {
    case 0:
        return QStringLiteral("Модуль объекта");
    case 1:
        return QStringLiteral("Модуль менеджера");
    case 2:
        return QStringLiteral("Модуль набора записей");
    case 3:
        return QStringLiteral("Модуль формы");
    case 4:
        return QStringLiteral("Модуль команды");
    case 5:
        return QStringLiteral("Модуль");
    default:
        return suffix > 5 ? QStringLiteral("Модуль %1").arg(suffix) : QString();
    }
}

void ModuleExtractor::searchElement(const QString &elementName, const QString &label,
                                    QVector<ModuleInfo> *modules) const
{
    if (!modules)
        return;

    const V8Element *element = m_container.find(elementName);
    if (!element)
        return;

    QByteArray payload;
    if (!m_container.elementPayload(*element, &payload, nullptr, nullptr)
        || payload.isEmpty()) {
        return;
    }

    if (V8Container::looksLikeContainer(payload)) {
        V8Container nested;
        if (nested.openData(payload, nullptr)) {
            const int before = modules->size();
            searchNested(nested, elementName, modules, 0);
            for (int i = before; i < modules->size(); ++i) {
                ModuleInfo &info = (*modules)[i];
                info.elementName = info.elementName.isEmpty()
                    ? elementName
                    : info.elementName;
                info.fromNestedContainer = true;
                if (info.name.isEmpty())
                    info.name = label;
            }
            return;
        }
    }

    searchPayload(payload, elementName, label, modules);
}

void ModuleExtractor::searchNested(const V8Container &container, const QString &prefix,
                                   QVector<ModuleInfo> *modules, int depth) const
{
    if (!modules || depth > 8)
        return;

    for (const V8Element &element : container.elements()) {
        QByteArray payload;
        if (!container.elementPayload(element, &payload, nullptr, nullptr)
            || payload.isEmpty()) {
            continue;
        }

        const QString path = prefix + QLatin1Char('/') + element.name;

        if (V8Container::looksLikeContainer(payload)) {
            V8Container nested;
            if (nested.openData(payload, nullptr)) {
                searchNested(nested, path, modules, depth + 1);
                continue;
            }
        }

        QString label;
        const QString lowerName = element.name.toLower();
        if (lowerName.contains(QStringLiteral("module"))
            || lowerName.contains(QStringLiteral("модул"))) {
            label = QStringLiteral("Модуль (%1)").arg(path);
        } else if (lowerName.contains(QStringLiteral("text"))
                   || lowerName.contains(QStringLiteral("текст"))) {
            label = QStringLiteral("Модуль (%1)").arg(path);
        } else {
            label = QStringLiteral("Модуль (%1)").arg(path);
        }

        const int before = modules->size();
        searchPayload(payload, path, label, modules);
        for (int i = before; i < modules->size(); ++i) {
            ModuleInfo &info = (*modules)[i];
            if (info.elementName.isEmpty())
                info.elementName = path;
            info.fromNestedContainer = true;
        }
    }
}

void ModuleExtractor::searchPayload(const QByteArray &payload, const QString &elementName,
                                    const QString &label,
                                    QVector<ModuleInfo> *modules) const
{
    if (!modules || payload.isEmpty())
        return;

    QString encoding;
    const QString text = V8Text::decode(payload, &encoding, nullptr);
    if (text.isEmpty())
        return;

    const auto appendModule = [&](const QString &moduleText, const QString &source) {
        if (!looksLikeModule(moduleText))
            return;
        ModuleInfo info;
        info.name = label;
        info.elementName = elementName;
        info.source = source;
        info.text = moduleText;
        modules->append(info);
    };

    if (!isMetadataTree(text)) {
        appendModule(text, QStringLiteral("элемент «%1»").arg(elementName));
        return;
    }

    V8TextNode root;
    if (V8Text::parseSequence(text, root, nullptr))
        searchTree(root, elementName, label, modules, 0);
}

void ModuleExtractor::searchTree(const V8TextNode &node, const QString &elementName,
                                 const QString &label,
                                 QVector<ModuleInfo> *modules, int depth) const
{
    if (!modules || depth > 64)
        return;

    if (node.isScalar()) {
        const QString value = node.value;
        if (looksLikeModule(value)) {
            ModuleInfo info;
            info.name = label;
            info.elementName = elementName;
            info.source = QStringLiteral("строка метаданных «%1»").arg(elementName);
            info.text = value;
            modules->append(info);
            return;
        }

        if (value.startsWith(QLatin1String("#base64:"))
            || value.startsWith(QLatin1String("#data:"))) {
            QByteArray decoded;
            if (V8Text::decodeBase64Value(value, &decoded) && !decoded.isEmpty()) {
                QString encoding;
                const QString moduleText = V8Text::decode(decoded, &encoding, nullptr);
                if (!moduleText.isEmpty() && looksLikeModule(moduleText)) {
                    ModuleInfo info;
                    info.name = label;
                    info.elementName = elementName;
                    info.source = QStringLiteral("base64 в элементе «%1»").arg(elementName);
                    info.text = moduleText;
                    modules->append(info);
                    return;
                }

                if (isMetadataTree(moduleText)) {
                    V8TextNode decodedRoot;
                    if (V8Text::parseSequence(moduleText, decodedRoot, nullptr)) {
                        searchTree(decodedRoot, elementName, label, modules, depth + 1);
                    }
                }
            }
        }
        return;
    }

    for (const V8TextNode &child : node.children)
        searchTree(child, elementName, label, modules, depth + 1);
}

} // namespace v8reader
