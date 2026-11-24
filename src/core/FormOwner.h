#ifndef QTV8READER_FORMOWNER_H
#define QTV8READER_FORMOWNER_H

#include "core/Configuration.h"
#include "core/FormDocument.h"

namespace v8reader {

class ConfigurationReader;

// Заполняет «владельческую» часть документа формы по метаданным объекта:
// заголовок окна, синонимы реквизитов, типы и квалификаторы для рендерера
// (objectMeta), а также платформенные наборы команд формы (запись/проведение
// для документов, создание для списков и т. п.).
void applyOwnerMetadata(const MetadataObject &object, FormDocument *document);

// Дополняет objectMeta стандартными реквизитами объекта по тексту его
// метаданных из контейнера: «Номер» документа, «Код» и «Наименование»
// справочника, «Дата» документа. Платформа хранит их длину и вид числами в
// списке свойств объекта, а рендерер по ним считает ширину поля: без этого
// «Номер» получает ширину по умолчанию в 20 символов вместо 11 и выглядит
// на 40 px шире, чем в конфигураторе.
void applyOwnerStandardAttributes(const QString &metadataText, FormDocument *document);

// Заполняет refMeta — метаданные справочников, на которые ссылаются реквизиты
// формы. Платформа берёт ширину поля ссылки из длины представления объекта,
// поэтому без этих чисел поле «Подотчетное лицо» (у «Физических лиц» 100)
// выходит в 15 знаков — на 96 px уже, чем в конфигураторе.
void applyReferencedCatalogMeta(const ConfigurationReader &reader, FormDocument *document);

// Код вида объекта для рендерера: «Document», «Catalog», «Report», …
QString ownerKindTag(const QString &typeTitle);

// Тип значения в терминах рендерера: «xs:string», «xs:decimal»,
// «cfg:CatalogRef.Валюты», составной — через запятую. Пустая строка, если
// тип не распознан.
QString rendererTypeFor(const QString &typeText);

} // namespace v8reader

#endif // QTV8READER_FORMOWNER_H
