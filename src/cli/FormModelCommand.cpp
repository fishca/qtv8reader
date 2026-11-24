#include "cli/CommandCommon.h"

#include "core/FormBindings.h"
#include "core/FormOwner.h"
#include "core/FormPictures.h"
#include "core/FormReader.h"
#include "core/FormStyles.h"
#include "core/FormWebModel.h"

namespace v8reader::cli {

namespace {

const MetadataEntry *findFormEntry(const MetadataObject &object,
                                   const QString &query)
{
    const MetadataSection *forms = object.section(MetadataSectionKind::Forms);
    if (!forms || forms->entries.isEmpty())
        return nullptr;
    if (!query.isEmpty()) {
        for (const MetadataEntry &entry : forms->entries) {
            if (entry.name.compare(query, Qt::CaseInsensitive) == 0
                || entry.synonym.compare(query, Qt::CaseInsensitive) == 0)
                return &entry;
        }
        for (const MetadataEntry &entry : forms->entries) {
            if (entry.name.contains(query, Qt::CaseInsensitive))
                return &entry;
        }
        return nullptr;
    }
    for (const MetadataEntry &entry : forms->entries) {
        const QString name = entry.name.toLower();
        if (name == QStringLiteral("формадокумента")
            || name == QStringLiteral("формаэлемента")
            || name == QStringLiteral("формасписка")) {
            return &entry;
        }
    }
    return &forms->entries.first();
}

void printItemTree(const QVector<FormItem> &items, int depth)
{
    const QString indent(depth * 2, QLatin1Char(' '));
    for (const FormItem &item : items) {
        out() << QStringLiteral("%1%2 \"%3\" type=%4 kind=%5 orient=%6 w=%7 h=%8")
                     .arg(indent)
                     .arg(item.typeTitle())
                     .arg(item.name)
                     .arg(item.typeCode)
                     .arg(item.groupKind)
                     .arg(item.orientation)
                     .arg(item.widthUnits)
                     .arg(item.heightUnits);
        if (!item.synonym.isEmpty())
            out() << QStringLiteral(" syn=\"%1\"").arg(item.synonym);
        if (!item.dataTitle.isEmpty())
            out() << QStringLiteral(" dataTitle=\"%1\"").arg(item.dataTitle);
        if (!item.dataPathText.isEmpty())
            out() << QStringLiteral(" data=%1").arg(item.dataPathText);
        if (!item.typeText.isEmpty())
            out() << QStringLiteral(" [%1]").arg(item.typeText);
        if (item.valueKind != FormValueKind::Unknown) {
            out() << QStringLiteral(" value=%1/%2")
                         .arg(static_cast<int>(item.valueKind))
                         .arg(item.valueChars);
        }
        if (!item.showTitle)
            out() << QStringLiteral(" noTitle");
        if (item.titleLocation != FormTitleLocation::Auto) {
            out() << QStringLiteral(" titleLoc=%1")
                         .arg(static_cast<int>(item.titleLocation));
        }
        if (item.labelOnly)
            out() << QStringLiteral(" labelOnly");
        if (!item.visible)
            out() << QStringLiteral(" hidden");
        if (!item.onCommandBar)
            out() << QStringLiteral(" offBar");
        if (item.horizontalStretch >= 0) {
            out() << QStringLiteral(" hStretch=%1").arg(item.horizontalStretch);
        }
        if (item.representation != FormGroupRepresentation::None) {
            out() << QStringLiteral(" repr=%1")
                         .arg(static_cast<int>(item.representation));
        }
        if (item.behavior != FormGroupBehavior::Usual) {
            out() << QStringLiteral(" behavior=%1")
                         .arg(static_cast<int>(item.behavior));
        }
        out() << QLatin1Char('\n');
        if (!item.children.isEmpty())
            printItemTree(item.children, depth + 1);
    }
}

} // namespace

int commandFormModel(const QString &path, const QString &objectQuery,
                     const QString &formQuery, const QString &output, bool itemsOnly,
                     bool nodes)
{
    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    QString error;
    MetadataObject *object = findObject(reader.configuration(), objectQuery, &error);
    if (!object) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }
    reader.loadSections(*object);

    const MetadataEntry *formEntry = findFormEntry(*object, formQuery);
    if (!formEntry) {
        err() << QStringLiteral("Ошибка: форма «%1» не найдена у объекта «%2»\n")
                     .arg(formQuery, object->displayName());
        return 1;
    }

    const V8Container &container = reader.container();
    const V8Element *element = !formEntry->guid.isEmpty()
        ? container.find(formEntry->guid + QStringLiteral(".0"))
        : nullptr;
    if (!element && !formEntry->guid.isEmpty())
        element = container.find(formEntry->guid);
    if (!element && !formEntry->name.isEmpty())
        element = container.find(formEntry->name);
    if (!element) {
        err() << QStringLiteral("Ошибка: данные формы «%1» не найдены в контейнере\n")
                     .arg(formEntry->displayName());
        return 1;
    }

    QByteArray payload;
    if (!container.elementPayload(*element, &payload, nullptr, &error)) {
        err() << QStringLiteral("Ошибка: не удалось прочитать форму «%1»: %2\n")
                     .arg(formEntry->displayName(), error);
        return 1;
    }

    FormDocument document;
    FormBindingResolver resolver;
    resolver.addObject(*object);
    FormStyleTable styles;
    styles.loadFromConfiguration(reader.configuration(), reader.container());
    FormPictureTable pictures;
    pictures.loadFromConfiguration(reader.configuration(), reader.container());
    if (!FormReader::parseBytes(payload, &document, &error,
                                resolver.isEmpty() ? nullptr : &resolver,
                                &styles, &pictures)) {
        err() << QStringLiteral("Ошибка: не удалось разобрать форму «%1»: %2\n")
                     .arg(formEntry->displayName(), error);
        return 1;
    }

    document.name = formEntry->name;
    document.synonym = formEntry->synonym;
    applyOwnerMetadata(*object, &document);
    QString objectText;
    if (reader.readObjectText(*object, &objectText, nullptr))
        applyOwnerStandardAttributes(objectText, &document);
    applyReferencedCatalogMeta(reader, &document);

    if (itemsOnly) {
        out() << QStringLiteral("Форма: %1 (%2)\n")
                     .arg(document.name, document.synonym);
        printItemTree(document.items, 0);
        return 0;
    }

    if (nodes) {
        const QByteArray described = FormReader::describeNodes(payload, &error);
        if (described.isEmpty()) {
            err() << QStringLiteral("Ошибка: не удалось описать узлы формы «%1»: %2\n")
                         .arg(formEntry->displayName(), error);
            return 1;
        }
        if (output.isEmpty()) {
            out() << QString::fromUtf8(described) << QLatin1Char('\n');
            return 0;
        }
        QFile nodeFile(output);
        if (!nodeFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            err() << QStringLiteral("Ошибка: не удалось записать «%1»\n").arg(output);
            return 1;
        }
        nodeFile.write(described);
        out() << QStringLiteral("Узлы формы сохранены: %1 (%2 байт)\n")
                     .arg(output)
                     .arg(described.size());
        return 0;
    }

    const QByteArray json = formWebModelJson(document);
    if (output.isEmpty()) {
        out() << QString::fromUtf8(json) << QLatin1Char('\n');
        return 0;
    }
    QFile file(output);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        err() << QStringLiteral("Ошибка: не удалось записать «%1»\n").arg(output);
        return 1;
    }
    file.write(json);
    out() << QStringLiteral("Модель формы сохранена: %1 (%2 байт)\n")
                 .arg(output)
                 .arg(json.size());
    return 0;
}

} // namespace v8reader::cli
