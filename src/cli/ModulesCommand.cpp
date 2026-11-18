#include "cli/CommandCommon.h"

namespace v8reader::cli {

int commandModules(const QString &path, const QString &objectQuery,
                   const QString &output)
{
    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    // Без объекта — корневые модули конфигурации.
    if (objectQuery.trimmed().isEmpty()) {
        const QVector<ModuleInfo> rootModules = reader.configurationModules();
        if (rootModules.isEmpty()) {
            out() << QStringLiteral("Корневые модули конфигурации не найдены\n");
            return 0;
        }
        QString text;
        for (int i = 0; i < rootModules.size(); ++i) {
            const ModuleInfo &module = rootModules.at(i);
            text += QStringLiteral("// === %1 (%2) ===\n")
                        .arg(module.name, module.elementName);
            text += module.text;
            if (!module.text.endsWith(QLatin1Char('\n')))
                text += QLatin1Char('\n');
            if (i + 1 < rootModules.size())
                text += QLatin1Char('\n');
        }
        if (!output.isEmpty()) {
            if (!writeOutputFile(output, text.toUtf8(), nullptr)) {
                err() << QStringLiteral("Ошибка записи файла %1\n").arg(output);
                return 1;
            }
            out() << QStringLiteral("Сохранено: %1 (%2 байт, модулей: %3)\n")
                         .arg(output)
                         .arg(text.toUtf8().size())
                         .arg(rootModules.size());
            return 0;
        }
        out() << text;
        return 0;
    }

    QString error;
    MetadataObject *object = findObject(reader.configuration(), objectQuery, &error);
    if (!object) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    reader.loadSections(*object);

    const ModuleExtractor extractor(reader.container());
    const QVector<ModuleInfo> modules = extractor.extractForObject(*object);
    if (modules.isEmpty()) {
        out() << QStringLiteral("Модули объекта «%1» не найдены\n")
                     .arg(object->displayName());
        return 0;
    }

    QString text;
    for (int i = 0; i < modules.size(); ++i) {
        const ModuleInfo &module = modules.at(i);
        text += QStringLiteral("// === %1 (%2) ===\n")
                    .arg(module.name, module.elementName);
        text += module.text;
        if (!module.text.endsWith('\n'))
            text += QLatin1Char('\n');
        if (i + 1 < modules.size())
            text += QLatin1Char('\n');
    }

    if (!output.isEmpty()) {
        if (!writeOutputFile(output, text.toUtf8(), &error)) {
            err() << QStringLiteral("Ошибка: %1\n").arg(error);
            return 1;
        }
        out() << QStringLiteral("Сохранено: %1 (%2 байт, модулей: %3)\n")
                     .arg(output)
                     .arg(text.toUtf8().size())
                     .arg(modules.size());
        return 0;
    }

    out() << text;
    return 0;
}


} // namespace v8reader::cli
