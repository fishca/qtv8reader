# Спецификация: чтение таблиц состава объектов метаданных 1С:Предприятие 8

Источник: эталонный проект `C:\work\cpp\Projects\v8_reader-main\src` (Delphi/C++Builder).
Все утверждения снабжены ссылками `файл:строка`. Пути вида `{0,1,1,1,2}` — это путь от
корневого узла дерева разбора (`tree* root`), где `root[0]` — внешний список текста
элемента контейнера (см. п.0).

---

## 0. Модель дерева: почему все пути начинаются с `[0]`

1. Текст файла контейнера читается `get_treeFromV8file()` — `Class_1CD.cpp:196-240`
   (чтение байтов, определение кодировки, конвертация в Unicode, `parse_1Ctext`).
2. `parse_1Ctext()` создаёт **синтетический корень** `nd_list`:
   `ret = new tree(L"", nd_list, NULL)` — `Parse_tree.cpp:657`, `t = ret` — `Parse_tree.cpp:658`.
   На каждый `{` создаётся дочерний список: `t = new tree(L"", nd_list, t)` — `Parse_tree.cpp:681`.
   Поэтому для текста `{1, ...}`:
   * `root` — синтетический узел (`nd_list`, без значения);
   * `root[0]` — **внешний список** объекта (`{1, <свойства>, <N>, <таблица1>, ...}`);
   * `root[1]`, `root[2]`, … — не используются (в тексте всего один корневой список).
   Отсюда все рабочие пути в коде начинаются с `{0, …}`.
3. Формат внешнего списка объекта (`root[0]`):
   ```
   root[0] = { 1, <список свойств>, N, <таблица 1>, ..., <таблица N> }
   таблица  = { <GUID таблицы>, K, <элемент 1>, ..., <элемент K> }
   ```
   * `root[0][2]` — количество таблиц `N`. При чтении **состава** объектов оно не
     используется; единственное чтение в эталоне — `Class_1CD.cpp:8279-8282` (разбор
     объекта «Конфигурация»): `numnode = (*tr)[0][2]`, таблица `i` — `(*tr)[0][3+i]`,
     GUID таблицы — `node[0]`. Это независимо подтверждает инвариант «таблицы с индекса 3».
     Там же видно, что у объекта «Конфигурация» таблица имеет вид `{GUID, <вложенный
     список>}` (`node[1][1]` — `Class_1CD.cpp:8286`), т.е. раскладка payload'а таблицы
     отличается от объектов метаданных (`{GUID, K, элементы…}`).
   * элемент `i` таблицы `T` = `root[0][T][i + 2]`
     (в коде: `Delta = Count - 2`, затем индекс `i + Count - Delta`; так как
     `Count - Delta == 2`, фактический индекс всегда `i + 2` —
     `MetadataObjectWithSections.cpp:106-111`, `MetadataObjectInformationRegister.cpp:60-65`).
   * `root[0][T][0]` — узел GUID таблицы, `root[0][T][1]` — узел количества `K`.
4. Проверено эмпирически на реальном объекте (см. п.7): у документа «АвансовыйОтчет»
   конфигурации «Бухгалтерия предприятия 3.0» (формат контейнера 1.6)
   `root[0]` = `{'1', список из 53 свойств, '5', 5 таблиц}`; все 5 таблиц лежат по индексам
   3…7, сумма их количеств = 5 = `root[0][2]`.
5. Вспомогательные функции:
   * `GetNodeByPath()` — `MetadataObjectWithSections.cpp:39-71` (безопасный спуск по пути);
   * `find_node_by_guid()` — `Parse_tree.cpp:1123-1144` (первый узел с нужным значением);
   * `find_metadata_node_by_guid()` — `Parse_tree.cpp:1146-1189`: ищет узел со значением
     `target_guid`, предпочитая тот, у которого **следующий sibling имеет тип `nd_number`**
     (т.е. «{GUID, количество, …}»), иначе возвращает первый найденный;
   * `FindFirstGuid()` — `MetadataObjectWithSections.cpp:20-37` (и дубли в
     `ChartOfAccounts.cpp:13-30`, `ChartOfCharacteristicTypes.cpp:13-30`,
     `MetadataObjectInformationRegister.cpp:13-30`): DFS-поиск первого значения,
     проходящего тест `ModuleTextStorage::IsGuidLike()` (`ModuleTextStorage.cpp:472-497`).

---

## 1. Сводная таблица соответствия «тип → индексы и пути»

### 1.1 Объекты на `MetadataObjectWithSections`
Базовый класс: `MetadataObjectWithSections` — `MetadataObjectWithSections.h:29-66`,
конфигурация путей — `struct MetadataTreePaths` — `MetadataObjectWithSections.h:13-25`,
общий алгоритм — `initializeFromTreeWithPaths()` — `MetadataObjectWithSections.cpp:99-217`.
Все такие объекты читают: реквизиты → ТЧ → формы → команды → макеты.
Имя формы берётся через `paths.getFormNameFunc` (`MetadataObjectWithSections.cpp:167`),
имя макета — **всегда** через `GetNameMoxCatalogs` (`MetadataObjectWithSections.cpp:209`).

| Тип (рус.) | GUID категории (`guids.h`) | Класс эталона | attIdx | tabIdx | formsIdx | cmdIdx | moxIdx | attItemPath | tabItemPath | cmdItemPath | hasTabulars | Имя формы | Имя макета | Источник |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Справочники | `cf4abea6-37b2-11d4-940f-008048da11f9` (`guids.h:138`) | `TCatalogs` | 6 | 5 | 7 | 4 | 3 | `{0,1,1,1,2}` | `{0,1,5,1,2}` | `{0,1,3,2,9,2}` | true | `GetNameFormCatalogs` | `GetNameMoxCatalogs` | `Catalogs.cpp:10-24`, `:49-52` |
| Документы | `061d872a-5787-460e-95ac-ed74ea3a3e84` (`guids.h:140`) | `TDocuments` | 5 | 3 | 7 | 6 | 4 | `{0,1,1,1,2}` | `{0,1,5,1,2}` | `{0,1,3,2,9,2}` | true | `GetNameFormCatalogs` | `GetNameMoxCatalogs` | `Documents.cpp:10-24`, `:49-52` |
| Журналы документов | `4612bd75-71b7-4a5c-8cc5-2b0b65f9fa0d` (`guids.h:143`) | `TJournals` | 4 | 3 (не исп.) | 6 | 5 | 3 | `{0,1,2}` | `{0,1,5,1,2}` | `{0,1,2,9,2}` | **false** | `GetNameFormCatalogs` | `GetNameMoxCatalogs` | `Journals.cpp:10-24`, `:22` |
| Отчеты | `631b75a0-29e2-11d6-a3c7-0050bae0a776` (`guids.h:145`) | `TReports` | 4 | 6 | 5 | 7 | 3 | `{0,1,1,1,2}` | `{0,1,5,1,2}` | `{0,1,2,9,2}` | true | `GetNameFormReports` | `GetNameMoxCatalogs` | `Reports.cpp:10-24` |
| Обработки | `bf845118-327b-4682-b5c6-285d2a0eb296` (`guids.h:146`) | `TDataProcessors` | 7 | 3 | 6 | 5 | 4 | `{0,1,1,1,2}` | `{0,1,5,1,2}` | `{0,1,2,9,2}` | true | `GetNameFormReports` | `GetNameMoxCatalogs` | `DataProcessors.cpp:10-24` |
| Бизнес-процессы | `fcd3404e-1523-48ce-9bc0-ecdb822684a1` (`guids.h:154`) | `TBusinessProceses` | 6 | 7 | 4 | 5 | 3 | `{0,1,1,1,2}` | `{0,1,5,1,2}` | `{0,1,3,2,9,2}` | true | `GetNameFormDescriptor` | `GetNameMoxCatalogs` | `BusinessProceses.cpp:10-24` |
| Задачи | `3e63355c-1378-4953-be9b-1deb5fb6bec5` (`guids.h:155`) | `TTasks` | 5 | 7 | 4 | 8 | 3 | `{0,1,1,1,2}` | `{0,1,5,1,2}` | `{0,1,3,2,9,2}` | true | `GetNameFormDescriptor` | `GetNameMoxCatalogs` | `Tasks.cpp:10-24` |
| Планы видов расчета | `30b100d6-b29f-47ac-aec7-cb8ca8a54767` (`guids.h:149`) | `TChartOfCalculationTypes` | 4 | 3 | 7 | 5 | 6 | `{0,1,1,1,2}` | `{0,1,5,1,2}` | `{0,1,3,2,9,2}` | true | `GetNameFormCatalogs` | `GetNameMoxCatalogs` | `ChartOfCalculationTypes.cpp:10-25` (в файле TODO, пути скопированы из Catalogs — `:13`) |
| Планы обмена | `857c4a91-e5f4-4fac-86ec-787626f1c108` (`guids.h:115`) | `TExchangePlans` | 3 | 5 | 6 | 7 | 4 | `{0,1,1,1,2}` | `{0,1,5,1,2}` | `{0,1,3,2,9,2}` | true | `GetNameFormCatalogs` | `GetNameMoxCatalogs` | `ExchangePlans.cpp:362-376` (+ фолбэки по GUID, п.2.6) |

> Индексы для **Справочников**, **Документов** и **Обработок** подтверждены на реальных
> объектах конфигурации (п.7). Индексы для остальных типов взяты из кода и на реальных
> данных не проверялись.

Соответствие «индекс → номер таблицы во внешнем списке» (наглядно; номер = индекс, т.к.
таблицы начинаются с `root[0][3]`):

| Тип | [3] | [4] | [5] | [6] | [7] | [8] | [9] |
|---|---|---|---|---|---|---|---|
| Справочники | макеты | команды | ТЧ | реквизиты | формы | — | — |
| Документы | ТЧ | макеты | реквизиты | команды | формы | — | — |
| Журналы | макеты | реквизиты (графы) | команды | формы | — | — | — |
| Отчеты | макеты | реквизиты | формы | ТЧ (по коду) | команды | — | — |
| Обработки | ТЧ | макеты | команды | формы | реквизиты | — | — |
| Бизнес-процессы | макеты | формы | команды | реквизиты | ТЧ | — | — |
| Задачи | макеты | формы | реквизиты | (не читается) | ТЧ | команды | — |
| Планы видов расчета | ТЧ | реквизиты | команды | макеты | формы | — | — |
| Планы обмена | реквизиты | макеты | ТЧ | формы | команды | — | — |

### 1.2 Регистры на `MetadataObjectInformationRegister`
Структура путей: `struct InfoRegisterTreePaths` — `MetadataObjectInformationRegister.h:13-26`
(дополнительно `dimIdx`/`dimItemPath`, `resIdx`/`resItemPath`; **флага `hasTabulars` нет** —
ТЧ у регистров не читаются, вектор `tabulars` всегда пуст, `MetadataObjectInformationRegister.h:40`).
Алгоритм: `initializeFromTreeWithPaths()` — `MetadataObjectInformationRegister.cpp:52-159`
(реквизиты 57-72, измерения 75-90, ресурсы 93-108, формы 111-125, команды 128-142, макеты 145-158).

| Тип (рус.) | GUID категории | Класс | attIdx | dimIdx | resIdx | formsIdx | cmdIdx | moxIdx | att/dim/res ItemPath | cmdItemPath | Имя формы | Источник |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Регистры сведений | `13134201-f60b-11d5-a3c7-0050bae0a776` (`guids.h:150`) | `TInformationRegisters` | 7 | 4 | 3 | 5 | 8 | 6 | `{0,1,1,1,2}` | `{0,1,2,9,2}` | `GetNameFormCatalogs` | `InformationRegisters.cpp:10-26` |
| Регистры накопления | `b64d9a40-1642-11d6-a3c7-0050bae0a776` (`guids.h:151`) | `TAccumulationRegisters` | 6 | 7 | 5 | 8 | 4 | 3 | `{0,1,1,1,2}` | `{0,1,2,9,2}` | `GetNameFormCatalogs` | `AccumulationRegisters.cpp:10-32` |
| Регистры бухгалтерии | `2deed9b8-0056-4ffe-a473-c20a6c32a0bc` (`guids.h:152`) | `TAccountingRegisters` | 7 | 3 | 5 | 8 | 6 | 4 | `{0,1,1,1,2}` | `{0,1,2,9,2}` | **`GetNameFormPVH`** | `AccountingRegisters.cpp:10-33`, `:31` |
| Регистры расчета | `f2de87a8-64e5-45eb-a22d-b3aedab050e7` (`guids.h:153`) | `TCalculationRegisters` | 3 | 9 | 6 | 7 | 8 | 5 | `{0,1,1,1,2}` | `{0,1,2,9,2}` | `GetNameFormCatalogs` | `CalculationRegisters.cpp:12-34` |

| Тип | [3] | [4] | [5] | [6] | [7] | [8] | [9] |
|---|---|---|---|---|---|---|---|
| Регистры сведений | ресурсы | измерения | формы | макеты | реквизиты | команды | — |
| Регистры накопления | макеты | команды | ресурсы | реквизиты | измерения | формы | — |
| Регистры бухгалтерии | измерения | макеты | ресурсы | команды | реквизиты | формы | — |
| Регистры расчета | реквизиты | — | макеты | ресурсы | формы | команды | измерения |

### 1.3 Особые реализации (собственный `initializeFromTree`, без `MetadataTreePaths`)

| Тип (рус.) | GUID категории | Класс / база | Индексы таблиц | Пути имён | Имя формы | Имя макета | Источник |
|---|---|---|---|---|---|---|---|
| Планы счетов | `238e7e88-3c5f-48b2-8a3b-81ebbecb20ed` (`guids.h:148`) | `TChartOfAccounts : BaseMetadataObject` (`ChartOfAccounts.h:19`) | реквизиты 7, признаки учета 8, признаки учета субконто 9, ТЧ 5, формы 6, команды 3, макеты 4 | реквизиты/признаки `[0][1][1][1][2]`; ТЧ `[0][1][5][1][2]`; команды `[0][1][3][2][9][2]` | `GetNameFormPVH` | `GetNameMoxCatalogs` | `ChartOfAccounts.cpp:57-189` (`:65,72,82,88,96,102,111,117,121,142,152,161,169,176,185`) |
| Планы видов характеристик | `82a1b659-b220-4d94-a9bd-14d757b95a48` (`guids.h:147`) | `TChartOfCharacteristicTypes : BaseMetadataObject` (`ChartOfCharacteristicTypes.h`) | реквизиты 3, макеты 4, ТЧ 5, команды 6, формы 7 | реквизиты `[0][1][1][1][2]`; ТЧ `[0][1][5][1][2]`; команды `[0][1][3][2][9][2]` | `GetNameFormPVH` | `GetNameMoxCatalogs` | `ChartOfCharacteristicTypes.cpp:53-163` (`:63,70,81,87,91,113,123,133,141,149,158`) |
| Перечисления | `f6a80749-5ad7-400b-8519-39dc5dff2542` (`guids.h:144`) | `TEnums : TObject` (`Enums.h:12`) | значения 6, формы 3, макеты 4, команды 5 | значения `[0][1][2]`; команды `[0][1][3][2][9][2]` | `GetNameFormCatalogs` | `GetNameMoxCatalogs` | `Enums.cpp:25-102` (`:36,43,54,63,72,79,87,96`) |
| Критерии отбора | `3e7bfcc0-067d-11d6-a3c7-0050bae0a776` (`guids.h:116`) | `TFilterCriteria : BaseMetadataObject` | формы — по GUID `00867c40-06b1-11d6-a3c7-0050bae0a776`, фолбэк `root[0][3]`; команды — по GUID `23fa3b84-220a-40e9-8331-e588bed87f7d`, фолбэк `root[0][4]` | имя формы — прямо из файла формы (`FindMetadataDescriptor`), имя команды — `descriptor[2]` | см. `FilterCriteria.cpp:12,85-97` | — | `FilterCriteria.cpp:159-212` |
| Внешние источники данных | `5274d9fc-9c3a-4a71-8f5e-a0db8ab23de5` (`guids.h:156`) | `TExternalDataSources : BaseMetadataObject` | кубы 3, функции 4, таблицы 5; внутри таблицы/куба: формы 3, макеты 4, команды 5, поля 6 (куб: измерения 7, ресурсы 8) | см. п.2.8 | имена читаются из файлов по путям `{1,1,2}` (формы), `{1,2,2}` (макеты) | — | `ExternalDataSources.cpp:126-211, 297-337` |
| Последовательности | `bc587f20-35d9-11d6-a3c7-0050bae0a776` (`guids.h:142`) | `TSequences : TObject` | состав **не читается** (только дерево в `root_data`) | — | — | — | `Sequences.cpp:18-32` |
| Нумераторы | `36a8e346-9aaa-4af9-bdbd-83be3c177977` (`guids.h:141`) | `TNumerators : TObject` | состав **не читается** | — | — | — | `Numerators.cpp:18-31` |

### 1.4 Типы, которые состав НЕ читают (только имя; часть — только модуль)

Имя объекта во всех этих классах приходит из `fill_md` (п.2.0) и просто присваивается в
конструкторе; `initializeFromTree()` пуст:

`TConstants` (`TConstants.cpp:67-71`), `TLangs` (`Langs.cpp:67-71`), `TRoles`
(`Roles.cpp:67-71`), `TSubsystem` (`Subsystem.cpp:66-70`), `TCommonPictures`
(`CommonPictures.cpp:67-71`), `TCommonTemplates` (`CommonTemplates.cpp:67-71`),
`TCommonCommands` (`CommonCommands.cpp:67-71`), `TCommonAttributes`
(`CommonAttributes.cpp:67-71`), `TCommandGroups` (`CommandGroups.cpp:67-71`),
`TSessionParameters` (`SessionParameters.cpp:67-71`), `TFunctionalOptions`
(`FunctionalOptions.cpp:67-71`), `TFunctionalOptionsParameters`
(`FunctionalOptionsParameters.cpp:67-71`), `TDefinedTypes` (`DefinedTypes.cpp:67-71`),
`TEventSubscriptions` (`EventSubscriptions.cpp:67-71`), `TScheduledJobs`
(`ScheduledJobs.cpp:67-71`), `TSettingsStorages` (`SettingsStorages.cpp:67-71`),
`TInterfaces` (`Interfaces.cpp:67-71`), `THTTPServices` (`HTTPServices.cpp:71-75`),
`TWebServices` (`WebServices.cpp:71-75`), `TXDTOPackages` (`XDTOPackages.cpp:66-70`),
`TCommonModules` (`CommonModules.cpp:460-463` — но см. п.3),
`TCommonForms` (`CommonForms.cpp:194-198` — но см. п.3).

Исключения из «пустых»: `TStyles` (`Styles.cpp:89-101`, имя `{0,1,1,2}`),
`TStyleItems` (`StyleItems.cpp:89-101`, имя `{0,1,3,2}`), `TBots` (`Bots.cpp:89-101`,
`{0,1,1,2}`), `TWebSocketClients` (`WebSocketClients.cpp:89-101`, `{0,1,1,2}`),
`TIntegrationServices` (`IntegrationServices.cpp:89-101`, `{0,1,1,2}`),
`TWSReferences` (`WSReferences.cpp:119-139`: `{1,2,2}`, затем структурный поиск),
`TFilterCriteria` (`{0,1,5,1,2}`, `FilterCriteria.cpp:169-172`),
`TExternalDataSources` (`ExternalDataSources.cpp:300-306`).

---

## 2. Что лежит в таблице и как получается имя элемента (точные пути)

### 2.0 Имя самого объекта (используется в `fill_md`, а не в классах объектов)
`fill_md(tr, guid_md)` — `MainUnit.cpp:1937-2483`; карта путей `namePaths` — `MainUnit.cpp:1948-1998`.
Алгоритм: `find_metadata_node_by_guid(tr, guid_md)` (`:2029`) → узел таблицы категории;
`K = node_md->get_next()->get_value().ToInt()` (`:2058-2063`) → итерация по
`curNode->get_next()` (первый sibling пропускается — это узел количества, `:2087-2092`) →
для каждого GUID файла объекта: `cf->GetFile(guid)`, `get_treeFromV8file` (`:2100-2116`),
`node = tryGetNodeByPath(tree_md, path)` (`:2148-2169`), `val = node->get_value()` (`:2192`).

Пути к имени объекта (`MainUnit.cpp:1948-1998`):

| Категория | Путь к имени объекта |
|---|---|
| Справочники, Документы | `{0,1,9,1,2}` |
| Языки, Общие модули, Роли, Общие макеты, Регламентные задания, Параметры функциональных опций, Подсистемы, Стили, Подписки на события, Общие картинки, Функциональные опции, XDTO-пакеты, Нумераторы, Боты, WebSocket-клиенты, Сервисы интеграции | `{0,1,1,2}` |
| HTTP-сервисы, Интерфейсы, Web-сервисы | `{0,1,2,2}` |
| Общие реквизиты, Константы | `{0,1,1,1,1,2}` |
| Параметры сеанса, Хранилища настроек, Общие формы | `{0,1,1,1,2}` |
| Критерии отбора | `{0,1,5,1,2}` |
| Элементы стиля, Определяемые типы | `{0,1,3,2}` |
| Планы обмена | `{0,1,12,2}` |
| Группы команд | `{0,1,6,2}` |
| Общие команды | `{0,1,1,2,9,2}` |
| Журналы документов, Отчеты, Обработки | `{0,1,3,1,2}` |
| Планы видов характеристик, Регистры накопления | `{0,1,13,1,2}` |
| Регистры сведений, Регистры расчета, Планы счетов | `{0,1,15,1,2}` |
| Регистры бухгалтерии | `{0,1,16,1,2}` |
| Планы видов расчета | `{0,1,1,1,2}` |
| Последовательности | `{0,1,7,1,2}` |
| Перечисления | `{0,1,5,1,2}` |
| Внешние источники данных | `{1,1,1,2}` (и фолбэки, `:2158-2162`) |
| WS-ссылки | `{1,2,2}` (`:1969`) |
| Бизнес-процессы, Задачи | `{0,1,1,2}` + фолбэки `{0,1,1}` и `{0,1,2}` (`:2153-2157`) |

> Эмпирически проверено (п.7): для документа и справочника путь `{0,1,9,1,2}` даёт их имена
> (`АвансовыйОтчет`, `АвансовыйОтчетПрисоединенныеФайлы`), для обработки работает только
> `{0,1,3,1,2}` (`WildberriesПереходРВБ`).

### 2.1 Таблицы, элементы которых — GUID-строки: ФОРМЫ и МАКЕТЫ
Элемент таблицы — `nd_string`/`nd_guid` со значением GUID (не вложенный список).
Проверено на реальных данных (п.7): элементы таблиц форм и макетов документа — строки GUID.

Итерация (`MetadataObjectWithSections.cpp:157-174` для форм и `:200-216` для макетов;
`MetadataObjectInformationRegister.cpp:111-125` и `:145-158` — идентично):
```
node        = root[0][formsIdx][0]        // узел GUID таблицы форм
cur         = node->get_next()            // узел количества (пропускается)
while (cur) { cur = cur->get_next();      // первый элемент — индекс 2
              if (cur) ... = cur->get_value(); }
```
То есть форма `j` = `root[0][formsIdx][2 + j]`, макет `j` = `root[0][moxIdx][2 + j]`.
Количество (`root[0][idx][1]`) в этих циклах **не используется** (в регистрах читается
в неиспользуемую переменную `CountChild` — `MetadataObjectInformationRegister.cpp:114`).
Имя: `getFormNameFunc(parent, guid)` (форма) — `MetadataObjectWithSections.cpp:167`;
`GetNameMoxCatalogs(parent, guid)` (макет) — `MetadataObjectWithSections.cpp:209`.
В `TExchangePlans` при фолбэке по GUID имя формы сбрасывается (`TForm1C(formName, L"")`,
`ExchangePlans.cpp:310`), а у макета GUID сохраняется (`ExchangePlans.cpp:349`).

### 2.2 Таблицы с вложенными списками: РЕКВИЗИТЫ, ТЧ, КОМАНДЫ
Элемент таблицы — вложенный список. Фактические формы (сняты с реальных данных, п.7):

```
элемент реквизита   = { { '6', <список свойств N>, '0', '1', '1', '0', … }, … }
                      список свойств[1] = { '2', <описатель>, <Pattern…> }
элемент ТЧ          = { { '2', <список свойств>, '5' }, '1', { 888744e1-…, K, <реквизиты ТЧ…> } }
                      список свойств[5] = { '0', <описатель> }
описатель (descriptor) = { '3', { 1, 0, <GUID> }, '<Имя>', { 1, 'ru', '<Синоним>' }, … }
                         имя = описатель[2],  GUID = описатель[1][2]
```
Т.е. путь к имени — это «спуск к описателю через фиксированный номер свойства
(`[1]` у реквизита, `[5]` у ТЧ), затем `[1]` — описатель, `[2]` — имя».

* **Реквизиты**: `item = root[0][attIdx][i+2]`; имя = `item[attItemPath]`,
  т.е. `root[0][attIdx][i+2][0][1][1][1][2]` (`MetadataObjectWithSections.cpp:105-119`).
  Для всех типов `attItemPath = {0,1,1,1,2}`, кроме журналов (`{0,1,2}` — `Journals.cpp:14`).
  Эмпирика (документ, 28 реквизитов): `ВидОперации, ВалютаДокумента, Комментарий,
  КурсДокумента, КратностьДокумента, Организация, …` — путь работает.
  У реквизита создаётся `TRequisite(NameAtt, "")` — GUID реквизита не заполняется.
* **Табличные части**: `tabularNode = root[0][tabIdx][i+2]`;
  имя = `tabularNode[tabItemPath]` = `root[0][tabIdx][i+2][0][1][5][1][2]`
  (`MetadataObjectWithSections.cpp:125-134`, `:16`);
  GUID ТЧ = `tabularNode[0][1][5][1][1]` с `Trim` (`MetadataObjectWithSections.cpp:136-138`,
  также `ChartOfAccounts.cpp:117-124`, `ChartOfCharacteristicTypes.cpp:87-94`).
  Далее, если `parent->GetFile(GuidAttTab)` существует, дерево ТЧ берётся из отдельного
  файла контейнера, иначе используется сам `tabularNode`
  (`MetadataObjectWithSections.cpp:142-151`; `Tabular.cpp:330-387`).
  Эмпирика: имя ТЧ `ВыданныеАвансы` по пути `{0,1,5,1,2}` работает; **но** в формате 8.3.16
  узел `[0][1][5][1][1]` — это список `{1,0,<GUID ТЧ>}`, а не строка, поэтому
  `GuidAttTab` получается пустым и ТЧ разбирается из вложенной таблицы элемента
  (см. п.7 и п.8).
  Внутри элемента ТЧ лежит вложенная таблица реквизитов ТЧ с GUID
  `888744e1-b616-11d4-9436-004095e12fc7` (эмпирика; та же константа —
  `Tabular.cpp:234`).
* **Команды**: `commandNode = root[0][cmdIdx][i+2]`; имя = `commandNode[cmdItemPath]`;
  GUID команды = `FindFirstGuid(commandNode)` — первый GUID-подобный литерал в DFS-порядке
  (`MetadataObjectWithSections.cpp:178-196`, `:20-37`).
  Варианты `cmdItemPath`: `{0,1,3,2,9,2}` (Справочники, Документы, Бизнес-процессы, Задачи,
  Планы видов расчета, Планы обмена, Планы счетов, ПВХ, Перечисления) и
  `{0,1,2,9,2}` (Журналы, Отчеты, Обработки, все регистры).
  *Путь на реальных данных проверить не удалось: во всех доступных объектах таблица
  команд пуста (п.7).*

### 2.2.1 Тип реквизита: список «Pattern» и внутренние идентификаторы типов
Тип значения лежит в списке `Pattern` рядом с описателем (эмпирика, БП 3.0):

```
{2, {3, {1,0,<GUID>}, "<Имя>", …}, {"Pattern", <тип>, …}}
```

Элементы списка типов: `{"S",<длина>,<перем.>}` — строка, `{"N",<точность>,
<масштаб>,<флаг>}` — число, `{"B"}` — булево, `{"D",…}` — дата, `{"#",<GUID>}` —
ссылочный тип; составной тип — несколько элементов подряд
(`{"Pattern",{"#",G1},{"S",20,1}}`).

GUID ссылочного типа — это не GUID объекта, а его «внутренний идентификатор типа».
Они перечислены в главном списке объекта перед описателем:

```
{<вид>, <typeId…>, {0, <описатель>}, …}
```

У определяемых типов и перечислений описатель стоит прямо в этом списке
(`{0, <typeId…>, {2, <описатель>}, …}`), поэтому идентификаторы собираются по
всем предкам описателя. Проверено на БП: `СправочникСсылка.Валюты`
(`655fd168-…`), `ПеречислениеСсылка.СпособыУстановкиКурсаВалюты`
(`a01d58fd-…`), `ОпределяемыйТип.КурсВалюты` (`d74ccadb-…`), составной
`ДокументСсылка.…, ДокументСсылка.…`.

Тот же список `Pattern` задаёт и собственный тип значения объектов «Общие
реквизиты», «Константы», «Параметры сеанса»: у них он стоит рядом с описателем
объекта (`{27, {2, <описатель>, {"Pattern", …}}}`), поэтому тип читается тем же
кодом, что и у реквизитов (эмпирика: «Общий реквизит.КомментарийЯзык1» —
`{"S"}`, «Константа.АвтоматическиВосстанавливать…» — `{"B"}`).

### 2.3 Регистры: ИЗМЕРЕНИЯ и РЕСУРСЫ
`MetadataObjectInformationRegister.cpp:75-108`: элементы ищутся по
`dimIdx`/`resIdx` и путям `dimItemPath`/`resItemPath` (для всех 4 типов —
`{0,1,1,1,2}`), результат кладётся в `TRequisite`, к полям
`getDimensions()` / `getResources()` (`MetadataObjectInformationRegister.h:56-57`).
Для регистров бухгалтерии `AccountingFlags`/`DimensionAccountingFlags` **не читаются**
(явный TODO: `AccountingRegisters.cpp:60-65`).

### 2.4 Планы счетов: ПРИЗНАКИ УЧЕТА и ПРИЗНАКИ УЧЕТА СУБКОНТО
`ChartOfAccounts.cpp:79-105`: признаки учёта — таблица 8, признаки учёта субконто — таблица 9;
путь к имени тот же, что у реквизитов: `[0][1][1][1][2]`;
результаты — `TAccountingFlag` и `TDimensionAccountingFlag`.

### 2.5 Перечисления
`TEnums` — единственный тип, который читает свой состав **прямо в конструкторе**
(не через `initializeFromTree()`), и только в 3-аргументном конструкторе
(`Enums.cpp:25-102`; 2-аргументный конструктор, `:18-23`, состав не читает).
(Похожая схема — у `TExternalDataSources`, `ExternalDataSources.cpp:239-241`, `:263-265`,
вызывающего `initializeFromTree()` из конструктора.)
Значения: количество `root[0][6][1]`, элемент `root[0][6][i+2]`,
имя = `[0][1][2]` (фактически `root[0][6][i+2][0][1][2]`) — `Enums.cpp:36-49`.
Формы — таблица 3 (`:53-66`), команды — таблица 5 по пути `[0][1][3][2][9][2]`
(`:70-82`), макеты — таблица 4 (`:86-100`). Годятся только имена (векторы `String`,
`Enums.h:22-26`).

### 2.6 Планы обмена: индексные пути + фолбэки по GUID таблиц
`TExchangePlans::initializeFromTree()` — `ExchangePlans.cpp:412-430`: сначала общий
алгоритм `MetadataObjectWithSections` с индексами (`:362-376`), затем, если
соответствующий список пуст — попытки по **явным GUID таблиц**:
реквизиты `1a1b4fea-e093-470d-94ff-1d2f16cda2ab` (`:88`, `:86-196`),
ТЧ `52293f4b-f98c-43ea-a80f-41047ae7ab58` (`:200`, `:198-241`),
команды `d5207c64-11d5-4d46-bba2-55b7b07ff4eb` (`:246`, `:244-281`),
формы `87c509ab-3d38-4d67-b379-aca796298578` (`:285`, `:283-321`),
макеты `3daea016-69b7-4ed4-9453-127911372fe6` (`:325`, `:323-360`).
Техника: `find_metadata_node_by_guid(root, GUID)` → `countNode = node->get_next()` →
итерация по `get_next()` (элементы — siblings), имя — либо по пути
`{0,1,1,1,2}` (реквизиты), `{0,1,5,1,2}` (ТЧ), `{0,1,3,2,9,2}` (команды), либо
`FindFirstMeaningfulString` (`:67-84`, отбрасывает GUID/числа/`#…`).

### 2.7 Критерии отбора: разделы по GUID, имя формы — из её файла
`FilterCriteria.cpp:159-212`:
* имя объекта — `{0,1,5,1,2}` (`:169-172`);
* формы: раздел ищется `FindChildListByFirstValue(rootNode, "00867c40-06b1-11d6-a3c7-0050bae0a776")`
  (`:12`, `:176-178`), фолбэк — `root[0][3]`; элементы — `section[i+2]` (GUID формы, `:180-194`);
  имя формы читается **из файла формы**: `GetMetadataNameFromFile` → `get_treeFromV8file` →
  `FindMetadataDescriptor` (узел-описатель, где `[0]=='3'`, `[1][2]` — GUID, `[2]` — имя),
  `GetDescriptorName() = descriptor[2]` (`:49-97`);
* команды: раздел по GUID `23fa3b84-220a-40e9-8331-e588bed87f7d`
  (`guids.h:177`), фолбэк — `root[0][4]`; имя и GUID — из описателя (`:196-211`).

### 2.8 Внешние источники данных
`ExternalDataSources.cpp:297-337`: корень нормализуется `GetPayloadRoot()` (если у узла
ровно один ребёнок — берётся он, `:46-53`); имя — по путям `{1,1,1,2}`/`{0,1,1,2}`/`{1,0,1,2}`
(`:300-306`, `:148-154`); кубы — раздел 3, функции — раздел 4, таблицы — раздел 5.
Внутри таблицы (`ReadTable`, `:126-161`): имя `{1,1,1,2}`…, формы — раздел 3 (имена из
файлов по пути `{1,1,2}`), макеты — раздел 4 (`{1,2,2}`), команды — раздел 5
(`{0,1,3,2,9,2}`), поля — раздел 6 (`{0,1,1,1,2}`). Внутри куба (`ReadCube`, `:163-211`):
формы/макеты/команды — те же, измерения — раздел 7, ресурсы — раздел 8, таблицы измерений —
раздел 6.

### 2.9 Что НЕ читается из текста объекта
* `root[0][2]` (число таблиц) — при чтении состава не проверяется (единственное чтение —
  `Class_1CD.cpp:8279` для объекта «Конфигурация»);
* GUID самих таблиц (`root[0][T][0]`) — не сверяется с ожидаемым (кроме ExchangePlans,
  FilterCriteria и `Tabular.cpp`); порядок/индексы зашиты константами;
* `root[0][1]` (список свойств объекта) — кроме путей к имени объекта из п.2.0 и
  точечного `{0,1,5,1,2}` у критериев отбора;
* тексты модулей в самом файле объекта не ищутся, кроме эвристики
  `FindEmbeddedModuleText` (`ModuleTextStorage.cpp:307-323`).

### 2.10 Подписки на события
Эмпирика (БП 3.0): текст подписки — это список
`{1, {1, <описатель>, {"Pattern", {"#", <GUID типа>}}, "<Событие>", <GUID модуля>, "<Процедура>"}}`:

* **Источник** — `Pattern` (может быть несколько типов, тогда «составной»);
  GUID расшифровывается по внутренним идентификаторам типов (п. 2.2.1):
  `ДокументСсылка.АвансовыйПлатежИностранцаПоНДФЛ`;
* **Событие** — строка вида `Posting_ОбработкаПроведения`,
  `BeforeWrite_ПередЗаписью`: в конфигураторе показывается русская часть после `_`;
* **Обработчик** — GUID общего модуля + имя процедуры
  (`УчетЗарплаты.АвансовыйПлатежИностранцаПоНДФЛПриПроведении`).

---

## 3. Модули

### 3.1 Кто умеет читать модуль
Переопределяют `HasEditableModuleText/GetEditableModuleText/...` только:
* `MetadataObjectWithSections` — `MetadataObjectWithSections.cpp:219-316`
  (кандидаты видов: `Unknown | ObjectModule | ManagerModule` — `:13-18`),
  т.е. **Справочники, Документы, Журналы, Отчеты, Обработки, Бизнес-процессы, Задачи,
  Планы видов расчета, Планы обмена**;
* `TCommonModules` — `CommonModules.cpp:382-458` (общий модуль);
* `TCommonForms` — `CommonForms.cpp:96-167` (общий макет формы).

Все остальные наследуют `BaseMetadataObject`, где методы — заглушки
(`HasEditableModuleText() == false`, пустой текст, `BaseMetadataObject.cpp:48-96`).
**Следствия:**
* у регистров (`MetadataObjectInformationRegister`) модуль объекта/менеджера не читается,
  хотя в UI для них создаются узлы «Модуль объекта»/«Модуль менеджера» —
  `MetadataTreeBuilder.cpp:111-112` (для стандартных объектов — `:95-96`);
* у Планов счетов, ПВХ, Перечислений, Критериев отбора, Внешних источников данных
  модуль тоже не читается (наследуют заглушки).

### 3.2 Как читается модуль объекта/менеджера (MetadataObjectWithSections)
1. `RefreshModuleDocument(kind)` — `MetadataObjectWithSections.cpp:224-239`: при первом
   обращении вызывает `ModuleTextStorage::LoadByMetadataObject(parent, guid, name, effectiveKind)`
   (`:235`); `Unknown` трактуется как `ObjectModule` (`:229`).
2. `LoadByMetadataObject()` — `ModuleTextStorage.cpp:591-643`, порядок попыток:
   1. `SourceCF/<normalizedGuid>.<i>/…` на диске (`TryLoadFromSourceCfByGuid`, `:183-260`);
   2. поиск в контейнере по кандидатам (`GetModuleContainerCandidates`, `:110-136`),
      и файлы по GUID-ссылкам из дерева объекта (`CollectGuidReferences`, `:612-620`);
   3. для каждого кандидата — `TryReadModuleContainer()` (`:360-413`), который
      трактует файл как **вложенный контейнер** и берёт из него файл `text`, иначе `module`,
      иначе любой дочерний, чьё имя содержит `text`/`module`; если это не контейнер —
      читает файл как текст; иначе ищет «встроенный» текст модуля в дереве (`FindEmbeddedModuleText`).
3. Кандидаты имён (`GetModuleContainerCandidates`, `ModuleTextStorage.cpp:110-136`):
   * `ModuleTextKind::ManagerModule`: **`<guid>.3`, `<guid>.1`, `<guid>.2`, `<guid>.4`,
     `<guid>.5`, `<guid>.0`**, затем `<guid>`;
   * иначе (объектный модуль): **`<guid>.0`, `<guid>.1`, `<guid>.2`, `<guid>.3`,
     `<guid>.4`, `<guid>.5`**, затем `<guid>`.
   (Ср. устаревший дубль в `CommonModules.cpp:110-117`: только `.0…3` + `<guid>`.)
4. Дисковая раскладка SourceCF (`TryLoadFromSourceCfByGuid`, `:194-250`):
   `SourceCF/<guid>.<i>/text`, `SourceCF/<guid>.<i>/module`, `SourceCF/<guid>.<i>` (файл).
   Корни SourceCF — `GetSourceCfRoots()` (`:89-95`): текущий каталог и каталог exe.
5. Кодировка определяется автоматически (`DecodeModuleText`, `:34-74`):
   BOM (UTF‑8/UTF‑16LE) → эвристика UTF‑16LE → UTF‑8 → ANSI.

### 3.3 Модули общих модулей и общих форм
* Общий модуль: `TCommonModules::LoadTextIfNeeded` → `ModuleTextStorage::LoadCommonModule`
  (`CommonModules.cpp:382-389`; `ModuleTextStorage.cpp:507-563`) — те же кандидаты
  `<guid>.0…5`/`<guid>`, плюс поиск по имени модуля в текстах метаданных
  (`TryLoadFromSourceCfByModuleName`, `:262-293`).
  Локальные дубли-помощники в `CommonModules.cpp:110-353`
  (`AddModuleFileCandidates`, `TryReadUnpackedModuleText`, …) **не используются**
  (мёртвый код).
* Общая форма: `TCommonForms::LoadTextIfNeeded` → `ModuleTextStorage::LoadCommonForm`
  (`CommonForms.cpp:96-103`; `ModuleTextStorage.cpp:565-589`).
  Отличие: встроенный вариант берётся **только** как `parent->GetFile(guid + ".0")`
  и читается как контейнер (`ModuleTextStorage.cpp:577-586`).
  В `CommonForms.cpp:38-52` есть `GetManagedFormModuleText()` (модуль формы —
  `root[0][2]`), но она **нигде не вызывается**.

### 3.4 Модули форм и команд (вложенных элементов)
Дерево строит узлы «Модуль формы»/«Модуль команды» с GUID элемента
(`MetadataTreeBuilder.cpp:63-74`, вызовы `:101-102, :118-119, :200-201, :221-222, :240-241`).
Загрузка — при выборе узла: `MainUnit.cpp:3247-3258` —
`ModuleTextStorage::LoadByMetadataObject(metadataObject->parent, Data->moduleItemGuid,
Data->Name, requestedKind)`, где `requestedKind` — `FormModule` или `CommandModule`;
кандидаты те же `<guidФормы>.0…5` / `<guidФормы>` (`ModuleTextStorage.cpp:110-136`).
Виды модулей — `enum class ModuleTextKind` — `ModuleTextStorage.h:12-21`.
Дополнительный фолбэк по имени узла — `FindUnpackedModuleTextByNodeName`,
`MainUnit.cpp:399-431`, вызов `:3291`.

Эмпирика (БП 3.0): у объектов «Общие команды» модуль команды лежит в элементах
`<GUID команды>.0…5` (например, `.2` у «АвтономнаяРабота») и в дереве должен
подписываться «Модуль команды», а не «Модуль объекта» — объектовый/менеджерский
модуль у команды не читаем.

### 3.5 Корневые модули конфигурации
Корневые модули лежат не в элементе конфигурации, а во вложенных контейнерах
с именами `<GUID описателя конфигурации>.<номер>` (у БП 3.0 описатель —
`f0ba0954-a66b-4085-9df1-b8a4283bdbd3`); внутри контейнера файлы `info`
(`{3,1,0,"",0}`) и `text`:

| Номер | Содержимое |
|---|---|
| `.6` | Модуль приложения |
| `.7` | Модуль сеанса (в тексте — `УстановкаПараметровСеанса`) |
| `.5` | Модуль внешнего соединения (у БП пуст: только комментарий лицензии) |
| `.0` | Модуль обычного приложения (клиентские обработчики) |

Номер элемента — это индекс свойства корневого объекта; порядок проверен
на БП 3.0. Модуль сеанса дополнительно узнаётся по тексту
(`УстановкаПараметровСеанса`).

---

## 4. Подсистемы

Состав подсистем читается **не** через `TSubsystem` (там `initializeFromTree` пуст,
`Subsystem.cpp:66-70`), а отдельными функциями в `MetadataTreeBuilder.cpp`:

1. `GetNameSubsystem(cf, guid)` — `MetadataTreeBuilder.cpp:344-358`:
   `get_treeFromV8file(cf->GetFile(guid))`, затем `root[0][1][1][2]` → имя подсистемы.
2. `GetSubsystemInnerGuid(cf, guid)` — `MetadataTreeBuilder.cpp:360-374`:
   тот же файл, путь `root[0][1][1][1]` → **внутренний GUID подсистемы**
   (в конфигурации файл подсистемы назван родительским GUID‑ом, а её собственный GUID
   лежит внутри).
3. `GetListChildrenSubsystem(cf, guid, child)` — `MetadataTreeBuilder.cpp:376-394`:
   ```
   node = root[0][3][0];                 // узел GUID таблицы «дочерние подсистемы»
   cur  = node->get_next();              // узел количества (пропускается)
   while (cur) { cur = cur->get_next();  // элементы — с индекса 2
                 if (cur) child.push_back(cur->get_value()); }   // GUID дочерних подсистем
   ```
   т.е. список дочерних = `root[0][3][2..]` (структура `{GUID таблицы, K, GUID₁…GUID_K}`).
4. Обход:
   * `fill_subsystem()` — `MainUnit.cpp:1791-1826`: `find_metadata_node_by_guid(tr, GUID_Subsystems)`
     → количество `node_md->get_next()` → для каждого файла `GetNameSubsystem` (`:1817`) и
     `GetListChildrenSubsystem` (`:1821`), результат — `SubSys{имя, children}`.
   * `addSubsystemChildrenToTreeByGuid()` — `MetadataTreeBuilder.cpp:432-467`:
     берёт детей по `GetListChildrenSubsystem`; **если пусто — повторяет по
     `GetSubsystemInnerGuid`** (`:441-447`); для каждого ребёнка находит объект
     `findSubsystemByAnyGuid` (сравнение и по файловому GUID, и по внутреннему —
     `:396-416`) и рекурсивно продолжает по `childSubsystem->guid` (`:464-465`).
   * `collectChildSubsystemGuids()` — `MetadataTreeBuilder.cpp:476-519`: собирает GUID всех
     детей (и файловых, и внутренних) — используется, чтобы не показывать дочерние
     подсистемы на верхнем уровне (`MainUnit.cpp:1178, 1196-1203`).
   * `getSubsystemDisplayNameByGuid()` — `MetadataTreeBuilder.cpp:418-430`.
   * При открытии подсистемы в дереве — `addSubsystemChildrenToTree` (`MainUnit.cpp:1240-1244`).

---

## 5. Формы и макеты: пути к именам

### 5.1 Функции получения имени (`Common.cpp`)
Все функции используют `GetNameFromTreePath(cf, guid, path)` — `Common.cpp:445-469`:
`get_treeFromV8file(cf->GetFile(guid))`, затем спуск `node = &(*node)[path[i]]`
от **корня дерева** (т.е. `path[0]==0` адресует `root[0]` — внешний список),
возврат `node->get_value()`. При отсутствии файла/дерева — пустая строка.

| Функция | Путь | Строки | Для каких типов используется |
|---|---|---|---|
| `GetNameFormCatalogs` | `{0,1,1,1,2}` | `Common.cpp:471-475` | Справочники (`Catalogs.cpp:21`), Документы (`Documents.cpp:21`), Журналы (`Journals.cpp:21`), Планы видов расчета (`ChartOfCalculationTypes.cpp:22`), Планы обмена (`ExchangePlans.cpp:373`, `:307`), Регистры сведений (`InformationRegisters.cpp:24`), Регистры накопления (`AccumulationRegisters.cpp:30`), Регистры расчета (`CalculationRegisters.cpp:32`), Перечисления (`Enums.cpp:63`) |
| `GetNameFormReports` | `{0,1,1,1,1,2}` | `Common.cpp:477-500` | Отчеты (`Reports.cpp:21`), Обработки (`DataProcessors.cpp:21`) |
| `GetNameFormPVH` | `{0,1,1,2}` | `Common.cpp:502-525` | Планы счетов (`ChartOfAccounts.cpp:152`), ПВХ (`ChartOfCharacteristicTypes.cpp:123`), Регистры бухгалтерии (`AccountingRegisters.cpp:31`) |
| `GetNameFormDescriptor` | `{0,1,1,2}` | `Common.cpp:527-531` | Бизнес-процессы (`BusinessProceses.cpp:21`), Задачи (`Tasks.cpp:21`) |
| `GetNameMoxCatalogs` | `{0,1,2,2}` | `Common.cpp:533-556` | **Все** типы, у которых есть макеты: `MetadataObjectWithSections.cpp:209`, `MetadataObjectInformationRegister.cpp:155`, `Enums.cpp:96`, `ChartOfAccounts.cpp:185`, `ChartOfCharacteristicTypes.cpp:158`, `ExchangePlans.cpp:347` |

> `GetNameFormPVH` и `GetNameFormDescriptor` имеют **одинаковый путь** `{0,1,1,2}` —
> различие только семантическое (разные виды объектов).
> `GetNameFormReports` — единственная с более глубоким путём `{0,1,1,1,1,2}`.

Проверено эмпирически: `GetNameMoxCatalogs` (`{0,1,2,2}`) на реальных файлах макетов
«Бухгалтерии предприятия 3.0» даёт `ПФ_MXL_АвансовыйОтчет` и `ПФ_MXL_РеестрБилетов`
(см. п.7).

### 5.2 Как имя попадает в объект
* формы: `TForm1C(NameForm, guid)` — `MetadataObjectWithSections.cpp:168`,
  `MetadataObjectInformationRegister.cpp:123`, `ChartOfAccounts.cpp:153`,
  `ChartOfCharacteristicTypes.cpp:124`, `Enums.cpp` (только имя, вектор `String`),
  `FilterCriteria.cpp:193` (имя из файла формы), `ExchangePlans.cpp:310` (фолбэк);
* макеты: `TMoxel(NameMox, "")` — `MetadataObjectWithSections.cpp:210`,
  `MetadataObjectInformationRegister.cpp:156`, `ChartOfAccounts.cpp:186`,
  `ChartOfCharacteristicTypes.cpp:159`, `Enums.cpp:97`;
  в фолбэке планов обмена — `TMoxel(layoutName, guid_md)` (`ExchangePlans.cpp:349`).

---

## 6. GUID таблиц состава, встречающиеся в исходниках как константы

### 6.1 В `guids.h` (таблицы состава «справочного» вида)
| GUID | Имя константы | Назначение | Строка |
|---|---|---|---|
| `3daea016-69b7-4ed4-9453-127911372fe6` | `GUID_MoxelsCatalogs` | Макеты справочника (используется и как GUID таблицы макетов вообще; фолбэк планов обмена) | `guids.h:161`, `ExchangePlans.cpp:325` |
| `4fe87c89-9ad4-43f6-9fdb-9dc83b3879c6` | `GUID_CommandsCatalogs` | Команды справочника | `guids.h:163` |
| `932159f9-95b2-4e76-a8dd-8849fe5c5ded` | `GUID_TabsCatalogs` | Табличные части справочника | `guids.h:165` |
| `cf4abea7-37b2-11d4-940f-008048da11f9` | `GUID_AttCatalogs` | Реквизиты справочника (и реквизиты ТЧ — используется в `Tabular.cpp:203`) | `guids.h:167`, `Tabular.cpp:201-230` |
| `fdf816d2-1ead-11d5-b975-0050bae0a95d` | `GUID_FormsCatalogs` | Формы справочника | `guids.h:169` |

> Эмпирически подтверждено, что это именно те GUID'ы, которые стоят в позициях 3–7
> таблицы состава реального справочника (см. п.6.4, п.7) — т.е. имена констант в `guids.h`
> соответствуют действительности, а не только историческим комментариям.

### 6.2 Команды по видам объектов (`guids.h:176-203`)
| GUID | Константа | Объект |
|---|---|---|
| `23fa3b84-220a-40e9-8331-e588bed87f7d` | `GUID_SelCritCommands` | Команды критерия отбора (`guids.h:177`, `FilterCriteria.cpp:196`) |
| `f27c2152-a2c9-4c30-adb1-130f5eb2590f` | `GUID_TasksCommands` | Команды задач |
| `7a3e533c-f232-40d5-a932-6a311d2480bf` | `GUID_BPCommands`, `GUID_BsnPrcCommand` | Команды бизнес-процессов (`guids.h:183`, `:203`) |
| `45556acb-826a-4f73-898a-6025fc9536e1` | `GUID_ProcessingCommand` | Команда обработки |
| `b44ba719-945c-445c-8aab-1088fa4df16e` | `GUID_InfoRegCommand` | Команда регистра сведений |
| `7162da60-f7fe-4d78-ad5d-e31700f9af18` | `GUID_AccRegCommand` | Команда регистра бухгалтерии |
| `b544fc6a-2ba3-4885-8fb2-cb289fb6d65e` | `GUID_DocCommand` | Команда документа |
| `d5207c64-11d5-4d46-bba2-55b7b07ff4eb` | `GUID_ExchPlanCommand` | Команда плана обмена (`guids.h:195`, `ExchangePlans.cpp:246`) |
| `a49a35ce-120a-4c80-8eea-b0618479cd70` | `GUID_DocJrnlCommand` | Команда журнала документов |
| `0df30176-6865-4787-9fc8-609eb144174f` | `GUID_AccPlnCommand` | Команда плана счетов |
| `99f328af-a77f-4572-a2d8-80ed20c81890` | `GUID_RegAcmCommand` | Команда регистра накопления |

### 6.3 Прочие GUID таблиц/разделов, зашитые в код
| GUID | Где | Назначение |
|---|---|---|
| `1a1b4fea-e093-470d-94ff-1d2f16cda2ab` | `ExchangePlans.cpp:88` | Таблица реквизитов плана обмена (фолбэк) |
| `52293f4b-f98c-43ea-a80f-41047ae7ab58` | `ExchangePlans.cpp:200` | Таблица табличных частей плана обмена |
| `87c509ab-3d38-4d67-b379-aca796298578` | `ExchangePlans.cpp:285` | Таблица форм плана обмена |
| `00867c40-06b1-11d6-a3c7-0050bae0a776` | `FilterCriteria.cpp:12` | Таблица форм критерия отбора |
| `888744e1-b616-11d4-9436-004095e12fc7` | `Tabular.cpp:234` | Таблица реквизитов табличной части |
| `9cd510cd-abfc-11d4-9434-004095e12fc7` | `Class_1CD.cpp:8284` (комментарий `// узел "Общие"`) | Таблица «Общие» — свойства самой конфигурации; из неё читаются имя/поставщик/версия (`confinfo[1][1][2]` и т.д., `Class_1CD.cpp:8286-8318`). Не путать с `GUID_Languages = 9cd510ce-…` (`guids.h:137`) |

### 6.4 GUID таблиц, наблюдаемые в реальной конфигурации (не константы кода)
Объект «Документ.АвансовыйОтчет» («Бухгалтерия предприятия 3.0», формат 1.6):
| Индекс | GUID таблицы | Содержимое | K |
|---|---|---|---|
| 3 | `21c53e09-8950-4b5e-a6a0-1054f1bbc274` | табличные части | 7 |
| 4 | `3daea016-69b7-4ed4-9453-127911372fe6` | макеты | 2 |
| 5 | `45e46cbc-3e24-4165-8b7b-cc98a6f80211` | реквизиты | 28 |
| 6 | `b544fc6a-2ba3-4885-8fb2-cb289fb6d65e` | команды (`GUID_DocCommand`) | 0 |
| 7 | `fb880e93-47d7-4127-9357-a20e69c17545` | формы | 11 |
внутри элемента ТЧ: `888744e1-b616-11d4-9436-004095e12fc7` — реквизиты ТЧ.

Объект-справочник (файл `_analysis\spec\cf_catalog1.txt`), `root[0][2] = '5'`:
| Индекс | GUID таблицы | Содержимое | K |
|---|---|---|---|
| 3 | `3daea016-69b7-4ed4-9453-127911372fe6` | макеты (`GUID_MoxelsCatalogs`) | 0 |
| 4 | `4fe87c89-9ad4-43f6-9fdb-9dc83b3879c6` | команды (`GUID_CommandsCatalogs`) | 0 |
| 5 | `932159f9-95b2-4e76-a8dd-8849fe5c5ded` | табличные части (`GUID_TabsCatalogs`) | 2 |
| 6 | `cf4abea7-37b2-11d4-940f-008048da11f9` | реквизиты (`GUID_AttCatalogs`) | 20 |
| 7 | `fdf816d2-1ead-11d5-b975-0050bae0a95d` | формы (`GUID_FormsCatalogs`) | 0 |

Объект-обработка (файл `_analysis\spec\cf_dataproc1.txt`), `root[0][2] = '5'`:
| Индекс | GUID таблицы | Содержимое | K |
|---|---|---|---|
| 3 | `2bcef0d1-0981-11d6-b9b8-0050bae0a95d` | табличные части | 0 |
| 4 | `3daea016-69b7-4ed4-9453-127911372fe6` | макеты | 0 |
| 5 | `45556acb-826a-4f73-898a-6025fc9536e1` | команды (`GUID_ProcessingCommand`) | 0 |
| 6 | `d5b0e5ed-256d-401c-9c36-f630cafd8a62` | формы | 1 |
| 7 | `ec6bb5e5-b7a8-4d75-bec9-658107a699cf` | реквизиты | 6 |

Объект-критерий отбора («Критерий отбора.СвязанныеДокументы», БП 3.0), `root[0][2] = '2'`:
| Индекс | GUID таблицы | Содержимое | K |
|---|---|---|---|
| 3 | `00867c40-06b1-11d6-a3c7-0050bae0a776` | формы (`FilterCriteria.cpp:12`) | 0 |
| 4 | `23fa3b84-220a-40e9-8331-e588bed87f7d` | команды (`GUID_SelCritCommands`) | 0 |

Объект-хранилище настроек («Хранилище настроек.ХранилищеВариантовОтчетов», БП 3.0),
`root[0][2] = '2'`:
| Индекс | GUID таблицы | Содержимое | K |
|---|---|---|---|
| 3 | `3daea016-69b7-4ed4-9453-127911372fe6` | макеты | 0 |
| 4 | `b8533c0c-2342-4db3-91a2-c2b08cbf6b23` | формы | 13 |

> Наблюдение: GUID таблицы макетов `3daea016-…` встретился в трёх разных типах объектов,
> а GUID таблиц реквизитов/форм/ТЧ — разный для каждого типа. Т.е. `guids.h:160-169` —
> это GUID'ы именно **справочника**, а не универсальные.

---

## 7. Эмпирическая проверка (реальная конфигурация)

Данные: `C:\work\cpp\utils\1Cv8_bp.cf` («Бухгалтерия предприятия», 8.3.16+, формат
контейнера 1.6), распакованные тексты объектов:
`_analysis\extract_doc\Документы\АвансовыйОтчет\metadata.txt` (документ),
`_analysis\spec\cf_catalog1.txt` (справочник), `_analysis\spec\cf_dataproc1.txt` (обработка),
файлы макетов — `...\templates\*.meta.txt`.
Парсер `parse_1Ctext` воспроизведён на Python (см. `_analysis\analyze*.py`).

| Проверяемое утверждение | Результат |
|---|---|
| `root[0]` = `{1, свойства, N, таблицы…}`; таблицы с индекса 3 | подтверждено на 3 объектах: `[0]='1'`, `[1]` = список свойств (53 / 61 / 12), `[2]` = N, таблицы по индексам 3…7 |
| `N` = число таблиц | документ `N=5` (таблиц 3…7 — 5), справочник `N=5`, обработка `N=5` |
| Документы: `attIdx=5`, путь `{0,1,1,1,2}` | `ВидОперации, ВалютаДокумента, Комментарий, КурсДокумента, КратностьДокумента, Организация, …` ✔ |
| Документы: `tabIdx=3`, путь `{0,1,5,1,2}` | `ВыданныеАвансы, Товары, ВозвратнаяТара, ОплатаПоставщикам, Прочее, Билеты, Суточные` ✔ |
| Документы: `formsIdx=7`, элементы — GUID-строки | `root[0][7][2] = '1b1a1eef-a68c-4af4-b74b-1254e04b6e1e'` ✔ |
| Документы: `moxIdx=4`, элементы — GUID-строки | `root[0][4][2] = '82c8dbdb-465b-41ed-84b3-bf66dbfbd82e'` ✔ |
| Документы: `cmdIdx=6` | таблица команд присутствует (`b544fc6a-…`), но `K=0` |
| **Справочники**: все 5 индексов (`attIdx=6`, `tabIdx=5`, `formsIdx=7`, `cmdIdx=4`, `moxIdx=3`) | все 5 таблиц совпали по GUID с `guids.h:161-169` ✔; реквизиты (20): `Автор, ВладелецФайла, ДатаМодификацииУниверсальная, ДатаСоздания, Зашифрован, Изменил, ИндексКартинки, Описание, …`; ТЧ (2): `УдалитьЭлектронныеПодписи, УдалитьСертификатыШифрования` ✔ |
| **Обработки**: все 5 индексов (`attIdx=7`, `tabIdx=3`, `formsIdx=6`, `cmdIdx=5`, `moxIdx=4`) | совпали: [4] — макеты `3daea016-…`; [5] — команды `45556acb-…` (= `GUID_ProcessingCommand`); [6] — формы (1 шт.); [7] — реквизиты (6): `Организация, КонтрагентИсточник, КонтрагентПриемник, ДоговорКонтрагентаИсточник, ДоговорКонтрагентаПриемник, ДатаПереходаПрав` ✔ |
| Индексы таблиц зашиты константами; GUID таблиц при чтении состава не сверяются (кроме трёх мест) | подтверждено кодом (п.2.9, п.8.8) |
| `GetNameMoxCatalogs` = `{0,1,2,2}` | на файлах макетов: `ПФ_MXL_АвансовыйОтчет`, `ПФ_MXL_РеестрБилетов` ✔ (совпадает с `composition.txt`) |
| Имя самого объекта документа = `{0,1,9,1,2}` | `'АвансовыйОтчет'` ✔ |
| Имя объекта-справочника = `{0,1,9,1,2}` | `'АвансовыйОтчетПрисоединенныеФайлы'` ✔ |
| Имя объекта-обработки = `{0,1,3,1,2}` | `'WildberriesПереходРВБ'` ✔ (а `{0,1,1,2}` и `{0,1,9,1,2}` — не работают, что подтверждает индивидуальность путей в `namePaths`) |
| Форма элемента реквизита / ТЧ (п.2.2) | `{ {'6', <props>, '0','1','1','0',…}, … }` и `{ {'2', <props>, '5'}, '1', {888744e1-…, K, …} }` ✔ |
| GUID ТЧ по пути `{0,1,5,1,1}` | в формате 1.6 узел — **список** `{1,0,<GUID>}`, а не строка → в эталоне получается пустая строка (см. п.8) |
| Путь имени команды `{0,1,3,2,9,2}` / `{0,1,2,9,2}` | **не подтверждён** на доступных данных: во всех трёх объектах таблица команд пуста (`K=0`) |

---

## 8. Обнаруженные особенности и потенциальные ошибки эталона

1. **GUID табличной части** читается путём `{0,1,5,1,1}` (`MetadataObjectWithSections.cpp:136`,
   `ChartOfAccounts.cpp:121`, `ChartOfCharacteristicTypes.cpp:91`) и его значение
   `Trim(...)`. В формате 8.3.16 (1.6) по этому пути лежит список `{1,0,<GUID ТЧ>}`,
   поэтому `GuidAttTab` пуст и ТЧ всегда разбирается из вложенной таблицы элемента
   (fallback `MetadataObjectWithSections.cpp:150`). Отдельный файл ТЧ в этой конфигурации
   не открывается.
2. **Мёртвая проверка `hasTabulars`**: у журналов `tabIdx == moxIdx == 3`
   (`Journals.cpp:15,20`) — корректно только потому, что `hasTabulars=false`
   (`Journals.cpp:22`, проверка `MetadataObjectWithSections.cpp:123`).
3. **Отчёты**: `hasTabulars=true` и `tabIdx=6` (`Reports.cpp:15,22`), хотя табличных частей
   у отчётов, как правило, нет; путь скопирован без проверки.
4. **Планы видов расчета**: явный `TODO` — «пути скопированы из Catalogs»
   (`ChartOfCalculationTypes.cpp:13`); то же для регистров сведений
   (`InformationRegisters.cpp:13`) и регистра накопления («аналогично регистрам сведений»,
   `AccumulationRegisters.cpp:13`).
5. **Дублирование кода**: `FindFirstGuid` скопирован в 4 файлах
   (`MetadataObjectWithSections.cpp:20`, `ChartOfAccounts.cpp:13`,
   `ChartOfCharacteristicTypes.cpp:13`, `MetadataObjectInformationRegister.cpp:13`);
   `GetNodeByPath` — в 10+ файлах (напр. `MetadataObjectWithSections.cpp:39`,
   `FilterCriteria.cpp:14`, `Tabular.cpp:18`, `Styles.cpp:11`, `Bots.cpp:11`); локальные
   помощники модулей в `CommonModules.cpp:110-353` не используются.
6. **`GetManagedFormModuleText`** (`CommonForms.cpp:38-52`) не вызывается.
7. **Регистры не имеют модулей** в модели эталона (`MetadataObjectInformationRegister`
   не переопределяет методы модулей), хотя узлы модулей в дереве создаются
   (`MetadataTreeBuilder.cpp:111-112`).
8. **Порядок «магических» индексов** нигде не документирован в коде, кроме комментариев
   в `struct MetadataTreePaths` (`MetadataObjectWithSections.h:15-24`); проверки
   «таблица с таким индексом действительно имеет ожидаемый GUID» нет (кроме трёх
   мест — п.2.6, п.2.7, `Tabular.cpp`).
9. Для элементов форм/макетов количество **не читается** — цикл идёт по всем siblings,
   что делает разбор устойчивым к отсутствию/несоответствию счётчика, но не позволяет
   обнаружить «лишние» узлы.
