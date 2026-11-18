#include "cli/CliApplication.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifndef QTV8READER_VERSION
#define QTV8READER_VERSION "0.0.0"
#endif

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("v8reader_cli"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QTV8READER_VERSION));

#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    QTextStream out(stdout);
    QTextStream err(stderr);
    out.setEncoding(QStringConverter::Utf8);
    err.setEncoding(QStringConverter::Utf8);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Просмотр конфигураций и внешних обработок 1С:Предприятие 8"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(
        {QStringLiteral("o"), QStringLiteral("output")},
        QStringLiteral("Каталог или файл для сохранения"), QStringLiteral("путь")));
    parser.addOption(QCommandLineOption(
        QStringLiteral("raw"), QStringLiteral("Не распаковывать данные элемента")));
    parser.addPositionalArgument(QStringLiteral("команда"),
                                 QStringLiteral("Команда: info, list, tree, dump, metadata, "
                                                "modules, composition, mxl, extract, extract-all"));
    parser.addPositionalArgument(QStringLiteral("файл"),
                                 QStringLiteral("Файл .cf, .cfe, .epf или .erf"));
    parser.addPositionalArgument(QStringLiteral("объект"),
                                 QStringLiteral("GUID, имя или синоним объекта (для dump — "
                                                "имя элемента контейнера)"),
                                 QStringLiteral("[объект]"));

    parser.process(app);

    const QStringList positional = parser.positionalArguments();
    v8reader::CliApplication::Options options;
    if (!positional.isEmpty())
        options.command = positional.at(0);
    if (positional.size() >= 2)
        options.path = positional.at(1);
    if (positional.size() >= 3)
        options.argument = positional.at(2);
    options.output = parser.value(QStringLiteral("output"));
    options.raw = parser.isSet(QStringLiteral("raw"));

    v8reader::CliApplication application;
    return application.run(options, out, err);
}
