#ifndef QTV8READER_FORMSTYLES_H
#define QTV8READER_FORMSTYLES_H

#include <QHash>
#include <QString>

namespace v8reader {

class Configuration;
class V8Container;

// Ссылки на стили оформления в управляемой форме 1С записаны не именем:
// общий стиль конфигурации — GUID-ом («{0, GUID}» в записи цвета или шрифта),
// стиль платформы — отрицательным кодом («{-22}» — BorderColor). Выгрузка в
// Form.xml и рендерер «точного вида» работают с именем: «style:BorderColor».
// Таблица переводит GUID в имя стиля по списку «Элементы стиля» конфигурации,
// а коды платформы — по постоянной таблице, сверенной с Form.xml на корпусе
// форм (26 форм, 93 элемента со стилями). Значения стилей-цветов лежат в
// отдельных элементах контейнера (по GUID-у стиля) и тоже разбираются здесь:
// без них рендерер знает имя стиля, но не его цвет.
class FormStyleTable
{
public:
    // Наполнить таблицу по метаданным конфигурации: категория «Элементы
    // стиля» перечисляет общие стили (имя и GUID), а данные каждого стиля
    // лежат в контейнере отдельным элементом.
    void loadFromConfiguration(const Configuration &configuration,
                               const V8Container &container);

    bool isEmpty() const { return m_namesByGuid.isEmpty(); }

    // «style:Имя» по GUID стиля; пустая строка, если стиль не найден.
    QString reference(const QString &guid) const;

    // Значение стиля-цвета по имени (например, «#FCEB9F» или «web:Gainsboro»);
    // пустая строка, если стиль не цвет или его значение не разобрано.
    // Данные стиля читаются из контейнера при первом обращении и запоминаются.
    QString colorValue(const QString &name) const;

    // Имя стиля платформы по отрицательному коду из формы; пустая строка,
    // если код неизвестен (стиль останется неотрисованным).
    static QString platformStyleName(int code);

    // Имя web-цвета по его коду в контейнере (48 — Gainsboro). В Form.xml
    // такой цвет записан как «web:Gainsboro», и рендерер понимает это имя.
    static QString webColorName(int index);

private:
    const V8Container *m_container = nullptr;
    QHash<QString, QString> m_namesByGuid;
    QHash<QString, QString> m_guidsByName;
    mutable QHash<QString, QString> m_colorsByName;
};

} // namespace v8reader

#endif // QTV8READER_FORMSTYLES_H
