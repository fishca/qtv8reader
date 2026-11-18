#include "gui/ModulePanel.h"
#include "gui/PropertiesPanel.h"
#include "gui/ThemeController.h"

#include <QApplication>
#include <QSettings>
#include <QSignalSpy>
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

QTEST_MAIN(TestGuiPanels)
#include "test_gui_panels.moc"
