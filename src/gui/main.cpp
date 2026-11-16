#include "gui/MainWindow.h"

#include <QApplication>
#include <QLocale>
#include <QPixmap>
#include <QStringList>
#include <QTextStream>

#ifndef QTV8READER_VERSION
#define QTV8READER_VERSION "0.0.0"
#endif

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qtv8reader"));
    QCoreApplication::setApplicationName(QStringLiteral("qtv8reader"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QTV8READER_VERSION));

    QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));

    v8reader::MainWindow window;
    window.show();

    bool smokeTest = false;
    bool expandCommon = false;
    bool expandDocuments = false;
    QString screenshotPath;
    QString mxlPdfPath;
    QString mxlObject;
    QString selectMxl;
    QString selectForm;
    int formScale = 0;
    bool showAreas = false;
    QString filePath;
    const QStringList arguments = QCoreApplication::arguments();
    for (int i = 1; i < arguments.size(); ++i) {
        const QString argument = arguments.at(i);
        if (argument == QStringLiteral("--smoke")) {
            smokeTest = true;
            continue;
        }
        if (argument == QStringLiteral("--expand-common")) {
            expandCommon = true;
            continue;
        }
        if (argument == QStringLiteral("--expand-documents")) {
            expandDocuments = true;
            continue;
        }
        if (argument == QStringLiteral("--screenshot")
            && i + 1 < arguments.size()) {
            screenshotPath = arguments.at(++i);
            continue;
        }
        if (argument == QStringLiteral("--mxl-pdf")
            && i + 1 < arguments.size()) {
            mxlPdfPath = arguments.at(++i);
            continue;
        }
        if (argument == QStringLiteral("--mxl-object")
            && i + 1 < arguments.size()) {
            mxlObject = arguments.at(++i);
            continue;
        }
        if (argument == QStringLiteral("--show-areas")) {
            showAreas = true;
            continue;
        }
        if (argument == QStringLiteral("--select-mxl")
            && i + 1 < arguments.size()) {
            selectMxl = arguments.at(++i);
            continue;
        }
        if (argument == QStringLiteral("--select-form")
            && i + 1 < arguments.size()) {
            selectForm = arguments.at(++i);
            continue;
        }
        if (argument == QStringLiteral("--form-scale")
            && i + 1 < arguments.size()) {
            formScale = arguments.at(++i).toInt();
            continue;
        }
        if (filePath.isEmpty() && !argument.startsWith(QLatin1Char('-')))
            filePath = argument;
    }

    const bool standaloneMxl = filePath.endsWith(QStringLiteral(".mxl"),
                                                 Qt::CaseInsensitive);
    if (!filePath.isEmpty() && !standaloneMxl)
        window.openInitialFile(filePath);

    if (!selectMxl.isEmpty() && window.isFileOpen())
        window.selectTemplateForObject(selectMxl);

    if (!selectForm.isEmpty() && window.isFileOpen()) {
        QString objectQuery = selectForm;
        QString formQuery;
        const int separator = selectForm.indexOf(QLatin1Char(':'));
        if (separator > 0) {
            objectQuery = selectForm.left(separator);
            formQuery = selectForm.mid(separator + 1);
        }
        window.selectFormForObject(objectQuery, formQuery);
        if (formScale > 0)
            window.setFormScale(formScale);
    }

    if (expandCommon)
        window.expandCommonBranch();
    if (expandDocuments)
        window.expandDocumentsBranch();
    if (showAreas)
        window.setShowNamedAreas(true);

    if (!mxlPdfPath.isEmpty()) {
        QTextStream out(stdout);
        out.setEncoding(QStringConverter::Utf8);
        if (filePath.endsWith(QStringLiteral(".mxl"), Qt::CaseInsensitive)) {
            const bool exported = window.exportMxlFileToPdf(filePath, mxlPdfPath);
            out << QStringLiteral("MXL PDF: %1 (%2)\n")
                       .arg(exported ? QStringLiteral("OK") : QStringLiteral("FAILED"),
                            mxlPdfPath);
            return exported ? 0 : 1;
        }

        if (!window.isFileOpen()) {
            out << QStringLiteral("MXL PDF: файл не открыт\n");
            return 1;
        }
        const bool selected = mxlObject.isEmpty()
            ? window.selectFirstTemplate()
            : window.selectTemplateForObject(mxlObject);
        if (!selected) {
            out << QStringLiteral("MXL PDF: макет не найден\n");
            return 1;
        }
        const bool exported = window.exportCurrentMxlPdf(mxlPdfPath);
        out << QStringLiteral("MXL PDF: %1 (%2)\n")
                   .arg(exported ? QStringLiteral("OK") : QStringLiteral("FAILED"),
                        mxlPdfPath);
        return exported ? 0 : 1;
    }

    if (smokeTest || !screenshotPath.isEmpty()) {
        QCoreApplication::processEvents();
        QTextStream out(stdout);
        out.setEncoding(QStringConverter::Utf8);

        if (smokeTest) {
            out << window.smokeReport() << Qt::endl;
            if (!window.isFileOpen())
                return 2;
            out << QStringLiteral("SMOKE OK: %1, объектов: %2")
                       .arg(window.fileKindTitle())
                       .arg(window.objectCount())
                << Qt::endl;
        }

        if (!screenshotPath.isEmpty()) {
            QCoreApplication::processEvents();
            const QPixmap shot = window.grab();
            const bool saved = !shot.isNull() && shot.save(screenshotPath);
            out << QStringLiteral("SCREENSHOT %1: %2 (%3x%4)")
                       .arg(saved ? QStringLiteral("OK") : QStringLiteral("FAILED"),
                            screenshotPath)
                       .arg(shot.width())
                       .arg(shot.height())
                << Qt::endl;
        }

        return 0;
    }

    return application.exec();
}
