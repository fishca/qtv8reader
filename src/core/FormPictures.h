#ifndef QTV8READER_FORMPICTURES_H
#define QTV8READER_FORMPICTURES_H

#include <QHash>
#include <QString>

namespace v8reader {

class Configuration;
class V8Container;

// Общие картинки конфигурации: форма ссылается на картинку GUID-ом, а
// Form.xml и рендерер «точного вида» работают с именем
// («CommonPicture.Внимание16»). Сами байты картинки лежат в контейнере
// отдельным элементом с именем GUID-а (поток .0) и запакованы в ZIP с
// вариантами по экранным плотностям; рендерер разбирает этот ZIP сам.
class FormPictureTable
{
public:
    // Наполнить таблицу по категории «Общие картинки» конфигурации.
    // Данные картинок читаются из контейнера при первом обращении.
    void loadFromConfiguration(const Configuration &configuration,
                               const V8Container &container);

    bool isEmpty() const { return m_namesByGuid.isEmpty(); }

    // «CommonPicture.Имя» по GUID картинки; пустая строка, если картинка
    // не найдена.
    QString reference(const QString &guid) const;

    // Имя общей картинки по GUID; пустая строка, если картинка не найдена.
    QString name(const QString &guid) const;

    // Данные картинки: base64 ZIP-архива с вариантами PNG (как их отдаёт
    // выгрузка в XML в узле commonPictures).
    QString data(const QString &guid) const;

    // Данные картинки по её имени (для уже разобранной ссылки).
    QString dataByName(const QString &name) const;

    // Имя стандартной картинки платформы по отрицательному коду из формы
    // (пока подтверждён один код: −2 — «очистить» у поля ввода).
    static QString standardPictureName(int code);

    // Имя стандартной картинки платформы по GUID-у: часть картинок платформа
    // хранит как обычные общие картинки своей библиотеки, а Form.xml пишет
    // их как StdPicture.<Имя>. Таблица сверена с Form.xml по корпусу.
    static QString stdPictureName(const QString &guid);

private:
    const V8Container *m_container = nullptr;
    QHash<QString, QString> m_namesByGuid;
    QHash<QString, QString> m_guidsByName;
    mutable QHash<QString, QString> m_dataByGuid;
};

} // namespace v8reader

#endif // QTV8READER_FORMPICTURES_H
