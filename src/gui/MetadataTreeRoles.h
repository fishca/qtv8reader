#ifndef QTV8READER_METADATATREEROLES_H
#define QTV8READER_METADATATREEROLES_H

#include <Qt>

namespace v8reader {

// Вид узла дерева метаданных.
enum ItemKind {
    RootItem = 1,
    GroupItem,
    CategoryItem,
    ObjectItem,
    SectionItem,
    EntryItem,
    ModuleNodeItem,
    RootModuleItem
};

// Роли QTreeWidgetItem. Числовые значения менять нельзя: на них завязаны
// обработчики выбора и ленивой загрузки.
enum ItemRole {
    KindRole = Qt::UserRole,
    GuidRole,
    SectionKindRole,
    EntryIndexRole,
    ChildIndexRole,
    ModuleIndexRole,
    LoadedRole,
    IndexRole
};

} // namespace v8reader

#endif // QTV8READER_METADATATREEROLES_H
