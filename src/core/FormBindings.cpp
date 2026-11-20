#include "core/FormBindings.h"

#include <QRegularExpression>

#include <cmath>
#include <functional>

namespace v8reader {
namespace {

// Разделяет «Строка(50), Число(15,2)» на составной тип, не путая запятую
// внутри квалификаторов числа.
bool looksComposite(const QString &typeText)
{
    int depth = 0;
    for (const QChar ch : typeText) {
        if (ch == QLatin1Char('('))
            ++depth;
        else if (ch == QLatin1Char(')'))
            --depth;
        else if (ch == QLatin1Char(',') && depth == 0)
            return true;
    }
    return false;
}

} // namespace

void FormBindingResolver::clear()
{
    m_byGuid.clear();
}

void FormBindingResolver::addObject(const MetadataObject &object)
{
    auto insert = [this](const MetadataEntry &entry) {
        if (entry.guid.isEmpty() || entry.typeText.isEmpty())
            return;
        Entry value;
        value.name = entry.name;
        value.synonym = entry.synonym;
        value.typeText = entry.typeText;
        m_byGuid.insert(entry.guid.toLower(), value);
    };
    for (const MetadataSection &section : object.sections) {
        for (const MetadataEntry &entry : section.entries) {
            insert(entry);
            for (const MetadataEntry &child : entry.children)
                insert(child);
        }
    }
}

QString FormBindingResolver::typeTextForStandardCode(int code)
{
    switch (code) {
    case -2: // Номер / Код / Номер строки — строковый стандартный реквизит
        return QStringLiteral("Строка(9)");
    case -3: // Дата
        return QStringLiteral("Дата");
    default:
        return QString();
    }
}

FormValueKind FormBindingResolver::kindForTypeText(const QString &typeText)
{
    const QString text = typeText.trimmed();
    if (text.isEmpty())
        return FormValueKind::Unknown;
    if (looksComposite(text))
        return FormValueKind::Composite;
    if (text.startsWith(QStringLiteral("Строка")))
        return FormValueKind::String;
    if (text.startsWith(QStringLiteral("Число")))
        return FormValueKind::Number;
    if (text.startsWith(QStringLiteral("Булево")))
        return FormValueKind::Boolean;
    if (text.startsWith(QStringLiteral("Дата")) || text.startsWith(QStringLiteral("Время")))
        return FormValueKind::Date;
    if (text.contains(QStringLiteral("ХранилищеЗначения"))
        || text.startsWith(QStringLiteral("Картинка"))) {
        return FormValueKind::Picture;
    }
    if (text.contains(QStringLiteral("Ссылка"))) {
        if (text.startsWith(QStringLiteral("Перечисление")))
            return FormValueKind::Enum;
        return FormValueKind::Reference;
    }
    if (text.startsWith(QStringLiteral("Составной")))
        return FormValueKind::Composite;
    return FormValueKind::Unknown;
}

int FormBindingResolver::charsForType(const QString &typeText, FormValueKind kind)
{
    const QString text = typeText.trimmed();
    switch (kind) {
    case FormValueKind::String: {
        static const QRegularExpression pattern(
            QStringLiteral("Строка\\((\\d+)\\)"));
        const QRegularExpressionMatch match = pattern.match(text);
        int length = 0;
        if (match.hasMatch())
            length = match.captured(1).toInt();
        if (length <= 0)
            return 20;
        return qBound(6, length, 40);
    }
    case FormValueKind::Number: {
        static const QRegularExpression pattern(
            QStringLiteral("Число\\((\\d+)(?:\\s*,\\s*(\\d+))?\\)"));
        const QRegularExpressionMatch match = pattern.match(text);
        if (!match.hasMatch())
            return 12;
        const int digits = match.captured(1).toInt();
        const int fraction = match.captured(2).toInt();
        if (digits <= 0)
            return 15;
        // Формула «LogFormElementsDefFilter» из платформы (как в BslEdit).
        int length = digits;
        if (fraction != 0) {
            ++length;
            if (digits == fraction)
                ++length;
        }
        const int whole = digits - fraction;
        int triads = whole / 3;
        if (triads != 0 && whole % 3 == 0)
            --triads;
        length += triads;
        const double converted = length * (length >= 6 ? 5.4 : 5.5) / 8.0;
        return static_cast<int>(std::ceil(converted));
    }
    case FormValueKind::Date:
        if (text.startsWith(QStringLiteral("ДатаВремя")))
            return 17;
        if (text.startsWith(QStringLiteral("Время")))
            return 8;
        return 9;
    case FormValueKind::Boolean:
        return 10;
    case FormValueKind::Reference:
    case FormValueKind::Enum:
    case FormValueKind::Composite:
        return 15;
    case FormValueKind::Picture:
        return 2;
    default:
        return 0;
    }
}

FormBinding FormBindingResolver::resolve(const QVector<FormPathSegment> &path,
                                         const FormDocument &document,
                                         const QString &fallbackName) const
{
    FormBinding binding;
    if (path.isEmpty())
        return binding;

    QStringList parts;
    const Entry *last = nullptr;
    for (int i = 0; i < path.size(); ++i) {
        const FormPathSegment &segment = path.at(i);
        switch (segment.kind) {
        case FormPathSegment::Kind::Index: {
            if (i == 0) {
                // Корневой реквизит формы: «Объект», «Список», «Отчет» …
                // Идентификаторы реквизитов формы нумеруются с единицы,
                // а в документе они лежат в порядке объявления.
                const int index = segment.code - 1;
                if (index >= 0 && index < document.attributes.size()) {
                    const FormAttribute &attribute = document.attributes.at(index);
                    parts.append(attribute.name);
                    // Путь из одного сегмента — это сам реквизит формы:
                    // его тип объявлен в самой форме (например, «Булево»).
                    if (path.size() == 1 && !attribute.typeHint.isEmpty()) {
                        binding.typeText = attribute.typeHint;
                        binding.kind = kindForTypeText(attribute.typeHint);
                        binding.chars = charsForType(attribute.typeHint, binding.kind);
                        binding.title = attribute.synonym;
                    }
                } else {
                    parts.append(QStringLiteral("Объект"));
                }
            } else {
                parts.append(QStringLiteral("#%1").arg(segment.code));
            }
            break;
        }
        case FormPathSegment::Kind::Attribute: {
            const auto it = m_byGuid.constFind(segment.guid.toLower());
            if (it == m_byGuid.constEnd()) {
                if (i == path.size() - 1 && !fallbackName.isEmpty())
                    parts.append(fallbackName);
                else
                    parts.append(QStringLiteral("?"));
                break;
            }
            parts.append(it->name);
            last = &it.value();
            break;
        }
        case FormPathSegment::Kind::StandardAttribute: {
            const QString standard = typeTextForStandardCode(segment.code);
            if (i == path.size() - 1 && !fallbackName.isEmpty()) {
                parts.append(fallbackName);
            } else {
                parts.append(QStringLiteral("Стандартный%1").arg(segment.code));
            }
            if (!standard.isEmpty()) {
                binding.typeText = standard;
                binding.kind = kindForTypeText(standard);
                binding.chars = charsForType(standard, binding.kind);
            }
            break;
        }
        }
    }

    binding.text = parts.join(QLatin1Char('.'));
    if (last) {
        binding.title = last->synonym;
        if (binding.typeText.isEmpty()) {
            binding.typeText = last->typeText;
            binding.kind = kindForTypeText(binding.typeText);
            binding.chars = charsForType(binding.typeText, binding.kind);
        }
    }
    return binding;
}

void FormBindingResolver::applyToDocument(FormDocument *document) const
{
    if (!document)
        return;

    std::function<void(QVector<FormItem> &)> apply = [&](QVector<FormItem> &items) {
        for (FormItem &item : items) {
            if (!item.dataPath.isEmpty()) {
                const FormBinding binding = resolve(item.dataPath, *document, item.name);
                item.dataPathText = binding.text;
                if (!binding.title.isEmpty())
                    item.dataTitle = binding.title;
                if (!binding.typeText.isEmpty()) {
                    item.typeText = binding.typeText;
                    item.valueKind = binding.kind;
                    item.valueChars = binding.chars;
                }
            }
            apply(item.children);
        }
    };
    apply(document->items);
}

} // namespace v8reader
