#include "cli/Output.h"

#include <cstdio>

namespace v8reader::cli {
namespace {

struct Utf8Stream
{
    explicit Utf8Stream(FILE *file) : stream(file)
    {
        stream.setEncoding(QStringConverter::Utf8);
    }

    QTextStream stream;
};

QTextStream &stdoutStream()
{
    static Utf8Stream holder(stdout);
    return holder.stream;
}

QTextStream &stderrStream()
{
    static Utf8Stream holder(stderr);
    return holder.stream;
}

QTextStream *g_out = nullptr;
QTextStream *g_err = nullptr;

} // namespace

QTextStream &out()
{
    return g_out ? *g_out : stdoutStream();
}

QTextStream &err()
{
    return g_err ? *g_err : stderrStream();
}

void setStreams(QTextStream *outStream, QTextStream *errStream)
{
    g_out = outStream;
    g_err = errStream;
}

} // namespace v8reader::cli
