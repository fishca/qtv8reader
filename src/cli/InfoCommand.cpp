#include "cli/CommandCommon.h"

namespace v8reader::cli {

int commandInfo(const QString &path)
{
    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    const Configuration &configuration = reader.configuration();
    out() << QStringLiteral("Конфигурация: %1\n").arg(configuration.displayName());
    out() << QStringLiteral("Тип файла: %1\n").arg(configuration.kindTitle);
    if (!configuration.name.isEmpty())
        out() << QStringLiteral("Имя: %1\n").arg(configuration.name);
    if (!configuration.synonym.isEmpty())
        out() << QStringLiteral("Синоним: %1\n").arg(configuration.synonym);
    if (!configuration.comment.isEmpty())
        out() << QStringLiteral("Комментарий: %1\n").arg(configuration.comment);
    if (!configuration.version.isEmpty())
        out() << QStringLiteral("Версия: %1\n").arg(configuration.version);
    out() << QStringLiteral("Формат: %1\n").arg(configuration.formatLabel);
    out() << QStringLiteral("Файл: %1\n").arg(configuration.sourcePath);
    out() << QStringLiteral("Размер: %1 байт\n").arg(configuration.fileSize);
    out() << QStringLiteral("GUID конфигурации: %1\n").arg(configuration.configGuid);
    out() << QStringLiteral("Категорий: %1\n").arg(configuration.categories.size());
    out() << QStringLiteral("Объектов: %1\n").arg(configuration.objectCount());

    for (const MetadataCategory &category : configuration.categories) {
        out() << QStringLiteral("  %1: %2\n")
                     .arg(category.title)
                     .arg(category.objects.size());
    }

    if (!configuration.warnings.isEmpty()) {
        out() << QStringLiteral("\nПредупреждения:\n");
        for (const QString &warning : configuration.warnings)
            out() << QStringLiteral("  - %1\n").arg(warning);
    }

    return 0;
}


} // namespace v8reader::cli
