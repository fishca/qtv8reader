#ifndef QTV8READER_V8CONTAINER_H
#define QTV8READER_V8CONTAINER_H

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QVector>

namespace v8reader {

// Описание одного элемента контейнера 1C:V8.
struct V8Element
{
    QString name;
    qint64 headerAddress = -1;
    qint64 dataAddress = -1;
    qint64 dataSize = -1;
    qint64 headerSize = 0;
    QDateTime creationDate;
    QDateTime modificationDate;
    quint32 reserved = 0;

    bool hasData() const { return dataAddress >= 0; }
};

// Параметры разобранного контейнера.
struct V8ContainerFormat
{
    int version = 0; // 15 (1C 8.0-8.3.15) или 16 (1C 8.3.16+)
    qint64 baseOffset = 0;
    qint64 fileHeaderSize = 0;
    int blockHeaderSize = 0;
    int tocEntrySize = 0;
    quint64 undefinedValue = 0;
    quint32 pageSize = 0;
    quint32 storageVersion = 0;
    quint32 reserved = 0;

    bool isValid() const { return version == 15 || version == 16; }
    QString label() const;
};

// Читатель контейнеров 1C:V8 (.cf, .cfe, .epf, .erf и вложенные контейнеры).
// Работает только на чтение и полностью размещает файл в памяти.
class V8Container
{
public:
    V8Container();

    bool open(const QString &filePath, QString *error = nullptr);
    bool openData(const QByteArray &data, QString *error = nullptr);

    void clear();
    bool isOpen() const { return m_format.isValid(); }

    const V8ContainerFormat &format() const { return m_format; }
    const QString &sourcePath() const { return m_sourcePath; }
    qint64 size() const { return m_data.size(); }

    int count() const { return m_elements.size(); }
    const V8Element *at(int index) const;
    int indexOf(const QString &name) const;
    const V8Element *find(const QString &name) const;
    const QVector<V8Element> &elements() const { return m_elements; }

    // Сырые данные элемента (цепочка блоков без распаковки).
    bool elementData(const V8Element &element, QByteArray *data,
                     QString *error = nullptr) const;

    // Полезная нагрузка элемента: при необходимости распаковывается.
    // compressed получает true, если данные были сжаты raw deflate.
    bool elementPayload(const V8Element &element, QByteArray *payload,
                        bool *compressed = nullptr, QString *error = nullptr) const;

    // Проверяет, является ли полезная нагрузка элемента вложенным контейнером,
    // и открывает его.
    bool openNested(const V8Element &element, V8Container *nested,
                    QString *error = nullptr) const;

    // Проверка сигнатуры контейнера без полного разбора.
    static bool looksLikeContainer(const QByteArray &data, int *version = nullptr);

    // Распаковка raw deflate (с запасным вариантом zlib).
    static bool inflate(const QByteArray &input, QByteArray *output,
                        QString *error = nullptr,
                        qint64 maxOutputSize = 512LL * 1024 * 1024);

    static bool isCompressed(const QByteArray &data);
    static QDateTime v8TimeToDateTime(quint64 value);

private:
    struct Candidate
    {
        int version = 0;
        qint64 baseOffset = 0;
    };

    bool parseCandidate(const Candidate &candidate, QString *error);
    bool readBlock(qint64 address, QByteArray *data, qint64 *declaredSize,
                   QString *error) const;
    bool readBlockHeader(qint64 address, qint64 *dataSize, qint64 *pageSize,
                         quint64 *nextAddress, QString *error) const;
    bool parseElement(const QByteArray &header, V8Element *element) const;

    QByteArray m_data;
    QString m_sourcePath;
    V8ContainerFormat m_format;
    QVector<V8Element> m_elements;
    // Имя в нижнем регистре → индекс первого элемента. Ускоряет find()/indexOf()
    // на больших конфигурациях (тысячи объектов).
    QHash<QString, int> m_index;
};

} // namespace v8reader

#endif // QTV8READER_V8CONTAINER_H
