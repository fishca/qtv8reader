#ifndef QTV8READER_CLI_COMMANDS_H
#define QTV8READER_CLI_COMMANDS_H

#include <QString>

namespace v8reader::cli {

int commandHelp();
int commandInfo(const QString &path);
int commandList(const QString &path);
int commandTree(const QString &path);
int commandDump(const QString &path, const QString &elementName,
                const QString &output, bool raw);
int commandMetadata(const QString &path, const QString &objectQuery,
                    const QString &output);
int commandModules(const QString &path, const QString &objectQuery,
                   const QString &output);
int commandComposition(const QString &path, const QString &objectQuery);
int commandFormModel(const QString &path, const QString &objectQuery,
                     const QString &formQuery, const QString &output, bool itemsOnly);
int commandMxl(const QString &path, const QString &objectQuery,
               const QString &output);
int commandExtract(const QString &path, const QString &objectQuery,
                   const QString &output);
int commandExtractAll(const QString &path, const QString &output);

} // namespace v8reader::cli

#endif // QTV8READER_CLI_COMMANDS_H
