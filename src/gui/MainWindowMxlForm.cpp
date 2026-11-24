#include "gui/MainWindow.h"

#include "gui/FormView.h"
#include "gui/MetadataIcons.h"
#include "gui/MxlPrinter.h"
#include "gui/MxlView.h"
#include "gui/PreviewDialog.h"
#include "core/ConfigurationReader.h"
#include "core/FormBindings.h"
#include "core/FormOwner.h"
#include "core/FormPictures.h"
#include "core/FormReader.h"
#include "core/FormStyles.h"
#include "core/FormWebModel.h"
#include "core/MxlReader.h"
#include "core/V8Text.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QPrintDialog>
#include <QStatusBar>
#include <QPrinter>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace v8reader {
void MainWindow::loadTemplatePreview(const MetadataEntry &entry)
{
    if (!m_reader || !m_mxlView)
        return;

    const V8Container &container = m_reader->container();
    const V8Element *element = nullptr;
    if (!entry.guid.isEmpty())
        element = container.find(entry.guid + QStringLiteral(".0"));
    if (!element)
        element = container.find(entry.guid);
    if (!element && !entry.name.isEmpty())
        element = container.find(entry.name);
    if (!element && !entry.guid.isEmpty()) {
        const QString prefix = entry.guid + QLatin1Char('.');
        for (const V8Element &candidate : container.elements()) {
            if (candidate.name == entry.guid || candidate.name.startsWith(prefix)) {
                element = &candidate;
                break;
            }
        }
    }
    if (!element) {
        m_mxlView->setMessage(
            QStringLiteral("Макет «%1» не найден в контейнере.").arg(entry.displayName()));
        return;
    }

    QByteArray payload;
    QString error;
    if (!container.elementPayload(*element, &payload, nullptr, &error)) {
        m_mxlView->setMessage(
            QStringLiteral("Не удалось прочитать макет «%1»: %2")
                .arg(entry.displayName(), error));
        return;
    }

    if (!MxlReader::looksLikeMxl(payload)) {
        m_mxlView->setMessage(
            QStringLiteral("Макет «%1» не является табличным документом MXL.")
                .arg(entry.displayName()));
        return;
    }

    mxl::Document document;
    if (!MxlReader::parseBytes(payload, &document, &error)) {
        m_mxlView->setMessage(
            QStringLiteral("Не удалось разобрать макет «%1»: %2")
                .arg(entry.displayName(), error));
        return;
    }

    m_mxlView->setDocument(document);
    m_mxlDocument = document;
    m_mxlDocumentValid = true;
    updateMxlActions();
    const int index = m_tabs->indexOf(m_mxlPage);
    if (index >= 0) {
        m_tabs->setTabText(index,
                           QStringLiteral("Табличный документ (%1×%2)")
                               .arg(document.rowCount())
                               .arg(document.maxColumn() + 1));
        m_tabs->setCurrentIndex(index);
    }
}
void MainWindow::loadFormPreview(const MetadataEntry &entry)
{
    if (!m_reader || !m_formView)
        return;

    const V8Container &container = m_reader->container();
    const V8Element *element = nullptr;
    if (!entry.guid.isEmpty())
        element = container.find(entry.guid + QStringLiteral(".0"));
    if (!element)
        element = container.find(entry.guid);
    if (!element && !entry.name.isEmpty())
        element = container.find(entry.name);
    if (!element && !entry.guid.isEmpty()) {
        const QString prefix = entry.guid + QLatin1Char('.');
        for (const V8Element &candidate : container.elements()) {
            if (candidate.name == entry.guid || candidate.name.startsWith(prefix)) {
                element = &candidate;
                break;
            }
        }
    }
    if (!element) {
        m_formView->setMessage(
            QStringLiteral("Форма «%1» не найдена в контейнере.").arg(entry.displayName()));
        return;
    }

    QByteArray payload;
    QString error;
    if (!container.elementPayload(*element, &payload, nullptr, &error)) {
        m_formView->setMessage(
            QStringLiteral("Не удалось прочитать форму «%1»: %2")
                .arg(entry.displayName(), error));
        return;
    }

    FormDocument document;
    FormBindingResolver resolver;
    if (m_currentObject)
        resolver.addObject(*m_currentObject);
    FormStyleTable styles;
    FormPictureTable pictures;
    if (m_reader) {
        styles.loadFromConfiguration(m_reader->configuration(), m_reader->container());
        pictures.loadFromConfiguration(m_reader->configuration(), m_reader->container());
    }
    if (!FormReader::parseBytes(payload, &document, &error,
                                resolver.isEmpty() ? nullptr : &resolver,
                                &styles, &pictures)) {
        m_formView->setMessage(
            QStringLiteral("Не удалось разобрать форму «%1»: %2")
                .arg(entry.displayName(), error));
        return;
    }

    document.name = entry.name;
    document.synonym = entry.synonym;
    if (m_currentObject) {
        applyOwnerMetadata(*m_currentObject, &document);
        QString objectText;
        if (m_reader->readObjectText(*m_currentObject, &objectText, nullptr))
            applyOwnerStandardAttributes(objectText, &document);
        applyReferencedCatalogMeta(*m_reader, &document);
    }
    m_formDocument = document;
    m_formDocumentValid = true;
    m_formView->setDocument(document);
    // «Точный вид»: браузерный рендерер получает модель формы в JSON.
    m_formView->setExactFormModel(formWebModelJson(document), document.name);

    const int index = m_tabs->indexOf(m_formPage);
    if (index >= 0) {
        m_tabs->setTabText(index,
                           QStringLiteral("Форма (%1)").arg(document.itemCount()));
        m_tabs->setCurrentIndex(index);
    }
}

void MainWindow::updateMxlActions()
{
    if (m_printMxlAction)
        m_printMxlAction->setEnabled(m_mxlDocumentValid);
    if (m_pdfMxlAction)
        m_pdfMxlAction->setEnabled(m_mxlDocumentValid);
}

QString MainWindow::mxlPrintSummary() const
{
    if (!m_mxlDocumentValid)
        return QStringLiteral("нет макета");
    const mxl::PrintSettings &settings = m_mxlDocument.printSettings;
    QString pageSize = settings.pageSize;
    if (pageSize.isEmpty())
        pageSize = QStringLiteral("A4");
    const QString orientation = settings.orientation == 2
        ? QStringLiteral("альбомная") : QStringLiteral("книжная");
    const int scale = settings.scale > 0 ? settings.scale : 100;
    return QStringLiteral("%1, %2, масштаб %3%, поля %4/%5/%6/%7 мм")
        .arg(pageSize, orientation)
        .arg(scale)
        .arg(settings.topMarginMm)
        .arg(settings.bottomMarginMm)
        .arg(settings.leftMarginMm)
        .arg(settings.rightMarginMm);
}

void MainWindow::printMxl()
{
    if (!m_mxlDocumentValid)
        return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(QStringLiteral("MXL"));
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(QStringLiteral("Печать табличного документа"));
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = MxlPrinter::print(m_mxlDocument, printer, &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::critical(this, QStringLiteral("Ошибка печати"), error);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Документ отправлен на печать"));
}

void MainWindow::exportMxlPdf()
{
    if (!m_mxlDocumentValid)
        return;

    QString defaultPath = m_currentPath;
    if (!defaultPath.isEmpty()) {
        QFileInfo info(defaultPath);
        defaultPath = info.absolutePath() + QLatin1Char('/') + info.completeBaseName()
            + QStringLiteral("-макет.pdf");
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Экспорт макета в PDF"), defaultPath,
        QStringLiteral("PDF (*.pdf)"));
    if (path.isEmpty())
        return;
    if (!exportCurrentMxlPdf(path))
        return;
    statusBar()->showMessage(QStringLiteral("PDF сохранён: %1").arg(path));
}

bool MainWindow::exportCurrentMxlPdf(const QString &path)
{
    if (!m_mxlDocumentValid)
        return false;

    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = MxlPrinter::exportPdf(m_mxlDocument, path, &error);
    QApplication::restoreOverrideCursor();
    return ok;
}

bool MainWindow::selectFirstTemplate()
{
    if (!m_reader)
        return false;

    Configuration &configuration = m_reader->configuration();
    for (MetadataCategory &category : configuration.categories) {
        for (MetadataObject &object : category.objects) {
            if (selectTemplateForObject(object.guid))
                return true;
        }
    }
    return false;
}

bool MainWindow::selectTemplateForObject(const QString &query)
{
    if (!m_reader)
        return false;

    Configuration &configuration = m_reader->configuration();
    for (MetadataCategory &category : configuration.categories) {
        for (MetadataObject &object : category.objects) {
            if (!query.isEmpty()) {
                const bool matches = object.guid.compare(query, Qt::CaseInsensitive) == 0
                    || object.name.compare(query, Qt::CaseInsensitive) == 0
                    || object.synonym.compare(query, Qt::CaseInsensitive) == 0
                    || object.displayName().compare(query, Qt::CaseInsensitive) == 0;
                if (!matches)
                    continue;
            }

            m_reader->loadSections(object);
            const MetadataSection *section =
                object.section(MetadataSectionKind::Templates);
            if (!section || section->entries.isEmpty())
                continue;

            ensureObjectItemChildren(object.guid);
            QTreeWidgetItem *objectItem = findObjectItem(object.guid);
            if (!objectItem)
                continue;
            QTreeWidgetItem *sectionItem = nullptr;
            for (int i = 0; i < objectItem->childCount(); ++i) {
                QTreeWidgetItem *child = objectItem->child(i);
                if (child->data(0, KindRole).toInt() == SectionItem
                    && child->data(0, SectionKindRole).toInt()
                        == static_cast<int>(MetadataSectionKind::Templates)) {
                    sectionItem = child;
                    break;
                }
            }
            if (!sectionItem)
                continue;
            sectionItem->setExpanded(true);
            if (sectionItem->childCount() > 0) {
                QTreeWidgetItem *entryItem = sectionItem->child(0);
                m_tree->setCurrentItem(entryItem);
                m_tree->scrollToItem(entryItem);
                QCoreApplication::processEvents();
                return m_mxlDocumentValid;
            }
        }
    }
    return false;
}

bool MainWindow::selectFormForObject(const QString &objectQuery,
                                     const QString &formQuery)
{
    if (!m_reader)
        return false;

    Configuration &configuration = m_reader->configuration();
    // В запросе вида «Документ.АвансовыйОтчет» вид отделяется точкой и только
    // уточняет поиск: с именем объекта он не сравнивается. Вид принимается и
    // в единственном числе, и как название категории («Документы»).
    QString nameQuery = objectQuery;
    if (const int dot = objectQuery.indexOf(QLatin1Char('.')); dot > 0) {
        const QString prefix = objectQuery.left(dot).trimmed();
        for (const MetadataCategory &category : configuration.categories) {
            if (category.title.compare(prefix, Qt::CaseInsensitive) == 0
                || category.typeTitle.compare(prefix, Qt::CaseInsensitive) == 0) {
                nameQuery = objectQuery.mid(dot + 1).trimmed();
                break;
            }
        }
    }
    for (MetadataCategory &category : configuration.categories) {
        for (MetadataObject &object : category.objects) {
            if (!objectQuery.isEmpty()) {
                const bool matches = object.guid.compare(objectQuery, Qt::CaseInsensitive) == 0
                    || object.name.compare(nameQuery, Qt::CaseInsensitive) == 0
                    || object.synonym.compare(nameQuery, Qt::CaseInsensitive) == 0
                    || object.displayName().compare(objectQuery, Qt::CaseInsensitive) == 0;
                if (!matches)
                    continue;
            }

            m_reader->loadSections(object);
            const MetadataSection *section =
                object.section(MetadataSectionKind::Forms);
            if (!section || section->entries.isEmpty())
                continue;

            int entryIndex = -1;
            for (int i = 0; i < section->entries.size(); ++i) {
                const MetadataEntry &entry = section->entries.at(i);
                if (formQuery.isEmpty()
                    || entry.name.compare(formQuery, Qt::CaseInsensitive) == 0
                    || entry.synonym.compare(formQuery, Qt::CaseInsensitive) == 0
                    || entry.displayName().compare(formQuery, Qt::CaseInsensitive) == 0) {
                    entryIndex = i;
                    break;
                }
            }
            if (entryIndex < 0)
                continue;

            ensureObjectItemChildren(object.guid);
            QTreeWidgetItem *objectItem = findObjectItem(object.guid);
            if (!objectItem)
                continue;
            QTreeWidgetItem *sectionItem = nullptr;
            for (int i = 0; i < objectItem->childCount(); ++i) {
                QTreeWidgetItem *child = objectItem->child(i);
                if (child->data(0, KindRole).toInt() == SectionItem
                    && child->data(0, SectionKindRole).toInt()
                        == static_cast<int>(MetadataSectionKind::Forms)) {
                    sectionItem = child;
                    break;
                }
            }
            if (!sectionItem)
                continue;
            sectionItem->setExpanded(true);
            if (entryIndex >= sectionItem->childCount())
                continue;
            QTreeWidgetItem *entryItem = sectionItem->child(entryIndex);
            m_tree->setCurrentItem(entryItem);
            m_tree->scrollToItem(entryItem);
            QCoreApplication::processEvents();
            return m_formDocumentValid;
        }
    }
    return false;
}

void MainWindow::setFormScale(int percent)
{
    if (!m_formView)
        return;
    m_formView->setScalePercent(percent);
}

void MainWindow::setShowNamedAreas(bool show)
{
    if (m_showAreasCheck)
        m_showAreasCheck->setChecked(show);
    if (m_mxlView)
        m_mxlView->setShowNamedAreas(show);
}

bool MainWindow::exportMxlFileToPdf(const QString &mxlPath, const QString &pdfPath)
{
    mxl::Document document;
    QString error;
    if (!MxlReader::parseFile(mxlPath, &document, &error)) {
        QMessageBox::critical(this, QStringLiteral("Ошибка разбора MXL"), error);
        return false;
    }
    m_mxlDocument = document;
    m_mxlDocumentValid = true;
    if (m_mxlView)
        m_mxlView->setDocument(document);
    updateMxlActions();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = MxlPrinter::exportPdf(document, pdfPath, &error);
    QApplication::restoreOverrideCursor();
    return ok;
}


} // namespace v8reader