#ifndef QTV8READER_CLI_VIEWCOMMAND_H
#define QTV8READER_CLI_VIEWCOMMAND_H

#include <QString>

namespace v8reader::cli {

// Один вызов на экран панели чтения конфигурации: сведения о файле, дерево
// категорий и объектов, состав выбранного объекта и, по запросу, текст модуля.
// Вывод — стабильный JSON (схема 1) или короткая текстовая сводка.
int commandView(const QString &path, const QString &objectQuery,
                const QString &moduleName, bool json);

} // namespace v8reader::cli

#endif // QTV8READER_CLI_VIEWCOMMAND_H
