#ifndef QTV8READER_FORMOWNER_H
#define QTV8READER_FORMOWNER_H

#include "core/Configuration.h"
#include "core/FormDocument.h"

namespace v8reader {

// Заполняет «владельческую» часть документа формы по метаданным объекта:
// заголовок окна, синонимы реквизитов, типы и квалификаторы для рендерера
// (objectMeta), а также платформенные наборы команд формы (запись/проведение
// для документов, создание для списков и т. п.).
void applyOwnerMetadata(const MetadataObject &object, FormDocument *document);

// Код вида объекта для рендерера: «Document», «Catalog», «Report», …
QString ownerKindTag(const QString &typeTitle);

// Тип значения в терминах рендерера: «xs:string», «xs:decimal»,
// «cfg:CatalogRef.Валюты», составной — через запятую. Пустая строка, если
// тип не распознан.
QString rendererTypeFor(const QString &typeText);

} // namespace v8reader

#endif // QTV8READER_FORMOWNER_H
