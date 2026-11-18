#ifndef QTV8READER_CLIAPPLICATION_H
#define QTV8READER_CLIAPPLICATION_H

#include <QString>

class QTextStream;

namespace v8reader {

class CliApplication
{
public:
    struct Options
    {
        QString command;
        QString path;
        QString argument;
        QString output;
        bool raw = false;
    };

    int run(const Options &options, QTextStream &out, QTextStream &err);
};

} // namespace v8reader

#endif // QTV8READER_CLIAPPLICATION_H
