#ifndef QTV8READER_TESTCONTAINERBUILDER_H
#define QTV8READER_TESTCONTAINERBUILDER_H

#include <QByteArray>
#include <QString>
#include <QVector>

namespace v8reader {
namespace test {

struct TestElement
{
    QString name;
    QByteArray payload;
    bool compress = true;
    // Если больше нуля, данные разбиваются на цепочку блоков такого размера.
    int pageSize = 0;
};

// Собирает синтетический контейнер 1C:V8 формата 1.5 или 1.6.
class TestContainerBuilder
{
public:
    static QByteArray build(int version, const QVector<TestElement> &elements,
                            QString *error = nullptr);
    static QByteArray build15(const QVector<TestElement> &elements,
                              QString *error = nullptr);
    static QByteArray build16(const QVector<TestElement> &elements,
                              QString *error = nullptr);

    static QByteArray compress(const QByteArray &data, QString *error = nullptr);
};

} // namespace test
} // namespace v8reader

#endif // QTV8READER_TESTCONTAINERBUILDER_H
