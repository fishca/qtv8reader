#include "gui/BslHighlightSettings.h"

namespace v8reader {
namespace {

QColor readColor(QSettings &settings, const char *name, const QColor &fallback)
{
    const QColor color = settings.value(QString::fromLatin1(name)).value<QColor>();
    return color.isValid() ? color : fallback;
}

} // namespace

BslHighlightSettings BslHighlightSettings::defaults(bool dark)
{
    BslHighlightSettings settings;
    settings.custom = false;
    if (dark) {
        settings.keyword = QColor(0x56, 0x9C, 0xD6);
        settings.directive = QColor(0xC5, 0x86, 0xC0);
        settings.annotation = QColor(0x4E, 0xC9, 0xB0);
        settings.number = QColor(0xB5, 0xCE, 0xA8);
        settings.string = QColor(0xCE, 0x91, 0x78);
        settings.comment = QColor(0x6A, 0x99, 0x55);
    } else {
        settings.keyword = QColor(0x00, 0x00, 0xA0);
        settings.directive = QColor(0x80, 0x00, 0x80);
        settings.annotation = QColor(0x00, 0x80, 0x80);
        settings.number = QColor(0x99, 0x33, 0x33);
        settings.string = QColor(0xA3, 0x15, 0x15);
        settings.comment = QColor(0x00, 0x80, 0x00);
    }
    return settings;
}

BslHighlightSettings BslHighlightSettings::load(QSettings &settings, bool dark)
{
    BslHighlightSettings result = defaults(dark);
    settings.beginGroup(QStringLiteral("BslHighlight"));

    result.enabled = settings.value(QStringLiteral("enabled"), true).toBool();
    result.boldKeywords = settings.value(QStringLiteral("boldKeywords"), true).toBool();
    result.boldDirectives =
        settings.value(QStringLiteral("boldDirectives"), true).toBool();
    result.italicComments =
        settings.value(QStringLiteral("italicComments"), true).toBool();

    const bool hasCustom = settings.contains(QStringLiteral("keyword"))
        || settings.contains(QStringLiteral("string"))
        || settings.contains(QStringLiteral("comment"));
    if (hasCustom) {
        result.custom = true;
        result.keyword = readColor(settings, "keyword", result.keyword);
        result.directive = readColor(settings, "directive", result.directive);
        result.annotation = readColor(settings, "annotation", result.annotation);
        result.number = readColor(settings, "number", result.number);
        result.string = readColor(settings, "string", result.string);
        result.comment = readColor(settings, "comment", result.comment);
    }
    settings.endGroup();
    return result;
}

void BslHighlightSettings::save(QSettings &settings) const
{
    settings.beginGroup(QStringLiteral("BslHighlight"));
    settings.setValue(QStringLiteral("enabled"), enabled);
    settings.setValue(QStringLiteral("boldKeywords"), boldKeywords);
    settings.setValue(QStringLiteral("boldDirectives"), boldDirectives);
    settings.setValue(QStringLiteral("italicComments"), italicComments);
    if (custom) {
        settings.setValue(QStringLiteral("keyword"), keyword);
        settings.setValue(QStringLiteral("directive"), directive);
        settings.setValue(QStringLiteral("annotation"), annotation);
        settings.setValue(QStringLiteral("number"), number);
        settings.setValue(QStringLiteral("string"), string);
        settings.setValue(QStringLiteral("comment"), comment);
    } else {
        for (const char *name : {"keyword", "directive", "annotation",
                                 "number", "string", "comment"}) {
            settings.remove(QString::fromLatin1(name));
        }
    }
    settings.endGroup();
}

} // namespace v8reader
