#ifndef QTV8READER_FORMWEBMODEL_H
#define QTV8READER_FORMWEBMODEL_H

#include <QByteArray>

namespace v8reader {

struct FormDocument;

// Собирает описание управляемой формы в JSON для браузерного рендерера
// «точного вида» (страница /formpreview/index.html поверх WebView2).
//
// Модель повторяет структуру FormDocument, но в терминах, удобных
// JavaScript-рендереру: вид элемента, заголовок, путь к данным, тип значения,
// ширина в единицах формы, режим группы и т. п.
QByteArray formWebModelJson(const FormDocument &document);

} // namespace v8reader

#endif // QTV8READER_FORMWEBMODEL_H
