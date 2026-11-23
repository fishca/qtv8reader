#include "core/FormStyles.h"

#include "core/Configuration.h"
#include "core/V8Container.h"
#include "core/V8Text.h"

namespace v8reader {
namespace {

// Корневые коллекции стилей конфигурации (см. MetadataCatalog).
const char kStyleItemsGuid[] = "58848766-36ea-4076-8800-e91eb49590d7";
const char kStylesGuid[] = "3e5404af-6ef8-4c73-ad11-91bd2dfac4c8";

struct PlatformStyle
{
    int code;
    const char *name;
};

// Стили, которые платформа не описывает в метаданных конфигурации: в форме
// они записаны отрицательным кодом, а Form.xml пишет их именем. Таблица
// сверена с Form.xml по корпусу из 26 форм: код взят из записи цвета того
// элемента, у которого в Form.xml стоит это имя. Коды платформы постоянны
// (одинаковы во всех конфигурациях), поэтому таблица хранится в коде.
const PlatformStyle kPlatformStyles[] = {
    {-1, "FormBackColor"},          // фон формы; 253, 1732
    {-16, "SpecialTextColor"},      // «специальный» текст; Новости
    {-22, "BorderColor"},           // рамка поля и таблицы; Валюты, Задачи
    {-23, "ToolTipBackColor"},      // фон подсказки; ЗагрузкаДокументов
    {-35, "TableHeaderBackColor"},  // фон заголовка таблицы; Лиды
    {-46, "AccentColor"},           // акцентный цвет; Лиды
    // Стили шрифтов платформы: имя выводится из записи шрифта.
    {-20, "TextFont"},
    {-30, "SmallTextFont"},
    {-31, "NormalTextFont"},
};

struct WebColor
{
    int index;
    const char *name;
};

// Коды web-цветов: в контейнере цвет записан номером известного цвета
// ({3, 2, {48}}), а Form.xml пишет его именем (web:Gainsboro). Номера
// сверены с выгрузкой конфигурации: для каждого стиля-цвета проверено
// соответствие номера из контейнера и имени из StyleItems/<имя>.xml.
const WebColor kWebColors[] = {
    {8, "Black"},       {10, "Blue"},         {20, "Cream"},
    {21, "Crimson"},    {23, "DarkBlue"},     {26, "DarkGray"},
    {33, "DarkRed"},    {37, "DarkSlateGray"}, {44, "FireBrick"},
    {46, "ForestGreen"}, {48, "Gainsboro"},   {49, "GhostWhite"},
    {50, "Gold"},       {52, "Gray"},         {53, "Green"},
    {55, "HoneyDew"},   {64, "LemonChiffon"}, {67, "LightCyan"},
    {71, "LightGray"},  {72, "LightPink"},    {79, "LightYellow"},
    {84, "Maroon"},     {87, "MediumGray"},   {98, "MistyRose"},
    {105, "Orange"},    {119, "Red"},         {128, "Silver"},
    {130, "SlateBlue"}, {134, "SteelBlue"},   {138, "Tomato"},
    {140, "Violet"},    {141, "VioletRed"},   {144, "WhiteSmoke"},
    {145, "Yellow"},
};

QString colorText(qint64 color)
{
    if (color <= 0)
        return QString();
    const int red = static_cast<int>(color & 0xFF);
    const int green = static_cast<int>((color >> 8) & 0xFF);
    const int blue = static_cast<int>((color >> 16) & 0xFF);
    return QStringLiteral("#%1%2%3")
        .arg(red, 2, 16, QLatin1Char('0'))
        .arg(green, 2, 16, QLatin1Char('0'))
        .arg(blue, 2, 16, QLatin1Char('0'))
        .toUpper();
}

// Цвет стиля в записи данных стиля:
//   {1, {3, 0, {"#", <вид>, 2, {3, 0, {<цвет>}}}}, {свойства}, 0}
// Внешняя запись {3, 0, {...}} описывает вид стиля, а сам цвет лежит во
// вложенной записи: {3, 0, {<число>}} — абсолютный цвет (COLORREF 0x00BBGGRR),
// {3, 2, {<номер>}} — web-цвет (48 — Gainsboro).
QString colorFromStyleData(const V8TextNode &node)
{
    if (node.isList() && node.children.size() == 3) {
        const V8TextNode *code = node.child(0);
        const V8TextNode *kind = node.child(1);
        const V8TextNode *payload = node.child(2);
        if (code && code->isInteger() && code->toInteger() == 3
            && kind && kind->isInteger()
            && payload && payload->isList() && payload->children.size() == 1) {
            const V8TextNode *value = payload->child(0);
            if (value && value->isInteger()) {
                QString text;
                if (kind->toInteger() == 0)
                    text = colorText(value->toInteger());
                else if (kind->toInteger() == 2)
                    text = QStringLiteral("web:")
                        + FormStyleTable::webColorName(static_cast<int>(value->toInteger()));
                if (!text.isEmpty())
                    return text;
            }
        }
    }
    for (const V8TextNode &child : node.children) {
        const QString text = colorFromStyleData(child);
        if (!text.isEmpty())
            return text;
    }
    return QString();
}

} // namespace

void FormStyleTable::loadFromConfiguration(const Configuration &configuration,
                                           const V8Container &container)
{
    m_container = &container;
    m_namesByGuid.clear();
    m_guidsByName.clear();
    m_colorsByName.clear();
    for (const MetadataCategory &category : configuration.categories) {
        const QString guid = category.guid.toLower();
        if (guid != QLatin1String(kStyleItemsGuid)
            && guid != QLatin1String(kStylesGuid)) {
            continue;
        }
        for (const MetadataObject &object : category.objects) {
            const QString objectGuid = object.guid.toLower();
            if (objectGuid.isEmpty() || object.name.isEmpty())
                continue;
            m_namesByGuid.insert(objectGuid, object.name);
            m_guidsByName.insert(object.name, objectGuid);
        }
    }
}

QString FormStyleTable::reference(const QString &guid) const
{
    const QString name = m_namesByGuid.value(guid.trimmed().toLower());
    if (name.isEmpty())
        return QString();
    return QStringLiteral("style:") + name;
}

QString FormStyleTable::colorValue(const QString &name) const
{
    auto cached = m_colorsByName.constFind(name);
    if (cached != m_colorsByName.constEnd())
        return cached.value();
    QString color;
    const QString guid = m_guidsByName.value(name);
    if (!guid.isEmpty() && m_container) {
        // Данные стиля — отдельный элемент контейнера с именем GUID-а.
        if (const V8Element *element = m_container->find(guid)) {
            QByteArray payload;
            if (m_container->elementPayload(*element, &payload, nullptr, nullptr)) {
                QString error;
                const QString text = V8Text::decode(payload, nullptr, &error);
                V8TextNode root;
                V8TextError parseError;
                if (!text.isEmpty() && V8Text::parse(text, root, &parseError))
                    color = colorFromStyleData(root);
            }
        }
    }
    m_colorsByName.insert(name, color);
    return color;
}

QString FormStyleTable::platformStyleName(int code)
{
    for (const PlatformStyle &style : kPlatformStyles) {
        if (style.code == code)
            return QString::fromLatin1(style.name);
    }
    return QString();
}

QString FormStyleTable::webColorName(int index)
{
    for (const WebColor &color : kWebColors) {
        if (color.index == index)
            return QString::fromLatin1(color.name);
    }
    return QString();
}

} // namespace v8reader
