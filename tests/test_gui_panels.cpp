#include "gui/BslHighlightSettings.h"
#include "gui/FormView.h"
#include "gui/HighlightSettingsDialog.h"
#include "gui/ModulePanel.h"
#include "gui/PropertiesPanel.h"
#include "gui/RecentFilesController.h"
#include "gui/ThemeController.h"
#include "gui/ExtractionController.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTest>

using namespace v8reader;

class TestGuiPanels : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void themePersists();
    void modulePanelSwitches();
    void propertiesAreReadOnly();
    void recentFilesOpenAndMissing();
    void extractionMessages();
    void formViewModuleTab();
    void highlightSettingsDialog();
};

void TestGuiPanels::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("qtv8reader-gui-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("qtv8reader-gui-tests"));
    QSettings settings;
    settings.clear();
}

void TestGuiPanels::themePersists()
{
    ThemeController controller;
    QSignalSpy spy(&controller, &ThemeController::darkModeChanged);

    controller.apply(QStringLiteral("dark"), true);
    QVERIFY(controller.isDark());
    QCOMPARE(spy.count(), 1);
    QSettings settings;
    QCOMPARE(settings.value(QStringLiteral("MainWindow/theme")).toString(),
             QStringLiteral("dark"));

    controller.apply(QStringLiteral("light"), true);
    QVERIFY(!controller.isDark());
    QCOMPARE(spy.count(), 2);
    QCOMPARE(settings.value(QStringLiteral("MainWindow/theme")).toString(),
             QStringLiteral("light"));
}

void TestGuiPanels::modulePanelSwitches()
{
    ModulePanel panel;
    ModuleInfo first;
    first.name = QStringLiteral("Модуль объекта");
    first.elementName = QStringLiteral("guid.0");
    first.text = QStringLiteral("// first");
    ModuleInfo second;
    second.name = QStringLiteral("Модуль менеджера");
    second.elementName = QStringLiteral("guid.1");
    second.text = QStringLiteral("// second");

    panel.setModules({first, second});
    QCOMPARE(panel.currentIndex(), 0);
    QVERIFY(panel.text().contains(QStringLiteral("first")));

    panel.setCurrentIndex(1);
    QVERIFY(panel.text().contains(QStringLiteral("second")));

    panel.setPlaceholder(QStringLiteral("нет модуля"));
    QCOMPARE(panel.text(), QStringLiteral("нет модуля"));
    QCOMPARE(panel.currentIndex(), -1);
}

void TestGuiPanels::propertiesAreReadOnly()
{
    PropertiesPanel panel;
    panel.setRows({{QStringLiteral("Имя"), QStringLiteral("Справочник1")},
                   {QStringLiteral("Тип"), QStringLiteral("Справочник")}});

    QCOMPARE(panel.rowCount(), 2);
    QCOMPARE(panel.item(0, 0)->text(), QStringLiteral("Имя"));
    QCOMPARE(panel.item(0, 1)->text(), QStringLiteral("Справочник1"));
    QVERIFY(!(panel.item(0, 0)->flags() & Qt::ItemIsEditable));
}

void TestGuiPanels::recentFilesOpenAndMissing()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString existing = temp.filePath(QStringLiteral("recent.cf"));
    {
        QFile file(existing);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
    }

    QMenu menu;
    RecentFilesController controller(&menu);
    QSignalSpy openSpy(&controller, &RecentFilesController::openRequested);
    QSignalSpy missingSpy(&controller, &RecentFilesController::missingFile);

    controller.add(existing);
    controller.add(QStringLiteral("C:/no/such/recent-file.cf"));
    QCOMPARE(controller.files().size(), 2);

    QAction *first = nullptr;
    QAction *missing = nullptr;
    for (QAction *action : menu.actions()) {
        if (action->data().toString() == existing)
            first = action;
        if (action->data().toString().contains(QStringLiteral("recent-file.cf")))
            missing = action;
    }
    QVERIFY(first);
    first->trigger();
    QCOMPARE(openSpy.count(), 1);
    QCOMPARE(openSpy.at(0).at(0).toString(), existing);

    QVERIFY(missing);
    missing->trigger();
    QCOMPARE(missingSpy.count(), 1);
    QCOMPARE(controller.files().size(), 1);
}

void TestGuiPanels::extractionMessages()
{
    MetadataObject object;
    object.name = QStringLiteral("Справочник1");
    ExtractionReport report;
    report.outputDirectory = QStringLiteral("C:/out");
    report.files.append({QStringLiteral("C:/out/info.txt"), 10});
    report.moduleCount = 1;
    report.formCount = 2;
    report.templateCount = 3;
    report.warnings.append(QStringLiteral("warning"));

    const QString objectMessage =
        ExtractionController::objectResultMessage(object, report);
    QVERIFY(objectMessage.contains(QStringLiteral("Справочник1")));
    QVERIFY(objectMessage.contains(QStringLiteral("warning")));
    QVERIFY(objectMessage.contains(QStringLiteral("C:/out")));

    const QString allMessage = ExtractionController::allResultMessage(report);
    QVERIFY(allMessage.contains(QStringLiteral("Сохранено файлов: 1")));
    QVERIFY(allMessage.contains(QStringLiteral("warning")));
}

void TestGuiPanels::formViewModuleTab()
{
    FormDocument document;
    document.name = QStringLiteral("Форма1");
    document.synonym = QStringLiteral("Форма 1");
    document.moduleText = QStringLiteral("Процедура ПриСозданииОбъекта()\nКонецПроцедуры");

    FormView view;
    view.setDocument(document);

    auto *tabs = view.findChild<QTabWidget *>(QStringLiteral("formTabs"));
    auto *moduleEdit = view.findChild<QPlainTextEdit *>(QStringLiteral("formModuleEdit"));
    auto *tree = view.findChild<QTreeWidget *>(QStringLiteral("formTree"));
    QVERIFY(tabs);
    QVERIFY(moduleEdit);
    QVERIFY(tree);
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(tabs->tabText(1), QStringLiteral("Модуль"));
    QCOMPARE(moduleEdit->toPlainText(), document.moduleText);
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QVERIFY(tree->topLevelItem(i)->text(0) != QStringLiteral("Модуль формы"));
    }
}

void TestGuiPanels::highlightSettingsDialog()
{
    BslHighlightSettings defaults = BslHighlightSettings::defaults(false);
    HighlightSettingsDialog dialog(defaults, false);

    auto *enabled = dialog.findChild<QCheckBox *>(QStringLiteral("highlightEnabled"));
    auto *bold = dialog.findChild<QCheckBox *>(QStringLiteral("highlightBoldKeywords"));
    QVERIFY(enabled);
    QVERIFY(bold);
    enabled->setChecked(false);
    bold->setChecked(false);

    const BslHighlightSettings result = dialog.settings();
    QVERIFY(!result.enabled);
    QVERIFY(!result.boldKeywords);

    QSettings settings;
    result.save(settings);
    const BslHighlightSettings loaded = BslHighlightSettings::load(settings, false);
    QVERIFY(!loaded.enabled);
    QVERIFY(!loaded.boldKeywords);
}

QTEST_MAIN(TestGuiPanels)
#include "test_gui_panels.moc"
