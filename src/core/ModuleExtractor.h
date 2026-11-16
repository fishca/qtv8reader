#ifndef QTV8READER_MODULEEXTRACTOR_H
#define QTV8READER_MODULEEXTRACTOR_H

#include "core/Configuration.h"
#include "core/V8Container.h"
#include "core/V8Text.h"

#include <QString>
#include <QVector>

namespace v8reader {

// Найденный модуль объекта метаданных.
struct ModuleInfo
{
    QString name;
    QString elementName;
    QString source;
    QString text;
    bool fromNestedContainer = false;

    bool isEmpty() const { return text.trimmed().isEmpty(); }
};

// Ищет текст модулей 1С в элементах контейнера.
// Модули обычно хранятся в элементах <GUID>.0 ... <GUID>.5,
// во вложенных контейнерах (элементы text/module) или внутри
// строк метаданных в виде #base64.
class ModuleExtractor
{
public:
    explicit ModuleExtractor(const V8Container &container);

    QVector<ModuleInfo> extractForObject(const MetadataObject &object) const;
    QVector<ModuleInfo> extractForGuid(const QString &guid) const;
    QVector<ModuleInfo> extractFromElement(const QString &elementName) const;

    static bool looksLikeModule(const QString &text);
    static QString moduleLabelForSuffix(int suffix);

private:
    // Модуль из первого найденного элемента <guid>.<суффикс> в заданном порядке.
    QVector<ModuleInfo> extractPrimary(const QString &guid,
                                       const QVector<int> &suffixOrder,
                                       const QString &label) const;
    void searchElement(const QString &elementName, const QString &label,
                       QVector<ModuleInfo> *modules) const;
    void searchNested(const V8Container &container, const QString &prefix,
                      QVector<ModuleInfo> *modules, int depth) const;
    void searchPayload(const QByteArray &payload, const QString &elementName,
                       const QString &label, QVector<ModuleInfo> *modules) const;
    void searchTree(const V8TextNode &node, const QString &elementName,
                    const QString &label, QVector<ModuleInfo> *modules,
                    int depth) const;

    const V8Container &m_container;
};

} // namespace v8reader

#endif // QTV8READER_MODULEEXTRACTOR_H
