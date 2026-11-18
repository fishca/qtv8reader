#ifndef QTV8READER_BINARYTEXT_H
#define QTV8READER_BINARYTEXT_H

#include <QByteArray>

namespace v8reader {

// Эвристика «это, скорее всего, текст» для содержимого контейнера.
// allowUtf16 включает распознавание UTF-16 без BOM (используется выгрузкой).
bool isProbablyText(const QByteArray &data, bool allowUtf16 = false);

} // namespace v8reader

#endif // QTV8READER_BINARYTEXT_H
