#include "core/MetadataCatalog.h"

#include <QHash>

namespace v8reader {
namespace {

struct CategoryDefinition
{
    const char *guid;
    const char *title;
    const char *typeTitle;
};

// GUID-ы корневых коллекций метаданных 1С:8.
const CategoryDefinition kCategories[] = {
    {"0195e80c-b157-11d4-9435-004095e12fc7", "Константы", "Константа"},
    {"cf4abea6-37b2-11d4-940f-008048da11f9", "Справочники", "Справочник"},
    {"061d872a-5787-460e-95ac-ed74ea3a3e84", "Документы", "Документ"},
    {"36a8e346-9aaa-4af9-bdbd-83be3c177977", "Нумераторы", "Нумератор"},
    {"4612bd75-71b7-4a5c-8cc5-2b0b65f9fa0d", "Журналы документов", "Журнал документов"},
    {"f6a80749-5ad7-400b-8519-39dc5dff2542", "Перечисления", "Перечисление"},
    {"631b75a0-29e2-11d6-a3c7-0050bae0a776", "Отчеты", "Отчет"},
    {"bf845118-327b-4682-b5c6-285d2a0eb296", "Обработки", "Обработка"},
    {"857c4a91-e5f4-4fac-86ec-787626f1c108", "Планы обмена", "План обмена"},
    {"238e7e88-3c5f-48b2-8a3b-81ebbecb20ed", "Планы счетов", "План счетов"},
    {"82a1b659-b220-4d94-a9bd-14d757b95a48", "Планы видов характеристик",
     "План видов характеристик"},
    {"30b100d6-b29f-47ac-aec7-cb8ca8a54767", "Планы видов расчета",
     "План видов расчета"},
    {"13134201-f60b-11d5-a3c7-0050bae0a776", "Регистры сведений", "Регистр сведений"},
    {"b64d9a40-1642-11d6-a3c7-0050bae0a776", "Регистры накопления",
     "Регистр накопления"},
    {"2deed9b8-0056-4ffe-a473-c20a6c32a0bc", "Регистры бухгалтерии",
     "Регистр бухгалтерии"},
    {"f2de87a8-64e5-45eb-a22d-b3aedab050e7", "Регистры расчета", "Регистр расчета"},
    {"bc587f20-35d9-11d6-a3c7-0050bae0a776", "Последовательности", "Последовательность"},
    {"fcd3404e-1523-48ce-9bc0-ecdb822684a1", "Бизнес-процессы", "Бизнес-процесс"},
    {"3e63355c-1378-4953-be9b-1deb5fb6bec5", "Задачи", "Задача"},
    {"5274d9fc-9c3a-4a71-8f5e-a0db8ab23de5", "Внешние источники данных",
     "Внешний источник данных"},

    {"0fe48980-252d-11d6-a3c7-0050bae0a776", "Общие модули", "Общий модуль"},
    {"07ee8426-87f1-11d5-b99c-0050bae0a95d", "Общие формы", "Общая форма"},
    {"0c89c792-16c3-11d5-b96b-0050bae0a95d", "Общие макеты", "Общий макет"},
    {"7dcd43d9-aca5-4926-b549-1842e6a4e8cf", "Общие картинки", "Общая картинка"},
    {"2f1a5187-fb0e-4b05-9489-dc5dd6412348", "Общие команды", "Общая команда"},
    {"1c57eabe-7349-44b3-b1de-ebfeab67b47d", "Группы команд", "Группа команд"},
    {"15794563-ccec-41f6-a83c-ec5f7b9a5bc1", "Общие реквизиты", "Общий реквизит"},
    {"09736b02-9cac-4e3f-b4f7-d3e9576ab948", "Роли", "Роль"},
    {"37f2fa9a-b276-11d4-9435-004095e12fc7", "Подсистемы", "Подсистема"},
    {"24c43748-c938-45d0-8d14-01424a72b11e", "Параметры сеанса", "Параметр сеанса"},
    {"af547940-3268-434f-a3e7-e47d6d2638c3", "Функциональные опции",
     "Функциональная опция"},
    {"30d554db-541e-4f62-8970-a1c6dcfeb2bc", "Параметры функциональных опций",
     "Параметр функциональной опции"},
    {"c045099e-13b9-4fb6-9d50-fca00202971e", "Определяемые типы", "Определяемый тип"},
    {"3e7bfcc0-067d-11d6-a3c7-0050bae0a776", "Критерии отбора", "Критерий отбора"},
    {"4e828da6-0f44-4b5b-b1c0-a2b3cfe7bdcc", "Подписки на события",
     "Подписка на событие"},
    {"11bdaf85-d5ad-4d91-bb24-aa0eee139052", "Регламентные задания",
     "Регламентное задание"},
    {"46b4cd97-fd13-4eaa-aba2-3bddd7699218", "Хранилища настроек", "Хранилище настроек"},
    {"3e5404af-6ef8-4c73-ad11-91bd2dfac4c8", "Стили", "Стиль"},
    {"58848766-36ea-4076-8800-e91eb49590d7", "Элементы стиля", "Элемент стиля"},
    {"9cd510ce-abfc-11d4-9434-004095e12fc7", "Языки", "Язык"},
    {"39bddf6a-0c3c-452b-921c-d99cfa1c2f1b", "Интерфейсы", "Интерфейс"},
    {"0fffc09c-8f4c-47cc-b41c-8d5c5a221d79", "HTTP-сервисы", "HTTP-сервис"},
    {"8657032e-7740-4e1d-a3ba-5dd6e8afb78f", "Web-сервисы", "Web-сервис"},
    {"d26096fb-7a5d-4df9-af63-47d04771fa9b", "WS-ссылки", "WS-ссылка"},
    {"cc9df798-7c94-4616-97d2-7aa0b7bc515e", "XDTO-пакеты", "XDTO-пакет"},
    {"6e6dc072-b7ac-41e7-8f88-278d25b6da2a", "Боты", "Бот"},
    {"a7641777-7813-45c6-96ef-9d51587a6ac6", "WebSocket-клиенты", "WebSocket-клиент"},
    {"bf3420b0-f6f9-41a0-b83a-fe9d4ab0b65d", "Сервисы интеграции", "Сервис интеграции"},
};

struct TableGuidDefinition
{
    const char *guid;
    MetadataSectionKind kind;
};

// GUID-ы таблиц состава, встречающиеся в текстах объектов метаданных.
// Часть из них общая для всех типов, часть зависит от типа и версии формата.
const TableGuidDefinition kSectionTableGuids[] = {
    // Общие.
    {"3daea016-69b7-4ed4-9453-127911372fe6", MetadataSectionKind::Templates},

    // Справочники (и родственные объекты в формате 1.5).
    {"4fe87c89-9ad4-43f6-9fdb-9dc83b3879c6", MetadataSectionKind::Commands},
    {"932159f9-95b2-4e76-a8dd-8849fe5c5ded", MetadataSectionKind::TabularSections},
    {"cf4abea7-37b2-11d4-940f-008048da11f9", MetadataSectionKind::Attributes},
    {"fdf816d2-1ead-11d5-b975-0050bae0a95d", MetadataSectionKind::Forms},
    {"888744e1-b616-11d4-9436-004095e12fc7", MetadataSectionKind::TabularAttributes},

    // Универсальные таблицы формата 1.5 (внешние обработки и т.п.).
    {"2bcef0d1-0981-11d6-b9b8-0050bae0a95d", MetadataSectionKind::TabularSections},
    {"d5b0e5ed-256d-401c-9c36-f630cafd8a62", MetadataSectionKind::Forms},
    {"ec6bb5e5-b7a8-4d75-bec9-658107a699cf", MetadataSectionKind::Attributes},
    {"45556acb-826a-4f73-898a-6025fc9536e1", MetadataSectionKind::Commands},

    // Документы (формат 1.6).
    {"21c53e09-8950-4b5e-a6a0-1054f1bbc274", MetadataSectionKind::TabularSections},
    {"45e46cbc-3e24-4165-8b7b-cc98a6f80211", MetadataSectionKind::Attributes},
    {"fb880e93-47d7-4127-9357-a20e69c17545", MetadataSectionKind::Forms},
    {"b544fc6a-2ba3-4885-8fb2-cb289fb6d65e", MetadataSectionKind::Commands},
    {"a49a35ce-120a-4c80-8eea-b0618479cd70", MetadataSectionKind::Commands},

    // Прочие таблицы команд (известные GUID-ы 1С).
    {"f27c2152-a2c9-4c30-adb1-130f5eb2590f", MetadataSectionKind::Commands},
    {"7a3e533c-f232-40d5-a932-6a311d2480bf", MetadataSectionKind::Commands},
    {"b44ba719-945c-445c-8aab-1088fa4df16e", MetadataSectionKind::Commands},
    {"7162da60-f7fe-4d78-ad5d-e31700f9af18", MetadataSectionKind::Commands},
    {"d5207c64-11d5-4d46-bba2-55b7b07ff4eb", MetadataSectionKind::Commands},
    {"0df30176-6865-4787-9fc8-609eb144174f", MetadataSectionKind::Commands},
    {"99f328af-a77f-4572-a2d8-80ed20c81890", MetadataSectionKind::Commands},

    // Критерии отбора: формы и команды.
    {"00867c40-06b1-11d6-a3c7-0050bae0a776", MetadataSectionKind::Forms},
    {"23fa3b84-220a-40e9-8331-e588bed87f7d", MetadataSectionKind::Commands},

    // Хранилища настроек: формы.
    {"b8533c0c-2342-4db3-91a2-c2b08cbf6b23", MetadataSectionKind::Forms},
};

QString lower(const char *value)
{
    return QString::fromLatin1(value).toLower();
}

MetadataLayout makeLayout(std::initializer_list<SectionLayout> sections, bool hasTabulars)
{
    MetadataLayout layout;
    for (const SectionLayout &section : sections)
        layout.sections.append(section);
    layout.hasTabularSections = hasTabulars;
    return layout;
}

const QHash<QString, MetadataLayout> &layouts()
{
    static const QHash<QString, MetadataLayout> result = [] {
        QHash<QString, MetadataLayout> map;
        const auto add = [&map](const char *categoryGuid, MetadataLayout layout) {
            map.insert(lower(categoryGuid), std::move(layout));
        };

        // Справочники: макеты 3, команды 4, табличные части 5,
        // реквизиты 6, формы 7.
        add("cf4abea6-37b2-11d4-940f-008048da11f9",
            makeLayout({{MetadataSectionKind::Templates, 3},
                        {MetadataSectionKind::Commands, 4},
                        {MetadataSectionKind::TabularSections, 5},
                        {MetadataSectionKind::Attributes, 6},
                        {MetadataSectionKind::Forms, 7}},
                       true));

        // Документы: табличные части 3, макеты 4, реквизиты 5,
        // команды 6, формы 7.
        add("061d872a-5787-460e-95ac-ed74ea3a3e84",
            makeLayout({{MetadataSectionKind::TabularSections, 3},
                        {MetadataSectionKind::Templates, 4},
                        {MetadataSectionKind::Attributes, 5},
                        {MetadataSectionKind::Commands, 6},
                        {MetadataSectionKind::Forms, 7}},
                       true));

        // Журналы документов: макеты 3, графы 4, команды 5, формы 6.
        add("4612bd75-71b7-4a5c-8cc5-2b0b65f9fa0d",
            makeLayout({{MetadataSectionKind::Templates, 3},
                        {MetadataSectionKind::JournalColumns, 4},
                        {MetadataSectionKind::Commands, 5},
                        {MetadataSectionKind::Forms, 6}},
                       false));

        // Перечисления: формы 3, макеты 4, команды 5, значения 6.
        add("f6a80749-5ad7-400b-8519-39dc5dff2542",
            makeLayout({{MetadataSectionKind::Forms, 3},
                        {MetadataSectionKind::Templates, 4},
                        {MetadataSectionKind::Commands, 5},
                        {MetadataSectionKind::Values, 6}},
                       false));

        // Отчеты: макеты 3, реквизиты 4, формы 5, табличные части 6, команды 7.
        add("631b75a0-29e2-11d6-a3c7-0050bae0a776",
            makeLayout({{MetadataSectionKind::Templates, 3},
                        {MetadataSectionKind::Attributes, 4},
                        {MetadataSectionKind::Forms, 5},
                        {MetadataSectionKind::TabularSections, 6},
                        {MetadataSectionKind::Commands, 7}},
                       true));

        // Обработки: табличные части 3, макеты 4, команды 5,
        // формы 6, реквизиты 7.
        add("bf845118-327b-4682-b5c6-285d2a0eb296",
            makeLayout({{MetadataSectionKind::TabularSections, 3},
                        {MetadataSectionKind::Templates, 4},
                        {MetadataSectionKind::Commands, 5},
                        {MetadataSectionKind::Forms, 6},
                        {MetadataSectionKind::Attributes, 7}},
                       true));

        // Планы обмена: реквизиты 3, макеты 4, табличные части 5,
        // формы 6, команды 7.
        add("857c4a91-e5f4-4fac-86ec-787626f1c108",
            makeLayout({{MetadataSectionKind::Attributes, 3},
                        {MetadataSectionKind::Templates, 4},
                        {MetadataSectionKind::TabularSections, 5},
                        {MetadataSectionKind::Forms, 6},
                        {MetadataSectionKind::Commands, 7}},
                       true));

        // Планы счетов: макеты 3, табличные части 5, формы 6,
        // реквизиты 7, признаки учета 8, признаки учета субконто 9.
        add("238e7e88-3c5f-48b2-8a3b-81ebbecb20ed",
            makeLayout({{MetadataSectionKind::Templates, 3},
                        {MetadataSectionKind::TabularSections, 5},
                        {MetadataSectionKind::Forms, 6},
                        {MetadataSectionKind::Attributes, 7},
                        {MetadataSectionKind::AccountingFlags, 8},
                        {MetadataSectionKind::SubcontoFlags, 9}},
                       true));

        // Планы видов характеристик: реквизиты 3, макеты 4,
        // табличные части 5, команды 6, формы 7.
        add("82a1b659-b220-4d94-a9bd-14d757b95a48",
            makeLayout({{MetadataSectionKind::Attributes, 3},
                        {MetadataSectionKind::Templates, 4},
                        {MetadataSectionKind::TabularSections, 5},
                        {MetadataSectionKind::Commands, 6},
                        {MetadataSectionKind::Forms, 7}},
                       true));

        // Планы видов расчета: табличные части 3, реквизиты 4,
        // команды 5, макеты 6, формы 7.
        add("30b100d6-b29f-47ac-aec7-cb8ca8a54767",
            makeLayout({{MetadataSectionKind::TabularSections, 3},
                        {MetadataSectionKind::Attributes, 4},
                        {MetadataSectionKind::Commands, 5},
                        {MetadataSectionKind::Templates, 6},
                        {MetadataSectionKind::Forms, 7}},
                       true));

        // Регистры сведений: ресурсы 3, измерения 4, формы 5,
        // макеты 6, реквизиты 7, команды 8.
        add("13134201-f60b-11d5-a3c7-0050bae0a776",
            makeLayout({{MetadataSectionKind::Resources, 3},
                        {MetadataSectionKind::Dimensions, 4},
                        {MetadataSectionKind::Forms, 5},
                        {MetadataSectionKind::Templates, 6},
                        {MetadataSectionKind::Attributes, 7},
                        {MetadataSectionKind::Commands, 8}},
                       false));

        // Регистры накопления: макеты 3, команды 4, ресурсы 5,
        // реквизиты 6, измерения 7, формы 8.
        add("b64d9a40-1642-11d6-a3c7-0050bae0a776",
            makeLayout({{MetadataSectionKind::Templates, 3},
                        {MetadataSectionKind::Commands, 4},
                        {MetadataSectionKind::Resources, 5},
                        {MetadataSectionKind::Attributes, 6},
                        {MetadataSectionKind::Dimensions, 7},
                        {MetadataSectionKind::Forms, 8}},
                       false));

        // Регистры бухгалтерии: измерения 3, макеты 4, ресурсы 5,
        // команды 6, реквизиты 7, формы 8.
        add("2deed9b8-0056-4ffe-a473-c20a6c32a0bc",
            makeLayout({{MetadataSectionKind::Dimensions, 3},
                        {MetadataSectionKind::Templates, 4},
                        {MetadataSectionKind::Resources, 5},
                        {MetadataSectionKind::Commands, 6},
                        {MetadataSectionKind::Attributes, 7},
                        {MetadataSectionKind::Forms, 8}},
                       false));

        // Регистры расчета: реквизиты 3, макеты 5, ресурсы 6,
        // формы 7, команды 8, измерения 9.
        add("f2de87a8-64e5-45eb-a22d-b3aedab050e7",
            makeLayout({{MetadataSectionKind::Attributes, 3},
                        {MetadataSectionKind::Templates, 5},
                        {MetadataSectionKind::Resources, 6},
                        {MetadataSectionKind::Forms, 7},
                        {MetadataSectionKind::Commands, 8},
                        {MetadataSectionKind::Dimensions, 9}},
                       false));

        // Бизнес-процессы: макеты 3, формы 4, команды 5,
        // реквизиты 6, табличные части 7.
        add("fcd3404e-1523-48ce-9bc0-ecdb822684a1",
            makeLayout({{MetadataSectionKind::Templates, 3},
                        {MetadataSectionKind::Forms, 4},
                        {MetadataSectionKind::Commands, 5},
                        {MetadataSectionKind::Attributes, 6},
                        {MetadataSectionKind::TabularSections, 7}},
                       true));

        // Задачи: макеты 3, формы 4, реквизиты 5,
        // табличные части 7, команды 8.
        add("3e63355c-1378-4953-be9b-1deb5fb6bec5",
            makeLayout({{MetadataSectionKind::Templates, 3},
                        {MetadataSectionKind::Forms, 4},
                        {MetadataSectionKind::Attributes, 5},
                        {MetadataSectionKind::TabularSections, 7},
                        {MetadataSectionKind::Commands, 8}},
                       true));

        // Критерии отбора: формы и команды. Таблицы ищутся по GUID
        // (00867c40-… — формы, 23fa3b84-… — команды), индексы не фиксированы.
        add("3e7bfcc0-067d-11d6-a3c7-0050bae0a776",
            makeLayout({{MetadataSectionKind::Forms, -1},
                        {MetadataSectionKind::Commands, -1}},
                       false));

        // Хранилища настроек: формы и макеты. Таблицы ищутся по GUID
        // (b8533c0c-… — формы, 3daea016-… — макеты).
        add("46b4cd97-fd13-4eaa-aba2-3bddd7699218",
            makeLayout({{MetadataSectionKind::Forms, -1},
                        {MetadataSectionKind::Templates, -1}},
                       false));

        return map;
    }();
    return result;
}

QString sectionGuidString(MetadataSectionKind kind)
{
    switch (kind) {
    case MetadataSectionKind::Templates:
        return QStringLiteral("3daea016-69b7-4ed4-9453-127911372fe6");
    case MetadataSectionKind::Commands:
        return QStringLiteral("4fe87c89-9ad4-43f6-9fdb-9dc83b3879c6");
    case MetadataSectionKind::TabularSections:
        return QStringLiteral("932159f9-95b2-4e76-a8dd-8849fe5c5ded");
    case MetadataSectionKind::Attributes:
        return QStringLiteral("cf4abea7-37b2-11d4-940f-008048da11f9");
    case MetadataSectionKind::Forms:
        return QStringLiteral("fdf816d2-1ead-11d5-b975-0050bae0a95d");
    case MetadataSectionKind::TabularAttributes:
        return QStringLiteral("888744e1-b616-11d4-9436-004095e12fc7");
    case MetadataSectionKind::Dimensions:
    case MetadataSectionKind::Resources:
    case MetadataSectionKind::AccountingFlags:
    case MetadataSectionKind::SubcontoFlags:
    case MetadataSectionKind::Values:
    case MetadataSectionKind::Columns:
    case MetadataSectionKind::JournalColumns:
        break;
    }
    return QString();
}

} // namespace

const QVector<MetadataCategoryInfo> &MetadataCatalog::categories()
{
    static const QVector<MetadataCategoryInfo> result = [] {
        QVector<MetadataCategoryInfo> list;
        list.reserve(static_cast<int>(sizeof(kCategories) / sizeof(kCategories[0])));
        for (const CategoryDefinition &definition : kCategories) {
            list.append(MetadataCategoryInfo{
                QString::fromLatin1(definition.guid),
                QString::fromUtf8(definition.title),
                QString::fromUtf8(definition.typeTitle),
            });
        }
        return list;
    }();
    return result;
}

const MetadataCategoryInfo *MetadataCatalog::category(const QString &guid)
{
    for (const MetadataCategoryInfo &info : categories()) {
        if (info.guid.compare(guid, Qt::CaseInsensitive) == 0)
            return &info;
    }
    return nullptr;
}

QString MetadataCatalog::sectionGuid(MetadataSectionKind kind)
{
    return sectionGuidString(kind);
}

QString MetadataCatalog::sectionTitle(MetadataSectionKind kind)
{
    switch (kind) {
    case MetadataSectionKind::Templates:
        return QStringLiteral("Макеты");
    case MetadataSectionKind::Commands:
        return QStringLiteral("Команды");
    case MetadataSectionKind::TabularSections:
        return QStringLiteral("Табличные части");
    case MetadataSectionKind::Attributes:
        return QStringLiteral("Реквизиты");
    case MetadataSectionKind::Forms:
        return QStringLiteral("Формы");
    case MetadataSectionKind::TabularAttributes:
        return QStringLiteral("Реквизиты табличной части");
    case MetadataSectionKind::Dimensions:
        return QStringLiteral("Измерения");
    case MetadataSectionKind::Resources:
        return QStringLiteral("Ресурсы");
    case MetadataSectionKind::AccountingFlags:
        return QStringLiteral("Признаки учета");
    case MetadataSectionKind::SubcontoFlags:
        return QStringLiteral("Признаки учета субконто");
    case MetadataSectionKind::Values:
        return QStringLiteral("Значения");
    case MetadataSectionKind::Columns:
        return QStringLiteral("Колонки");
    case MetadataSectionKind::JournalColumns:
        return QStringLiteral("Графы");
    }
    return QStringLiteral("Состав");
}

QString MetadataCatalog::sectionEntryTitle(MetadataSectionKind kind)
{
    switch (kind) {
    case MetadataSectionKind::Templates:
        return QStringLiteral("Макет");
    case MetadataSectionKind::Commands:
        return QStringLiteral("Команда");
    case MetadataSectionKind::TabularSections:
        return QStringLiteral("Табличная часть");
    case MetadataSectionKind::Attributes:
        return QStringLiteral("Реквизит");
    case MetadataSectionKind::Forms:
        return QStringLiteral("Форма");
    case MetadataSectionKind::TabularAttributes:
        return QStringLiteral("Реквизит");
    case MetadataSectionKind::Dimensions:
        return QStringLiteral("Измерение");
    case MetadataSectionKind::Resources:
        return QStringLiteral("Ресурс");
    case MetadataSectionKind::AccountingFlags:
        return QStringLiteral("Признак учета");
    case MetadataSectionKind::SubcontoFlags:
        return QStringLiteral("Признак учета субконто");
    case MetadataSectionKind::Values:
        return QStringLiteral("Значение");
    case MetadataSectionKind::Columns:
        return QStringLiteral("Колонка");
    case MetadataSectionKind::JournalColumns:
        return QStringLiteral("Графа");
    }
    return QStringLiteral("Элемент");
}

bool MetadataCatalog::sectionKindForGuid(const QString &guid, MetadataSectionKind *kind)
{
    return sectionKindForTableGuid(guid, kind);
}

QVector<MetadataSectionKind> MetadataCatalog::sectionKinds()
{
    return {
        MetadataSectionKind::Templates,
        MetadataSectionKind::Commands,
        MetadataSectionKind::TabularSections,
        MetadataSectionKind::Attributes,
        MetadataSectionKind::Forms,
        MetadataSectionKind::TabularAttributes,
    };
}

const MetadataLayout *MetadataCatalog::layoutForCategory(const QString &categoryGuid)
{
    const auto it = layouts().constFind(categoryGuid.toLower());
    if (it == layouts().constEnd())
        return nullptr;
    return &it.value();
}

bool MetadataCatalog::sectionKindForTableGuid(const QString &tableGuid, MetadataSectionKind *kind)
{
    const QString normalized = tableGuid.trimmed().toLower();
    if (normalized.isEmpty())
        return false;

    for (const TableGuidDefinition &definition : kSectionTableGuids) {
        if (normalized == lower(definition.guid)) {
            if (kind)
                *kind = definition.kind;
            return true;
        }
    }
    return false;
}

bool MetadataCatalog::hasComposition(const QString &categoryGuid)
{
    return layoutForCategory(categoryGuid) != nullptr;
}

QString MetadataCatalog::zeroGuid()
{
    return QStringLiteral("00000000-0000-0000-0000-000000000000");
}

} // namespace v8reader
