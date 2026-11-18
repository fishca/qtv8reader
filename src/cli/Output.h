#ifndef QTV8READER_CLI_OUTPUT_H
#define QTV8READER_CLI_OUTPUT_H

#include <QTextStream>

namespace v8reader::cli {

QTextStream &out();
QTextStream &err();
void setStreams(QTextStream *outStream, QTextStream *errStream);

} // namespace v8reader::cli

#endif // QTV8READER_CLI_OUTPUT_H
