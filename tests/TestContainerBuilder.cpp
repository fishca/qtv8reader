#include "TestContainerBuilder.h"

#include <QDateTime>
#include <QTimeZone>
#include <QtEndian>

#include <zlib.h>

#include <algorithm>
#include <cstring>

namespace v8reader {
namespace test {
namespace {

constexpr qint64 kBase15 = 0;
constexpr qint64 kBase16 = 0x1359;
constexpr int kHeaderSize15 = 31;
constexpr int kHeaderSize16 = 55;
constexpr int kTocEntrySize15 = 12;
constexpr int kTocEntrySize16 = 24;
constexpr quint64 kUndefined15 = 0x7fffffffULL;
constexpr quint64 kUndefined16 = 0xffffffffffffffffULL;

void appendU16(QByteArray &out, quint16 value)
{
    char bytes[2];
    qToLittleEndian(value, bytes);
    out.append(bytes, 2);
}

void appendU32(QByteArray &out, quint32 value)
{
    char bytes[4];
    qToLittleEndian(value, bytes);
    out.append(bytes, 4);
}

void appendU64(QByteArray &out, quint64 value)
{
    char bytes[8];
    qToLittleEndian(value, bytes);
    out.append(bytes, 8);
}

int blockHeaderSize(int version)
{
    return version == 16 ? kHeaderSize16 : kHeaderSize15;
}

int tocEntrySize(int version)
{
    return version == 16 ? kTocEntrySize16 : kTocEntrySize15;
}

quint64 undefinedValue(int version)
{
    return version == 16 ? kUndefined16 : kUndefined15;
}

QByteArray blockHeader(int version, qint64 dataSize, qint64 pageSize,
                       quint64 nextAddress)
{
    const int width = version == 16 ? 16 : 8;
    QByteArray header;
    header.reserve(blockHeaderSize(version));
    header += "\r\n";
    header += QByteArray::number(dataSize, 16).rightJustified(width, '0');
    header += ' ';
    header += QByteArray::number(pageSize, 16).rightJustified(width, '0');
    header += ' ';
    header += QByteArray::number(nextAddress, 16).rightJustified(width, '0');
    header += ' ';
    header += "\r\n";
    return header;
}

QByteArray elementHeader(const QString &name)
{
    // 2024-01-02 03:04:05 UTC в единицах 1C (100 мкс от 0001-01-01).
    constexpr qint64 kUnitsToUnixEpoch = 621355968000000LL;
    const qint64 msecs = QDateTime(QDate(2024, 1, 2), QTime(3, 4, 5),
                                   QTimeZone(QByteArrayLiteral("UTC")))
                             .toMSecsSinceEpoch();
    const quint64 ticks = static_cast<quint64>(msecs * 10 + kUnitsToUnixEpoch);

    QByteArray header;
    header.reserve(20 + name.size() * 2 + 2);
    appendU64(header, ticks);
    appendU64(header, ticks);
    appendU32(header, 0);
    for (const QChar ch : name)
        appendU16(header, ch.unicode());
    appendU16(header, 0);
    return header;
}

bool deflateRaw(const QByteArray &input, QByteArray *output, QString *error)
{
    output->clear();

    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
    stream.avail_in = static_cast<uInt>(input.size());

    if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK) {
        if (error)
            *error = QStringLiteral("deflateInit2 failed");
        return false;
    }

    char buffer[64 * 1024];
    int result = Z_OK;
    do {
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        stream.avail_out = sizeof(buffer);
        result = deflate(&stream, Z_FINISH);
        const int produced = static_cast<int>(sizeof(buffer) - stream.avail_out);
        if (produced > 0)
            output->append(buffer, produced);
    } while (result == Z_OK);

    deflateEnd(&stream);

    if (result != Z_STREAM_END) {
        output->clear();
        if (error)
            *error = QStringLiteral("deflate failed: %1").arg(result);
        return false;
    }
    return true;
}

} // namespace

QByteArray TestContainerBuilder::build(int version, const QVector<TestElement> &elements,
                                       QString *error)
{
    if (error)
        error->clear();
    if (version != 15 && version != 16) {
        if (error)
            *error = QStringLiteral("unsupported version");
        return QByteArray();
    }
    if (elements.isEmpty()) {
        if (error)
            *error = QStringLiteral("no elements");
        return QByteArray();
    }

    const qint64 base = version == 16 ? kBase16 : kBase15;
    const int headerSize = blockHeaderSize(version);
    const quint64 undefined = undefinedValue(version);
    const int entrySize = tocEntrySize(version);

    QByteArray document(static_cast<qsizetype>(base), '\0');

    // Заголовок файла.
    appendU32(document, 0xFFFFFF7F);
    if (version == 16) {
        appendU32(document, 0); // резерв сигнатуры
        appendU32(document, 0x200);
        appendU32(document, 0);
        appendU32(document, 0);
    } else {
        appendU32(document, 0x200);
        appendU32(document, 0);
        appendU32(document, 0);
    }

    const auto appendBlock = [&](const QByteArray &data, qint64 pageSize,
                                 quint64 nextAddress, qint64 *dataOffset) -> qint64 {
        const qint64 address = document.size() - base;
        const qint64 effectivePageSize = pageSize > 0 ? pageSize : data.size();
        document += blockHeader(version, data.size(), effectivePageSize, nextAddress);
        if (dataOffset)
            *dataOffset = document.size();
        document += data;
        return address;
    };

    QByteArray toc(static_cast<qsizetype>(elements.size() * entrySize), '\0');
    qint64 tocDataOffset = 0;
    appendBlock(toc, toc.size(), undefined, &tocDataOffset);

    struct Entry
    {
        qint64 headerAddress = -1;
        qint64 dataAddress = -1;
    };
    QVector<Entry> entries;
    entries.reserve(elements.size());

    for (const TestElement &element : elements) {
        if (element.name.isEmpty()) {
            if (error)
                *error = QStringLiteral("empty element name");
            return QByteArray();
        }

        Entry entry;
        const QByteArray header = elementHeader(element.name);
        entry.headerAddress = appendBlock(header, header.size(), undefined, nullptr);

        QByteArray payload = element.payload;
        if (element.compress && !payload.isEmpty()) {
            QByteArray compressed;
            if (!deflateRaw(payload, &compressed, error))
                return QByteArray();
            payload = compressed;
        }

        if (!payload.isEmpty()) {
            if (element.pageSize > 0
                && payload.size() > element.pageSize) {
                const qint64 firstAddress = document.size() - base;
                int offset = 0;
                while (offset < payload.size()) {
                    const int chunkSize = std::min<int>(
                        element.pageSize, payload.size() - offset);
                    const qint64 blockAddress = document.size() - base;
                    const bool hasNext = offset + chunkSize < payload.size();
                    const quint64 nextAddress = hasNext
                        ? static_cast<quint64>(blockAddress + headerSize + chunkSize)
                        : undefined;

                    document += blockHeader(version, payload.size(), chunkSize, nextAddress);
                    document += payload.mid(offset, chunkSize);
                    offset += chunkSize;
                }
                entry.dataAddress = firstAddress;
            } else {
                entry.dataAddress = appendBlock(
                    payload, payload.size(), undefined, nullptr);
            }
        }

        entries.append(entry);
    }

    QByteArray headerToc;
    headerToc.reserve(toc.size());
    for (const Entry &entry : entries) {
        if (version == 16) {
            appendU64(headerToc, static_cast<quint64>(entry.headerAddress));
            appendU64(headerToc, entry.dataAddress < 0
                                     ? undefined
                                     : static_cast<quint64>(entry.dataAddress));
            appendU64(headerToc, undefined);
        } else {
            appendU32(headerToc, static_cast<quint32>(entry.headerAddress));
            appendU32(headerToc, entry.dataAddress < 0
                                     ? static_cast<quint32>(undefined)
                                     : static_cast<quint32>(entry.dataAddress));
            appendU32(headerToc, static_cast<quint32>(undefined));
        }
    }

    if (headerToc.size() != toc.size()) {
        if (error)
            *error = QStringLiteral("invalid TOC size");
        return QByteArray();
    }

    if (tocDataOffset < 0 || tocDataOffset + headerToc.size() > document.size()) {
        if (error)
            *error = QStringLiteral("TOC patch out of bounds");
        return QByteArray();
    }

    std::memcpy(document.data() + tocDataOffset, headerToc.constData(),
                static_cast<size_t>(headerToc.size()));

    return document;
}

QByteArray TestContainerBuilder::build15(const QVector<TestElement> &elements,
                                         QString *error)
{
    return build(15, elements, error);
}

QByteArray TestContainerBuilder::build16(const QVector<TestElement> &elements,
                                         QString *error)
{
    return build(16, elements, error);
}

QByteArray TestContainerBuilder::compress(const QByteArray &data, QString *error)
{
    QByteArray result;
    if (!deflateRaw(data, &result, error))
        return QByteArray();
    return result;
}

} // namespace test
} // namespace v8reader
