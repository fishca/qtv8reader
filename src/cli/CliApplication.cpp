#include "cli/CliApplication.h"

#include "cli/Commands.h"
#include "cli/Output.h"

#include <QFileInfo>
#include <QTextStream>

namespace v8reader {

int CliApplication::run(const Options &options, QTextStream &outStream,
                        QTextStream &errStream)
{
    cli::setStreams(&outStream, &errStream);

    const QString command = options.command.toLower();
    if (command.isEmpty() || command == QStringLiteral("help")) {
        cli::commandHelp();
        return command.isEmpty() ? 1 : 0;
    }

    if (options.path.isEmpty()) {
        cli::err() << QStringLiteral("Ошибка: не указан файл конфигурации\n");
        cli::commandHelp();
        return 1;
    }

    if (!QFileInfo::exists(options.path)) {
        cli::err() << QStringLiteral("Ошибка: файл «%1» не найден\n").arg(options.path);
        return 1;
    }

    if (command == QStringLiteral("info"))
        return cli::commandInfo(options.path);
    if (command == QStringLiteral("list"))
        return cli::commandList(options.path);
    if (command == QStringLiteral("tree"))
        return cli::commandTree(options.path);
    if (command == QStringLiteral("dump")) {
        if (options.argument.isEmpty()) {
            cli::err() << QStringLiteral("Ошибка: не указан элемент контейнера\n");
            return 1;
        }
        return cli::commandDump(options.path, options.argument, options.output,
                                options.raw);
    }
    if (command == QStringLiteral("metadata"))
        return cli::commandMetadata(options.path, options.argument, options.output);
    if (command == QStringLiteral("modules"))
        return cli::commandModules(options.path, options.argument, options.output);
    if (command == QStringLiteral("composition"))
        return cli::commandComposition(options.path, options.argument);
    if (command == QStringLiteral("view"))
        return cli::commandView(options.path, options.argument, options.module,
                                options.json);
    if (command == QStringLiteral("form-model"))
        return cli::commandFormModel(options.path, options.argument, options.form,
                                     options.output, options.items, options.nodes);
    if (command == QStringLiteral("mxl")) {
        cli::MxlCommandOptions mxlOptions;
        mxlOptions.templateName = options.templateName;
        mxlOptions.htmlOutput = options.htmlOutput;
        mxlOptions.json = options.json;
        return cli::commandMxl(options.path, options.argument, options.output,
                               mxlOptions);
    }
    if (command == QStringLiteral("extract"))
        return cli::commandExtract(options.path, options.argument, options.output);
    if (command == QStringLiteral("extract-all"))
        return cli::commandExtractAll(options.path, options.output);

    cli::err() << QStringLiteral("Ошибка: неизвестная команда «%1»\n").arg(command);
    cli::commandHelp();
    return 1;
}

} // namespace v8reader
