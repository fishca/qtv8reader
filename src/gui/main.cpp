#include "gui/MainWindow.h"

#include <QApplication>
#include <QLocale>
#include <QPixmap>
#include <QStringList>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qtv8reader"));
    QCoreApplication::setApplicationName(QStringLiteral("qtv8reader"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.3.0"));

    QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));

    v8reader::MainWindow window;
    window.show();

    bool smokeTest = false;
    QString screenshotPath;
    QString filePath;
    const QStringList arguments = QCoreApplication::arguments();
    for (int i = 1; i < arguments.size(); ++i) {
        const QString argument = arguments.at(i);
        if (argument == QStringLiteral("--smoke")) {
            smokeTest = true;
            continue;
        }
        if (argument == QStringLiteral("--screenshot")
            && i + 1 < arguments.size()) {
            screenshotPath = arguments.at(++i);
            continue;
        }
        if (filePath.isEmpty() && !argument.startsWith(QLatin1Char('-')))
            filePath = argument;
    }

    if (!filePath.isEmpty())
        window.openInitialFile(filePath);

    if (smokeTest || !screenshotPath.isEmpty()) {
        QCoreApplication::processEvents();
        const QString report = window.smokeReport();
        QTextStream out(stdout);
        out.setEncoding(QStringConverter::Utf8);
        out << report << Qt::endl;

        if (!screenshotPath.isEmpty()) {
            const QPixmap shot = window.grab();
            const bool saved = !shot.isNull() && shot.save(screenshotPath);
            out << QStringLiteral("SCREENSHOT %1: %2 (%3x%4)")
                       .arg(saved ? QStringLiteral("OK") : QStringLiteral("FAILED"),
                            screenshotPath)
                       .arg(shot.width())
                       .arg(shot.height())
                << Qt::endl;
        }

        if (!window.isFileOpen())
            return 2;
        out << QStringLiteral("SMOKE OK: %1, объектов: %2")
                   .arg(window.fileKindTitle())
                   .arg(window.objectCount())
            << Qt::endl;
        return 0;
    }

    return application.exec();
}
