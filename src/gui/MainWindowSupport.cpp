#include "gui/MainWindowSupport.h"

#include <QSet>

namespace v8reader {

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

bool isEventSubscriptionCategory(const QString &guid)
{
    return guid.compare(QStringLiteral("4e828da6-0f44-4b5b-b1c0-a2b3cfe7bdcc"),
                        Qt::CaseInsensitive)
        == 0;
}

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
        QStringLiteral("0195e80c-b157-11d4-9435-004095e12fc7"),
        QStringLiteral("cf4abea6-37b2-11d4-940f-008048da11f9"),
        QStringLiteral("061d872a-5787-460e-95ac-ed74ea3a3e84"),
        QStringLiteral("4612bd75-71b7-4a5c-8cc5-2b0b65f9fa0d"),
        QStringLiteral("f6a80749-5ad7-400b-8519-39dc5dff2542"),
        QStringLiteral("631b75a0-29e2-11d6-a3c7-0050bae0a776"),
        QStringLiteral("bf845118-327b-4682-b5c6-285d2a0eb296"),
        QStringLiteral("82a1b659-b220-4d94-a9bd-14d757b95a48"),
        QStringLiteral("238e7e88-3c5f-48b2-8a3b-81ebbecb20ed"),
        QStringLiteral("30b100d6-b29f-47ac-aec7-cb8ca8a54767"),
        QStringLiteral("13134201-f60b-11d5-a3c7-0050bae0a776"),
        QStringLiteral("b64d9a40-1642-11d6-a3c7-0050bae0a776"),
        QStringLiteral("2deed9b8-0056-4ffe-a473-c20a6c32a0bc"),
        QStringLiteral("f2de87a8-64e5-45eb-a22d-b3aedab050e7"),
        QStringLiteral("fcd3404e-1523-48ce-9bc0-ecdb822684a1"),
        QStringLiteral("3e63355c-1378-4953-be9b-1deb5fb6bec5"),
        QStringLiteral("5274d9fc-9c3a-4a71-8f5e-a0db8ab23de5"),
    };
    return order;
}

const QStringList &standardVisibleBranches()
{
    return canonicalTopLevelOrder();
}

const QStringList &canonicalCommonOrder()
{
    static const QStringList order = {
        QStringLiteral("37f2fa9a-b276-11d4-9435-004095e12fc7"),
        QStringLiteral("0fe48980-252d-11d6-a3c7-0050bae0a776"),
        QStringLiteral("24c43748-c938-45d0-8d14-01424a72b11e"),
        QStringLiteral("09736b02-9cac-4e3f-b4f7-d3e9576ab948"),
        QStringLiteral("15794563-ccec-41f6-a83c-ec5f7b9a5bc1"),
        QStringLiteral("857c4a91-e5f4-4fac-86ec-787626f1c108"),
        QStringLiteral("3e7bfcc0-067d-11d6-a3c7-0050bae0a776"),
        QStringLiteral("4e828da6-0f44-4b5b-b1c0-a2b3cfe7bdcc"),
        QStringLiteral("11bdaf85-d5ad-4d91-bb24-aa0eee139052"),
        QStringLiteral("6e6dc072-b7ac-41e7-8f88-278d25b6da2a"),
        QStringLiteral("af547940-3268-434f-a3e7-e47d6d2638c3"),
        QStringLiteral("30d554db-541e-4f62-8970-a1c6dcfeb2bc"),
        QStringLiteral("c045099e-13b9-4fb6-9d50-fca00202971e"),
        QStringLiteral("46b4cd97-fd13-4eaa-aba2-3bddd7699218"),
        QStringLiteral("2f1a5187-fb0e-4b05-9489-dc5dd6412348"),
        QStringLiteral("1c57eabe-7349-44b3-b1de-ebfeab67b47d"),
        QStringLiteral("07ee8426-87f1-11d5-b99c-0050bae0a95d"),
        QStringLiteral("0c89c792-16c3-11d5-b96b-0050bae0a95d"),
        QStringLiteral("7dcd43d9-aca5-4926-b549-1842e6a4e8cf"),
        QStringLiteral("cc9df798-7c94-4616-97d2-7aa0b7bc515e"),
        QStringLiteral("8657032e-7740-4e1d-a3ba-5dd6e8afb78f"),
        QStringLiteral("0fffc09c-8f4c-47cc-b41c-8d5c5a221d79"),
        QStringLiteral("d26096fb-7a5d-4df9-af63-47d04771fa9b"),
        QStringLiteral("a7641777-7813-45c6-96ef-9d51587a6ac6"),
        QStringLiteral("bf3420b0-f6f9-41a0-b83a-fe9d4ab0b65d"),
        QStringLiteral("58848766-36ea-4076-8800-e91eb49590d7"),
        QStringLiteral("3e5404af-6ef8-4c73-ad11-91bd2dfac4c8"),
        QStringLiteral("9cd510ce-abfc-11d4-9434-004095e12fc7"),
        QStringLiteral("39bddf6a-0c3c-452b-921c-d99cfa1c2f1b"),
    };
    return order;
}

const QStringList &standardCommonVisibleBranches()
{
    static const QStringList branches = [] {
        QStringList list = canonicalCommonOrder();
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
                        Qt::CaseInsensitive)
        == 0;
}

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

} // namespace v8reader
