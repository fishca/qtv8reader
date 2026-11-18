#include "core/BinaryText.h"

#include <algorithm>

namespace v8reader {
namespace {

bool looksLikeUtf16(const QByteArray &data)
{
    const int sampleSize = static_cast<int>(std::min<qsizetype>(data.size(), 512));
    if (sampleSize < 4)
        return false;

    int zeroOdd = 0;
    int zeroEven = 0;
    const int chars = sampleSize / 2;
    for (int i = 0; i < sampleSize; ++i) {
        if (data.at(i) == 0) {
            if (i & 1)
                ++zeroOdd;
            else
                ++zeroEven;
        }
    }
    return zeroOdd > chars * 0.7 || zeroEven > chars * 0.7;
}

} // namespace

bool isProbablyText(const QByteArray &data, bool allowUtf16)
{
    if (data.isEmpty())
        return false;
    if (data.startsWith("\xef\xbb\xbf")
        || data.startsWith("\xff\xfe")
        || data.startsWith("\xfe\xff")) {
        return true;
    }
    if (allowUtf16 && looksLikeUtf16(data))
        return true;

    const int sampleSize = static_cast<int>(std::min<qsizetype>(data.size(), 8192));
    int zeros = 0;
    int controls = 0;
    int printable = 0;
    for (int i = 0; i < sampleSize; ++i) {
        const auto byte = static_cast<unsigned char>(data.at(i));
        if (byte == 0)
            ++zeros;
        else if (byte < 0x09 || (byte > 0x0d && byte < 0x20) || byte == 0x7f)
            ++controls;
        else if (byte >= 0x20)
            ++printable;
    }
    return zeros * 20 <= sampleSize
        && controls * 20 <= sampleSize
        && printable * 10 >= sampleSize * 9;
}

} // namespace v8reader
