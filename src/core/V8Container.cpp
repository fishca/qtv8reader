#include "core/V8Container.h"

#include <QFile>
#include <QSet>
#include <QStringList>

#include <zlib.h>

#include <algorithm>
#include <cstring>
#include <limits>

namespace v8reader {
namespace {

constexpr int kFormat15 = 15;
constexpr int kFormat16 = 16;

constexpr qint64 kBase15 = 0;
constexpr qint64 kBase16 = 0x1359;

constexpr qint64 kFileHeaderSize15 = 16;
constexpr qint64 kFileHeaderSize16 = 20;
constexpr int kBlockHeaderSize15 = 31;
constexpr int kBlockHeaderSize16 = 55;
constexpr int kTocEntrySize15 = 12;
constexpr int kTocEntrySize16 = 24;

constexpr quint64 kUndefined15 = 0x7fffffffULL;
constexpr quint64 kUndefined16 = 0xffffffffffffffffULL;

constexpr qint64 kMaxDeclaredBlockSize = 2LL * 1024 * 1024 * 1024;
constexpr int kMaxTocEntries = 1000000;
constexpr int kMaxBlockPages = 1000000;

int blockHeaderSize(int version)
{
    return version == kFormat16 ? kBlockHeaderSize16 : kBlockHeaderSize15;
}

qint64 fileHeaderSize(int version)
{
    return version == kFormat16 ? kFileHeaderSize16 : kFileHeaderSize15;
}

int tocEntrySize(int version)
{
    return version == kFormat16 ? kTocEntrySize16 : kTocEntrySize15;
}

quint64 undefinedValue(int version)
{
    return version == kFormat16 ? kUndefined16 : kUndefined15;
}

quint32 readU32(const QByteArray &data, qint64 offset)
{
    const auto *p = reinterpret_cast<const unsigned char *>(data.constData()) + offset;
    return static_cast<quint32>(p[0])
        | (static_cast<quint32>(p[1]) << 8)
        | (static_cast<quint32>(p[2]) << 16)
        | (static_cast<quint32>(p[3]) << 24);
}

quint64 readU64(const QByteArray &data, qint64 offset)
{
    const auto low = static_cast<quint64>(readU32(data, offset));
    const auto high = static_cast<quint64>(readU32(data, offset + 4));
    return low | (high << 32);
}

int hexDigit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool parseHexField(const QByteArray &data, qint64 offset, int length,
                   quint64 *value, QString *error)
{
    qint64 begin = offset;
    qint64 end = offset + length;
    while (begin < end && data.at(begin) == ' ')
        ++begin;
    while (end > begin && data.at(end - 1) == ' ')
        --end;

    if (begin == end) {
        // Поля нулевой длины встречаются в пустых/служебных блоках.
        *value = 0;
        return true;
    }

    quint64 result = 0;
    for (qint64 i = begin; i < end; ++i) {
        const int digit = hexDigit(data.at(i));
        if (digit < 0) {
            if (error) {
                *error = QStringLiteral("Некорректное шестнадцатеричное поле в заголовке блока");
            }
            return false;
        }
        result = (result << 4) | static_cast<quint64>(digit);
    }

    *value = result;
    return true;
}

bool parseBlockHeaderAt(const QByteArray &data, qint64 position, int version,
                        quint64 *dataSize, quint64 *pageSize,
                        quint64 *nextAddress, QString *error)
{
    const int headerSize = blockHeaderSize(version);
    if (position < 0 || position + headerSize > data.size()) {
        if (error)
            *error = QStringLiteral("Заголовок блока выходит за границы файла");
        return false;
    }

    const int fieldLength = version == kFormat16 ? 16 : 8;
    if (static_cast<unsigned char>(data.at(position)) != 0x0d
        || static_cast<unsigned char>(data.at(position + 1)) != 0x0a
        || data.at(position + 2 + fieldLength) != ' '
        || data.at(position + 3 + fieldLength * 2) != ' '
        || data.at(position + 4 + fieldLength * 3) != ' '
        || static_cast<unsigned char>(data.at(position + headerSize - 2)) != 0x0d
        || static_cast<unsigned char>(data.at(position + headerSize - 1)) != 0x0a) {
        if (error)
            *error = QStringLiteral("Некорректный заголовок блока");
        return false;
    }

    if (!parseHexField(data, position + 2, fieldLength, dataSize, error)
        || !parseHexField(data, position + 3 + fieldLength, fieldLength, pageSize, error)
        || !parseHexField(data, position + 4 + fieldLength * 2, fieldLength,
                          nextAddress, error)) {
        return false;
    }

    return true;
}

bool inflateWithWindowBits(const QByteArray &input, QByteArray *output, int windowBits,
                           qint64 maxOutputSize, QString *error)
{
    output->clear();
    if (input.isEmpty())
        return false;

    if (static_cast<quint64>(input.size()) > std::numeric_limits<uInt>::max()) {
        if (error)
            *error = QStringLiteral("Слишком большой объём сжатых данных");
        return false;
    }

    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
    stream.avail_in = static_cast<uInt>(input.size());

    int result = inflateInit2(&stream, windowBits);
    if (result != Z_OK) {
        if (error)
            *error = QStringLiteral("Не удалось инициализировать распаковку zlib");
        return false;
    }

    char buffer[64 * 1024];
    QString failure;
    bool finished = false;

    while (!finished) {
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        stream.avail_out = sizeof(buffer);

        result = inflate(&stream, Z_NO_FLUSH);
        const int produced = static_cast<int>(sizeof(buffer) - stream.avail_out);

        if (produced > 0) {
            if (output->size() + produced > maxOutputSize) {
                failure = QStringLiteral("Распакованные данные превышают допустимый размер");
                break;
            }
            output->append(buffer, produced);
        }

        if (result == Z_STREAM_END) {
            finished = true;
        } else if (result != Z_OK) {
            failure = stream.msg
                ? QString::fromLatin1(stream.msg)
                : QStringLiteral("Ошибка распаковки zlib (%1)").arg(result);
            break;
        } else if (stream.avail_in == 0 && produced == 0) {
            failure = QStringLiteral("Сжатые данные неожиданно закончились");
            break;
        }
    }

    inflateEnd(&stream);

    if (!finished) {
        output->clear();
        if (error)
            *error = failure;
        return false;
    }

    return true;
}

} // namespace

QString V8ContainerFormat::label() const
{
    switch (version) {
    case kFormat15:
        return QStringLiteral("1C 8.0-8.3.15 (формат 1.5)");
    case kFormat16:
        return QStringLiteral("1C 8.3.16+ (формат 1.6)");
    default:
        return QStringLiteral("неизвестный формат");
    }
}

V8Container::V8Container() = default;

bool V8Container::open(const QString &filePath, QString *error)
{
    if (error)
        error->clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Не удалось открыть файл: %1").arg(file.errorString());
        return false;
    }

    const QByteArray data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        if (error)
            *error = QStringLiteral("Не удалось прочитать файл: %1").arg(file.errorString());
        return false;
    }

    if (!openData(data, error))
        return false;

    m_sourcePath = filePath;
    return true;
}

bool V8Container::openData(const QByteArray &data, QString *error)
{
    clear();
    m_sourcePath.clear();

    if (data.size() < kFileHeaderSize15) {
        if (error)
            *error = QStringLiteral("Файл слишком мал для контейнера 1C:V8");
        return false;
    }

    const Candidate candidates[] = {
        {kFormat16, kBase16},
        {kFormat15, kBase15},
    };

    QStringList failures;
    const auto tryParse = [&](const QByteArray &bytes) {
        m_data = bytes;
        m_elements.clear();
        m_format = V8ContainerFormat();

        QStringList localFailures;
        for (const Candidate &candidate : candidates) {
            QString candidateError;
            if (parseCandidate(candidate, &candidateError))
                return true;

            if (candidateError.isEmpty())
                candidateError = QStringLiteral("неизвестная ошибка");
            localFailures.append(QStringLiteral("%1: %2")
                                     .arg(candidate.version == kFormat16
                                              ? QStringLiteral("1.6")
                                              : QStringLiteral("1.5"),
                                          candidateError));
        }

        failures += localFailures;
        return false;
    };

    if (tryParse(data))
        return true;

    // Файлы .cfu содержат контейнер, сжатый целиком (raw deflate).
    QByteArray inflated;
    if (inflate(data, &inflated, nullptr)) {
        if (tryParse(inflated))
            return true;
    }

    clear();
    if (error)
        *error = QStringLiteral("Не удалось прочитать контейнер 1C:V8 (%1)")
                     .arg(failures.join(QStringLiteral("; ")));
    return false;
}

void V8Container::clear()
{
    m_data.clear();
    m_sourcePath.clear();
    m_format = V8ContainerFormat();
    m_elements.clear();
}

const V8Element *V8Container::at(int index) const
{
    if (index < 0 || index >= m_elements.size())
        return nullptr;
    return &m_elements.at(index);
}

int V8Container::indexOf(const QString &name) const
{
    for (int i = 0; i < m_elements.size(); ++i) {
        if (m_elements.at(i).name.compare(name, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

const V8Element *V8Container::find(const QString &name) const
{
    const int index = indexOf(name);
    return index >= 0 ? &m_elements.at(index) : nullptr;
}

bool V8Container::parseCandidate(const Candidate &candidate, QString *error)
{
    m_elements.clear();
    m_format = V8ContainerFormat();
    m_format.version = candidate.version;
    m_format.baseOffset = candidate.baseOffset;
    m_format.fileHeaderSize = fileHeaderSize(candidate.version);
    m_format.blockHeaderSize = blockHeaderSize(candidate.version);
    m_format.tocEntrySize = tocEntrySize(candidate.version);
    m_format.undefinedValue = undefinedValue(candidate.version);

    const qint64 base = candidate.baseOffset;
    if (base < 0 || base + m_format.fileHeaderSize > m_data.size()) {
        if (error)
            *error = QStringLiteral("заголовок файла выходит за границы");
        return false;
    }

    if (candidate.version == kFormat16) {
        m_format.pageSize = readU32(m_data, base + 8);
        m_format.storageVersion = readU32(m_data, base + 12);
        m_format.reserved = readU32(m_data, base + 16);
    } else {
        m_format.pageSize = readU32(m_data, base + 4);
        m_format.storageVersion = readU32(m_data, base + 8);
        m_format.reserved = readU32(m_data, base + 12);
    }

    QByteArray toc;
    if (!readBlock(m_format.fileHeaderSize, &toc, nullptr, error))
        return false;

    const int entrySize = m_format.tocEntrySize;
    if (entrySize <= 0 || toc.isEmpty()) {
        if (error)
            *error = QStringLiteral("пустая таблица размещения элементов");
        return false;
    }

    const int entryCount = static_cast<int>(
        std::min<qint64>(toc.size() / entrySize, kMaxTocEntries));
    for (int i = 0; i < entryCount; ++i) {
        const qint64 offset = static_cast<qint64>(i) * entrySize;

        quint64 headerAddress = 0;
        quint64 dataAddress = 0;
        quint64 terminator = 0;

        if (entrySize == kTocEntrySize16) {
            headerAddress = readU64(toc, offset);
            dataAddress = readU64(toc, offset + 8);
            terminator = readU64(toc, offset + 16);
        } else {
            headerAddress = readU32(toc, offset);
            dataAddress = readU32(toc, offset + 4);
            terminator = readU32(toc, offset + 8);
        }

        if (terminator != m_format.undefinedValue)
            break;

        if (headerAddress == m_format.undefinedValue)
            continue;

        if (headerAddress > static_cast<quint64>(std::numeric_limits<qint64>::max()))
            continue;

        if (dataAddress != m_format.undefinedValue
            && dataAddress
                   > static_cast<quint64>(std::numeric_limits<qint64>::max())) {
            continue;
        }

        QByteArray headerData;
        if (!readBlock(static_cast<qint64>(headerAddress), &headerData, nullptr, nullptr))
            continue;

        V8Element element;
        if (!parseElement(headerData, &element))
            continue;

        element.headerAddress = static_cast<qint64>(headerAddress);
        if (dataAddress == m_format.undefinedValue) {
            element.dataAddress = -1;
        } else {
            element.dataAddress = static_cast<qint64>(dataAddress);

            qint64 dataSize = 0;
            qint64 pageSize = 0;
            quint64 nextAddress = 0;
            if (readBlockHeader(element.dataAddress, &dataSize, &pageSize,
                                &nextAddress, nullptr)) {
                element.dataSize = dataSize;
            }
        }

        m_elements.append(element);
    }

    if (m_elements.isEmpty()) {
        if (error)
            *error = QStringLiteral("не найдено ни одного корректного элемента");
        return false;
    }

    return true;
}

bool V8Container::readBlock(qint64 address, QByteArray *data, qint64 *declaredSize,
                            QString *error) const
{
    if (error)
        error->clear();
    if (data)
        data->clear();
    if (declaredSize)
        *declaredSize = 0;

    if (!m_format.isValid()) {
        if (error)
            *error = QStringLiteral("контейнер не открыт");
        return false;
    }

    if (address < 0 || address > static_cast<qint64>(m_data.size()) - m_format.blockHeaderSize) {
        if (error)
            *error = QStringLiteral("адрес блока вне файла");
        return false;
    }

    qint64 blockDataSize = 0;
    qint64 pageSize = 0;
    quint64 nextAddress = 0;
    if (!readBlockHeader(address, &blockDataSize, &pageSize, &nextAddress, error))
        return false;

    if (blockDataSize < 0 || blockDataSize > kMaxDeclaredBlockSize
        || blockDataSize > m_data.size()) {
        if (error)
            *error = QStringLiteral("недопустимый размер блока данных");
        return false;
    }

    if (blockDataSize == 0) {
        if (declaredSize)
            *declaredSize = 0;
        return true;
    }

    if (declaredSize)
        *declaredSize = blockDataSize;

    QByteArray result;
    result.reserve(static_cast<qsizetype>(
        std::min<qint64>(blockDataSize, 16 * 1024 * 1024)));

    QSet<qint64> visitedHeaders;
    visitedHeaders.insert(address);

    qint64 remaining = blockDataSize;
    qint64 headerAddress = address;
    qint64 dataPosition = m_format.baseOffset + address + m_format.blockHeaderSize;
    int pageCount = 0;

    while (remaining > 0) {
        if (++pageCount > kMaxBlockPages) {
            if (error)
                *error = QStringLiteral("слишком длинная цепочка блоков");
            return false;
        }

        if (pageSize <= 0) {
            if (error)
                *error = QStringLiteral("нулевой размер страницы блока");
            return false;
        }

        const qint64 bytesToRead = std::min(pageSize, remaining);
        if (dataPosition < 0
            || bytesToRead > static_cast<qint64>(m_data.size()) - dataPosition) {
            if (error)
                *error = QStringLiteral("страница блока выходит за границы файла");
            return false;
        }

        result.append(m_data.constData() + dataPosition,
                      static_cast<qsizetype>(bytesToRead));
        remaining -= bytesToRead;

        if (remaining == 0)
            break;

        if (nextAddress == m_format.undefinedValue) {
            if (error)
                *error = QStringLiteral("цепочка блоков оборвана");
            return false;
        }

        if (nextAddress > static_cast<quint64>(std::numeric_limits<qint64>::max())) {
            if (error)
                *error = QStringLiteral("недопустимый адрес следующего блока");
            return false;
        }

        headerAddress = static_cast<qint64>(nextAddress);
        if (visitedHeaders.contains(headerAddress)) {
            if (error)
                *error = QStringLiteral("обнаружен цикл в цепочке блоков");
            return false;
        }
        visitedHeaders.insert(headerAddress);

        if (!readBlockHeader(headerAddress, nullptr, &pageSize, &nextAddress, error))
            return false;

        dataPosition = m_format.baseOffset + headerAddress + m_format.blockHeaderSize;
    }

    if (data)
        *data = result;

    return true;
}

bool V8Container::readBlockHeader(qint64 address, qint64 *dataSize, qint64 *pageSize,
                                  quint64 *nextAddress, QString *error) const
{
    if (error)
        error->clear();

    if (!m_format.isValid()) {
        if (error)
            *error = QStringLiteral("контейнер не открыт");
        return false;
    }

    const qint64 position = m_format.baseOffset + address;
    quint64 rawDataSize = 0;
    quint64 rawPageSize = 0;
    quint64 rawNextAddress = 0;

    if (!parseBlockHeaderAt(m_data, position, m_format.version,
                            &rawDataSize, &rawPageSize, &rawNextAddress, error)) {
        return false;
    }

    if (rawDataSize > static_cast<quint64>(std::numeric_limits<qint64>::max())
        || rawPageSize > static_cast<quint64>(std::numeric_limits<qint64>::max())) {
        if (error)
            *error = QStringLiteral("недопустимые размеры в заголовке блока");
        return false;
    }

    if (dataSize)
        *dataSize = static_cast<qint64>(rawDataSize);
    if (pageSize)
        *pageSize = static_cast<qint64>(rawPageSize);
    if (nextAddress)
        *nextAddress = rawNextAddress;

    return true;
}

bool V8Container::parseElement(const QByteArray &header, V8Element *element) const
{
    if (!element)
        return false;

    if (header.size() < 20)
        return false;

    element->headerSize = header.size();
    element->creationDate = v8TimeToDateTime(readU64(header, 0));
    element->modificationDate = v8TimeToDateTime(readU64(header, 8));
    element->reserved = readU32(header, 16);

    QString name;
    name.reserve((header.size() - 20) / 2);
    for (qint64 offset = 20; offset + 1 < header.size(); offset += 2) {
        const auto low = static_cast<unsigned char>(header.at(offset));
        const auto high = static_cast<unsigned char>(header.at(offset + 1));
        const ushort codeUnit = static_cast<ushort>(low | (high << 8));
        if (codeUnit == 0)
            break;
        name.append(QChar(codeUnit));
    }

    if (name.isEmpty())
        return false;

    element->name = name;
    return true;
}

bool V8Container::elementData(const V8Element &element, QByteArray *data,
                              QString *error) const
{
    if (error)
        error->clear();
    if (data)
        data->clear();

    if (!m_format.isValid()) {
        if (error)
            *error = QStringLiteral("контейнер не открыт");
        return false;
    }

    if (!element.hasData()) {
        if (data)
            data->clear();
        return true;
    }

    return readBlock(element.dataAddress, data, nullptr, error);
}

bool V8Container::elementPayload(const V8Element &element, QByteArray *payload,
                                 bool *compressed, QString *error) const
{
    if (error)
        error->clear();
    if (payload)
        payload->clear();
    if (compressed)
        *compressed = false;

    QByteArray raw;
    if (!elementData(element, &raw, error))
        return false;

    if (raw.isEmpty()) {
        if (payload)
            payload->clear();
        return true;
    }

    QString inflateError;
    QByteArray inflated;
    if (inflate(raw, &inflated, &inflateError)) {
        if (compressed)
            *compressed = true;
        if (payload)
            *payload = inflated;
        return true;
    }

    if (compressed)
        *compressed = false;
    if (payload)
        *payload = raw;
    return true;
}

bool V8Container::openNested(const V8Element &element, V8Container *nested,
                             QString *error) const
{
    if (error)
        error->clear();
    if (!nested) {
        if (error)
            *error = QStringLiteral("не задан приёмник вложенного контейнера");
        return false;
    }

    QByteArray payload;
    if (!elementPayload(element, &payload, nullptr, error))
        return false;

    if (!looksLikeContainer(payload, nullptr)) {
        if (error)
            *error = QStringLiteral("элемент «%1» не является контейнером").arg(element.name);
        return false;
    }

    return nested->openData(payload, error);
}

bool V8Container::looksLikeContainer(const QByteArray &data, int *version)
{
    if (version)
        *version = 0;

    struct Probe
    {
        int version;
        qint64 base;
    };

    const Probe probes[] = {
        {kFormat16, kBase16},
        {kFormat15, kBase15},
    };

    for (const Probe &probe : probes) {
        const qint64 position = probe.base + fileHeaderSize(probe.version);
        quint64 dataSize = 0;
        quint64 pageSize = 0;
        quint64 nextAddress = 0;
        if (parseBlockHeaderAt(data, position, probe.version,
                               &dataSize, &pageSize, &nextAddress, nullptr)) {
            if (version)
                *version = probe.version;
            return true;
        }
    }

    return false;
}

bool V8Container::inflate(const QByteArray &input, QByteArray *output,
                          QString *error, qint64 maxOutputSize)
{
    if (error)
        error->clear();
    if (output)
        output->clear();

    if (input.isEmpty()) {
        if (error)
            *error = QStringLiteral("пустые сжатые данные");
        return false;
    }

    QString rawError;
    if (inflateWithWindowBits(input, output, -MAX_WBITS, maxOutputSize, &rawError))
        return true;

    QString zlibError;
    if (inflateWithWindowBits(input, output, MAX_WBITS, maxOutputSize, &zlibError))
        return true;

    if (error) {
        *error = QStringLiteral("не удалось распаковать данные (raw deflate: %1; zlib: %2)")
                     .arg(rawError, zlibError);
    }
    return false;
}

bool V8Container::isCompressed(const QByteArray &data)
{
    if (data.isEmpty())
        return false;

    QByteArray output;
    return inflate(data, &output, nullptr, 64 * 1024 * 1024);
}

QDateTime V8Container::v8TimeToDateTime(quint64 value)
{
    if (value == 0)
        return QDateTime();

    // 1C хранит время в единицах по 100 мкс от 0001-01-01.
    // 621355968000000 таких единиц соответствуют 1970-01-01.
    constexpr qint64 kUnitsToUnixEpoch = 621355968000000LL;
    if (value > static_cast<quint64>(std::numeric_limits<qint64>::max()))
        return QDateTime();

    const qint64 units = static_cast<qint64>(value);
    const qint64 msecs = (units - kUnitsToUnixEpoch) / 10;
    const QDateTime result = QDateTime::fromMSecsSinceEpoch(msecs);
    return result.isValid() ? result : QDateTime();
}

} // namespace v8reader
