#ifndef QTV8READER_MAINWINDOWSUPPORT_H
#define QTV8READER_MAINWINDOWSUPPORT_H

#include <QString>
#include <QStringList>

namespace v8reader {

QString sizeText(qint64 size);
bool isEventSubscriptionCategory(const QString &guid);

const QString &documentsCategoryGuid();
const QString &numeratorsCategoryGuid();
const QString &sequencesCategoryGuid();

const QStringList &canonicalTopLevelOrder();
const QStringList &standardVisibleBranches();
const QStringList &canonicalCommonOrder();
const QStringList &standardCommonVisibleBranches();

bool isStandardVisibleBranch(const QString &guid);
bool isCanonicalTopLevel(const QString &guid);
bool isSubsystemCategory(const QString &guid);
bool isCommonCategoryGuid(const QString &guid);

} // namespace v8reader

#endif // QTV8READER_MAINWINDOWSUPPORT_H
