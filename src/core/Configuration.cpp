#include "core/Configuration.h"

namespace v8reader {

QString MetadataEntry::displayName() const
{
    if (!name.isEmpty())
        return name;
    if (!synonym.isEmpty())
        return synonym;
    if (!guid.isEmpty())
        return guid;
    return QStringLiteral("(без имени)");
}

QString MetadataObject::displayName() const
{
    if (!name.isEmpty())
        return name;
    if (!synonym.isEmpty())
        return synonym;
    if (!guid.isEmpty())
        return guid;
    return QStringLiteral("(без имени)");
}

bool MetadataObject::hasSections() const
{
    for (const MetadataSection &section : sections) {
        if (!section.isEmpty())
            return true;
    }
    return false;
}

const MetadataSection *MetadataObject::section(MetadataSectionKind kind) const
{
    for (const MetadataSection &section : sections) {
        if (section.kind == kind)
            return &section;
    }
    return nullptr;
}

QStringList MetadataObject::sectionSummaries() const
{
    QStringList result;
    for (const MetadataSection &section : sections) {
        if (section.isEmpty())
            continue;
        result.append(QStringLiteral("%1: %2")
                          .arg(section.title)
                          .arg(section.entries.size()));
    }
    return result;
}

QString Configuration::displayName() const
{
    if (!name.isEmpty())
        return name;
    if (!synonym.isEmpty())
        return synonym;
    if (!configGuid.isEmpty())
        return configGuid;
    return kindTitle.isEmpty() ? QStringLiteral("Конфигурация") : kindTitle;
}

int Configuration::objectCount() const
{
    int total = 0;
    for (const MetadataCategory &category : categories)
        total += category.objects.size();
    return total;
}

const MetadataCategory *Configuration::category(const QString &guid) const
{
    for (const MetadataCategory &category : categories) {
        if (category.guid.compare(guid, Qt::CaseInsensitive) == 0)
            return &category;
    }
    return nullptr;
}

const MetadataObject *Configuration::object(const QString &guid) const
{
    for (const MetadataCategory &category : categories) {
        for (const MetadataObject &object : category.objects) {
            if (object.guid.compare(guid, Qt::CaseInsensitive) == 0)
                return &object;
        }
    }
    return nullptr;
}

MetadataCategory *Configuration::category(const QString &guid)
{
    for (MetadataCategory &category : categories) {
        if (category.guid.compare(guid, Qt::CaseInsensitive) == 0)
            return &category;
    }
    return nullptr;
}

MetadataObject *Configuration::object(const QString &guid)
{
    for (MetadataCategory &category : categories) {
        for (MetadataObject &object : category.objects) {
            if (object.guid.compare(guid, Qt::CaseInsensitive) == 0)
                return &object;
        }
    }
    return nullptr;
}

QStringList Configuration::categoryTitles() const
{
    QStringList result;
    for (const MetadataCategory &category : categories) {
        if (!category.isEmpty())
            result.append(category.title);
    }
    return result;
}

} // namespace v8reader
