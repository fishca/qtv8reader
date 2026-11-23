#ifndef QTV8READER_TESTPATHS_H
#define QTV8READER_TESTPATHS_H

#include <QFileInfo>
#include <QString>
#include <QStringList>

namespace v8reader {
namespace test {

// Каталог (или сам файл) из QTV8_TEST_CORPUS: так указывается набор образцов
// на машине, где пути разработчика не совпадают. Пустая строка — переменная
// не задана или файл в ней не найден.
inline QString corpusFromEnvironment(const QString &fileName)
{
    const QString env = qEnvironmentVariable("QTV8_TEST_CORPUS");
    if (env.isEmpty())
        return QString();
    if (QFileInfo(env).isFile())
        return env;
    const QString fromEnv = env + QLatin1Char('/') + fileName;
    return QFileInfo::exists(fromEnv) ? fromEnv : QString();
}

// Образец для тестов, которым нужен настоящий контейнер 1С: сначала
// QTV8_TEST_CORPUS, затем известные каталоги, где образцы лежат на рабочих
// машинах разработчиков. Пустая строка означает «образец не найден»: тест
// пропускается через QSKIP.
inline QString corpusFile(const QString &fileName)
{
    const QString fromEnv = corpusFromEnvironment(fileName);
    if (!fromEnv.isEmpty())
        return fromEnv;

    const QStringList dirs = {
        QStringLiteral("E:/work/dt_cf"),
        QStringLiteral("E:/work/1с"),
        QStringLiteral("E:/work/1с/Обработки"),
        QStringLiteral("E:/work/repo_github/yellow-watcher/1c-extention"),
        QStringLiteral("C:/work/cpp/utils"),
        QStringLiteral("C:/work/cpp/Projects/_e8unpack_/release_package"),
        QStringLiteral("C:/work/1с"),
        QStringLiteral("C:/work/1с/Обработки"),
        QStringLiteral("C:/work/repo_github/yellow_watcher/1c-extention"),
    };

    for (const QString &dir : dirs) {
        const QString candidate = dir + QLatin1Char('/') + fileName;
        if (QFileInfo::exists(candidate))
            return candidate;
    }

    return QString();
}

// Образец, на свойства которого завязаны точные проверки (число элементов,
// версия, GUID): он ищется только через QTV8_TEST_CORPUS, потому что файл
// с тем же именем в другом каталоге может быть другой сборкой.
inline QString exactSampleFile(const QString &fileName)
{
    return corpusFromEnvironment(fileName);
}

} // namespace test
} // namespace v8reader

#endif // QTV8READER_TESTPATHS_H
