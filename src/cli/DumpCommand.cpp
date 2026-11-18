#include "cli/CommandCommon.h"

namespace v8reader::cli {

int commandDump(const QString &path, const QString &elementName,
                const QString &output, bool raw)
{
    V8Container container;
    QString error;
    if (!container.open(path, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    const V8Element *element = container.find(elementName);
    if (!element) {
        err() << QStringLiteral("Элемент «%1» не найден\n").arg(elementName);
        return 1;
    }

    QByteArray data;
    const bool ok = raw
        ? container.elementData(*element, &data, &error)
        : container.elementPayload(*element, &data, nullptr, &error);
    if (!ok) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }

    if (output.isEmpty()) {
        if (isProbablyText(data)) {
            out() << V8Text::decode(data, nullptr, nullptr);
            if (!data.endsWith('\n'))
                out() << QLatin1Char('\n');
        } else {
            out() << QStringLiteral("Двоичные данные, %1 байт. Укажите -o для сохранения.\n")
                         .arg(data.size());
            const int sample = static_cast<int>(std::min<qint64>(data.size(), 256));
            QString hex;
            for (int i = 0; i < sample; ++i) {
                hex += QStringLiteral("%1 ").arg(
                    static_cast<unsigned char>(data.at(i)), 2, 16, QLatin1Char('0'));
                if ((i + 1) % 16 == 0)
                    hex += QLatin1Char('\n');
            }
            out() << hex << QLatin1Char('\n');
        }
        return 0;
    }

    QString target = output;
    if (QFileInfo(output).isDir() || output.endsWith('/') || output.endsWith('\\'))
        target = QDir(output).filePath(element->name);

    if (!writeOutputFile(target, data, &error)) {
        err() << QStringLiteral("Ошибка: %1\n").arg(error);
        return 1;
    }
    out() << QStringLiteral("Сохранено: %1 (%2 байт)\n").arg(target).arg(data.size());
    return 0;
}


} // namespace v8reader::cli
