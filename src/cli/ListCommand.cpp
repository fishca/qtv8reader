#include "cli/CommandCommon.h"

namespace v8reader::cli {

int commandList(const QString &path)
{
    ConfigurationReader reader;
    if (!loadConfiguration(path, &reader))
        return 1;

    const Configuration &configuration = reader.configuration();
    out() << QStringLiteral("%1 (%2), объектов: %3\n\n")
                 .arg(configuration.displayName(), configuration.formatLabel)
                 .arg(configuration.objectCount());

    for (const MetadataCategory &category : configuration.categories) {
        out() << QStringLiteral("[%1] %2 объектов\n")
                     .arg(category.title)
                     .arg(category.objects.size());
        for (const MetadataObject &object : category.objects) {
            out() << QStringLiteral("  %1").arg(object.displayName());
            if (!object.synonym.isEmpty())
                out() << QStringLiteral(" / %1").arg(object.synonym);
            out() << QStringLiteral(" [%1]").arg(object.guid);
            if (!object.elementFound)
                out() << QStringLiteral(" (элемент не найден)");
            out() << QLatin1Char('\n');
        }
    }
    return 0;
}


} // namespace v8reader::cli
