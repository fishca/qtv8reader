#include "cli/CommandCommon.h"

namespace v8reader::cli {

int commandTree(const QString &path)
{
    V8Container container;
    QString error;
    if (!container.open(path, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    out() << QStringLiteral("Формат: %1, элементов: %2\n\n")
                 .arg(container.format().label())
                 .arg(container.count());
    out() << QStringLiteral(" №  Имя                                              "
                            "Изменён              Размер   Упак.\n");

    for (int i = 0; i < container.count(); ++i) {
        const V8Element *element = container.at(i);
        if (!element)
            continue;
        QByteArray payload;
        bool compressed = false;
        container.elementPayload(*element, &payload, &compressed, nullptr);
        out() << QStringLiteral("%1  %2  %3  %4  %5\n")
                     .arg(i, 3)
                     .arg(element->name, -48)
                     .arg(dateTimeText(element->modificationDate), -19)
                     .arg(payload.size(), 8)
                     .arg(compressed ? QStringLiteral("да") : QStringLiteral("нет"));
    }
    return 0;
}


} // namespace v8reader::cli
