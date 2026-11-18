#include "gui/MetadataIcons.h"

#include <QFile>
#include <QHash>
#include <QSize>

namespace v8reader {

QIcon MetadataIcons::icon(const QString &name)
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

QString MetadataIcons::categoryIconName(const QString &guid)
{
    static const QHash<QString, QString> map = [] {
        QHash<QString, QString> m;
        const auto add = [&m](const char *categoryGuid, const char *icon) {
            m.insert(QString::fromLatin1(categoryGuid), QString::fromLatin1(icon));
        };
        add("0195e80c-b157-11d4-9435-004095e12fc7", "constant");
        add("cf4abea6-37b2-11d4-940f-008048da11f9", "catalog");
        add("061d872a-5787-460e-95ac-ed74ea3a3e84", "document");
        add("36a8e346-9aaa-4af9-bdbd-83be3c177977", "documentNumerator");
        add("4612bd75-71b7-4a5c-8cc5-2b0b65f9fa0d", "documentJournal");
        add("f6a80749-5ad7-400b-8519-39dc5dff2542", "enum");
        add("631b75a0-29e2-11d6-a3c7-0050bae0a776", "report");
        add("bf845118-327b-4682-b5c6-285d2a0eb296", "dataProcessor");
        add("857c4a91-e5f4-4fac-86ec-787626f1c108", "exchangePlan");
        add("238e7e88-3c5f-48b2-8a3b-81ebbecb20ed", "chartsOfAccount");
        add("82a1b659-b220-4d94-a9bd-14d757b95a48", "chartsOfCharacteristicType");
        add("30b100d6-b29f-47ac-aec7-cb8ca8a54767", "chartsOfCalculationType");
        add("13134201-f60b-11d5-a3c7-0050bae0a776", "informationRegister");
        add("b64d9a40-1642-11d6-a3c7-0050bae0a776", "accumulationRegister");
        add("2deed9b8-0056-4ffe-a473-c20a6c32a0bc", "accountingRegister");
        add("f2de87a8-64e5-45eb-a22d-b3aedab050e7", "calculationRegister");
        add("bc587f20-35d9-11d6-a3c7-0050bae0a776", "sequence");
        add("fcd3404e-1523-48ce-9bc0-ecdb822684a1", "businessProcess");
        add("3e63355c-1378-4953-be9b-1deb5fb6bec5", "task");
        add("5274d9fc-9c3a-4a71-8f5e-a0db8ab23de5", "externalDataSource");
        add("0fe48980-252d-11d6-a3c7-0050bae0a776", "commonModule");
        add("07ee8426-87f1-11d5-b99c-0050bae0a95d", "form");
        add("0c89c792-16c3-11d5-b96b-0050bae0a95d", "template");
        add("7dcd43d9-aca5-4926-b549-1842e6a4e8cf", "picture");
        add("2f1a5187-fb0e-4b05-9489-dc5dd6412348", "command");
        add("1c57eabe-7349-44b3-b1de-ebfeab67b47d", "command");
        add("15794563-ccec-41f6-a83c-ec5f7b9a5bc1", "attribute");
        add("09736b02-9cac-4e3f-b4f7-d3e9576ab948", "role");
        add("37f2fa9a-b276-11d4-9435-004095e12fc7", "subsystem");
        add("24c43748-c938-45d0-8d14-01424a72b11e", "sessionParameter");
        add("af547940-3268-434f-a3e7-e47d6d2638c3", "parameter");
        add("30d554db-541e-4f62-8970-a1c6dcfeb2bc", "parameter");
        add("c045099e-13b9-4fb6-9d50-fca00202971e", "parameter");
        add("3e7bfcc0-067d-11d6-a3c7-0050bae0a776", "filterCriteria");
        add("4e828da6-0f44-4b5b-b1c0-a2b3cfe7bdcc", "eventSubscription");
        add("11bdaf85-d5ad-4d91-bb24-aa0eee139052", "scheduledJob");
        add("46b4cd97-fd13-4eaa-aba2-3bddd7699218", "settingsStorage");
        add("3e5404af-6ef8-4c73-ad11-91bd2dfac4c8", "style");
        add("58848766-36ea-4076-8800-e91eb49590d7", "style");
        add("9cd510ce-abfc-11d4-9434-004095e12fc7", "language");
        add("39bddf6a-0c3c-452b-921c-d99cfa1c2f1b", "interfaces");
        add("0fffc09c-8f4c-47cc-b41c-8d5c5a221d79", "http");
        add("8657032e-7740-4e1d-a3ba-5dd6e8afb78f", "ws");
        add("d26096fb-7a5d-4df9-af63-47d04771fa9b", "wsLink");
        add("cc9df798-7c94-4616-97d2-7aa0b7bc515e", "urlTemplate");
        add("6e6dc072-b7ac-41e7-8f88-278d25b6da2a", "bots");
        add("a7641777-7813-45c6-96ef-9d51587a6ac6", "ws");
        add("bf3420b0-f6f9-41a0-b83a-fe9d4ab0b65d", "ws");
        return m;
    }();

    const auto it = map.constFind(guid.toLower());
    return it != map.constEnd() ? it.value() : QStringLiteral("common");
}

QString MetadataIcons::sectionIconName(MetadataSectionKind kind)
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

} // namespace v8reader
