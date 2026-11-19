#include "ControlGallery.h"

#include "ButtonControl.h"
#include "FieldControl.h"
#include "FormStyle.h"
#include "SearchControl.h"
#include "StaticControl.h"
#include "TabControl.h"
#include "TableControl.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPixmap>

namespace controls {

QPixmap renderControlGallery()
{
    QPixmap pixmap(900, 600);
    pixmap.fill(Qt::white);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, false);

    QFont base(QStringLiteral("Arial"));
    base.setPointSize(9);
    QFont headingFont = base;
    headingFont.setBold(true);
    headingFont.setPointSize(10);
    QFont titleFont = base;
    titleFont.setBold(true);
    titleFont.setPointSize(14);

    painter.setFont(titleFont);
    painter.setPen(FormStyle::text);
    painter.drawText(QRectF(24, 14, 500, 28), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Библиотека 1С-контролов"));
    painter.setFont(base);
    painter.setPen(FormStyle::secondaryText);
    painter.drawText(QRectF(24, 38, 700, 20), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Кнопки, поля, поиск, закладки и таблица в стиле управляемой формы 1С:8.3"));

    auto section = [&](const QString &text, double y) {
        painter.setFont(headingFont);
        painter.setPen(FormStyle::text);
        painter.drawText(QRectF(24, y, 400, 20), Qt::AlignLeft | Qt::AlignVCenter,
                         text);
        painter.setFont(base);
    };

    QFontMetricsF metrics(base);
    painter.setFont(base);

    section(QStringLiteral("Кнопки"), 66);
    ButtonVisual primary;
    primary.rect = QRectF(24, 88, 170, FormStyle::buttonHeight);
    primary.text = QStringLiteral("Провести и закрыть");
    primary.primary = true;
    paintButton(painter, primary, metrics);

    ButtonVisual write;
    write.rect = QRectF(202, 88, 90, FormStyle::buttonHeight);
    write.text = QStringLiteral("Записать");
    paintButton(painter, write, metrics);

    ButtonVisual more;
    more.rect = QRectF(300, 88, 64, FormStyle::buttonHeight);
    more.text = QStringLiteral("Еще");
    more.bordered = true;
    more.dropDown = true;
    paintButton(painter, more, metrics);

    ButtonVisual help;
    help.rect = QRectF(372, 88, 32, FormStyle::buttonHeight);
    help.icon = ButtonIcon::Help;
    help.iconOnly = true;
    help.bordered = true;
    paintButton(painter, help, metrics);

    ButtonVisual up;
    up.rect = QRectF(412, 88, 26, FormStyle::buttonHeight);
    up.icon = ButtonIcon::Up;
    up.iconOnly = true;
    up.bordered = true;
    paintButton(painter, up, metrics);

    ButtonVisual down;
    down.rect = QRectF(444, 88, 26, FormStyle::buttonHeight);
    down.icon = ButtonIcon::Down;
    down.iconOnly = true;
    down.bordered = true;
    paintButton(painter, down, metrics);

    section(QStringLiteral("Поля"), 124);
    FieldVisual organization;
    organization.rect = QRectF(24, 146, 330, 28);
    organization.label = QStringLiteral("Организация");
    organization.choiceButton = true;
    paintField(painter, organization, metrics);

    FieldVisual date;
    date.rect = QRectF(366, 146, 220, 28);
    date.label = QStringLiteral("Дата");
    date.calendar = true;
    paintField(painter, date, metrics);

    FieldVisual kind;
    kind.rect = QRectF(24, 180, 330, 28);
    kind.label = QStringLiteral("Вид операции");
    kind.dropDown = true;
    paintField(painter, kind, metrics);

    FieldVisual check;
    check.rect = QRectF(366, 180, 220, 28);
    check.label = QStringLiteral("Быстрый отбор");
    check.checkBox = true;
    paintField(painter, check, metrics);

    section(QStringLiteral("Поиск"), 218);
    SearchVisual search;
    search.rect = QRectF(24, 240, 300, FormStyle::buttonHeight);
    search.placeholder = QStringLiteral("Поиск (Ctrl+F)");
    paintSearch(painter, search);
    paintSearchOptions(painter, QRectF(330, 240, 40, FormStyle::buttonHeight));

    section(QStringLiteral("Закладки"), 278);
    TabVisual firstTab;
    firstTab.rect = QRectF(24, 300, 130, 26);
    firstTab.title = QStringLiteral("Авансы");
    firstTab.selected = true;
    paintTab(painter, firstTab, metrics);
    TabVisual secondTab;
    secondTab.rect = QRectF(154, 300, 130, 26);
    secondTab.title = QStringLiteral("Товары");
    paintTab(painter, secondTab, metrics);
    TabVisual thirdTab;
    thirdTab.rect = QRectF(284, 300, 130, 26);
    thirdTab.title = QStringLiteral("Прочее");
    paintTab(painter, thirdTab, metrics);

    section(QStringLiteral("Таблица"), 338);
    TableVisual table;
    table.rect = QRectF(24, 360, 760, 200);
    table.columns = {QStringLiteral("Дата"), QStringLiteral("Номер"),
                     QStringLiteral("Подотчетное лицо"), QStringLiteral("Сумма")};
    table.widths = {140.0, 100.0, 220.0, 300.0};
    paintTable(painter, table, base, metrics);

    paintHorizontalLine(painter, QRectF(24, 578, 760, 1));
    painter.setFont(base);
    painter.setPen(FormStyle::secondaryText);
    painter.drawText(QRectF(24, 580, 760, 16), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("qtv8reader · golden-скриншоты совпадают побайтово"));

    painter.end();
    return pixmap;
}

} // namespace controls
