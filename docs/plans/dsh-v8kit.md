# План: плагин DeepSeek Harness для чтения конфигураций 1С (.cf/.cfe)

Статус: утверждён, реализация — ветка `feature/dsh-v8kit`.
Трекер этапов — раздел 12; отметки о выполнении ведутся там же.

## 1. Цель и критерии готовности

В GUI DeepSeek Harness пользователь открывает файл 1С и работает с ним, не покидая
приложение: дерево метаданных, исходный код модулей с подсветкой, формы в их точном
виде, макеты. Чтение — существующим `v8reader_cli`, без второй реализации формата.

Готово, когда в запущенной GUI DSH:

1. `.cf` и `.cfe` из дерева Files (и по ссылке на файл в чате) открываются в правом
   сайдбаре вкладкой с деревом категорий/объектов и содержимым объекта.
2. Вкладка «1C» из `+` правого сайдбара открывает произвольный абсолютный путь к
   `.cf/.cfe/.epf/.erf` (файл может лежать вне рабочего каталога).
3. Для объекта показываются реквизиты, табличные части, формы, макеты, модули;
   модуль — код с подсветкой BSL и настройками цветов/размера (сохраняются между
   сессиями); форма — существующий веб-рендерер «точного вида»; макет — таблица MXL
   с масштабом.
4. Тема light/dark применяется к панели; стили рендерера формы не протекают в UI;
   закрытие вкладки отменяет HTTP-запросы и дочерние процессы.
5. `ctest` и `npm test` зелёные, в CI есть job плагина, `npm pack --dry-run` не
   содержит путей вне каталога плагина.

## 2. Вне объёма

Редактирование и запись; новые разборщики (8.2-формы, обычные формы приложений
остаются как сейчас); MXL→PDF из панели; агентские инструменты `v8_*` (отдельный
этап); Electron-десктоп DSH (`file://` + IPC, там HTTP-маршрута нет); SSH/удалённые
workspace; параллельный diff конфигураций.

## 3. Архитектура

Плагин двухполовинный, обе половины в одном npm-пакете.

```
Браузер (Client half)                     Хост DSH (Host half)                Процесс
+---------------------------+  fetch      +----------------------+  spawn     +----------------+
| вкладка-страница «1C»     | ----------> | HTTP prefix /v8reader| ---------> | v8reader_cli   |
| тип документа .cf/.cfe    |             |  state/form/mxl/...  |  stdout    | view --json    |
| PanelView: дерево | код   | <---------- |  assets: formpreview | <--------- | form-model     |
| <iframe> форма / макет    |  JSON/HTML  |  LRU-кэш, таймауты   |   JSON     | mxl --html     |
+---------------------------+             +----------------------+            +----------------+
```

Точки интеграции в DSH (проверены по живому реестру слотов текущей установки):

| Что | Механизм |
|---|---|
| Страница «1C» из `+` сайдбара | `ctx.sidebarRightTabs.register({ id, kind, title, guide, keepMounted })` + тело в `sidebar.right.pane.tab` + заголовок в `sidebar.right.pane.tab.title` |
| Автооткрытие `.cf/.cfe/.epf/.erf` из дерева Files и ссылок | `ctx.documentPreviews.register({ id, extensions, binaryExtensions, priority, title, loading:'renderer' })` + тело в `sidebar.right.tab.document` |
| Данные клиенту | `ctx.webServer.register(...)` — Fetch-подобный ответ на префикс `/v8reader` |
| Абсолютный путь открытого файла | метаданные `WorkspaceFileStat { absolutePath, version, bytes }` через `useResource<'file'>(tab.contentId)` |
| Запуск CLI | `node:child_process.spawn` с argv-массивом; на Windows дерево убивается `taskkill /T`, на POSIX — `SIGKILL`. Сервис `ctx.subprocess` не используется: лишняя зависимость от чужой реализации процесса и второй путь исполнения, который пришлось бы держать в тестах |
| Локализация и метаданные плагина | `ctx.locale.bind('v8reader')`, `locale/ru.json`, `locale/en.json` с `meta.title/description/detail` |

Обе точки входа используют один React-компонент `PanelView`; различие — источник
пути (ввод пользователя против `absolutePath` из ресурса).

## 4. Репозиторий и границы

Каталог `plugins/dsh-v8reader/` в этом репозитории, автономный пакет со своей
версией. Переезд в отдельный репозиторий позже — `git subtree split`.

Правила границы с первого коммита: плагин не импортирует ничего из `../../src` и
`../../resources` в рантайме; ресурсы рендерера формы копируются шагом сборки;
контракт с C++ — только argv + JSON со `schema`.

```
plugins/dsh-v8reader/
  package.json             dsh.bundle.patch; dsh.client{platform,immediately,inject};
                           exports ./client, ./locale/*.json, ./icon
  cordis.patch.yml         insert: id dsh-v8reader, name @fishca/dsh-v8reader
  build.mjs                esbuild: src/client -> client.js (обёртка __ModuleLoader__.load)
  scripts/sync-assets.mjs  resources/formpreview/** -> assets/formpreview/
  client.js                собранный артефакт (в files)
  src/host/*.js            маршруты, spawn, кэш, пути
  src/client/*.jsx         entry, PanelView, CodeView, Tree, i18n
  assets/formpreview/**    копия веб-рендерера формы + model.html
  locale/{ru,en}.json      meta.* + строки UI
  docs/DETAIL.md  icon.svg  test/*.test.js
```

В репозитории появляются: `src/cli/ViewCommand.{h,cpp}`, `src/core/MxlHtmlExporter.{h,cpp}`
и строки в `CMakeLists.txt`; `.gitignore` получает `plugins/**/node_modules/`, `*.tgz`.

## 5. Контракт CLI (M1, версия приложения 0.14.0)

### 5.1 `view` — один вызов на экран

```
v8reader_cli view <файл> --json [--object <имя|guid|синоним>] [--module <имя>]
```

```json
{
  "schema": 1,
  "appVersion": "0.14.0",
  "file": { "path": "...", "size": 0, "modified": "...",
            "kind": "configuration|extension|externalProcessing|externalReport",
            "name": "...", "synonym": "...", "version": "...", "compatibilityMode": "...",
            "formatVersion": 4, "objectCount": 15523, "warnings": [] },
  "categories": [ { "guid": "...", "title": "Документы", "typeTitle": "Документ", "count": 1234,
                    "objects": [ { "guid": "...", "name": "АвансовыйОтчет", "synonym": "...",
                                   "typeTitle": "Документ", "elementFound": true } ] } ],
  "object": { "guid": "...", "name": "...", "synonym": "...", "typeTitle": "Документ",
              "typeText": "", "comment": "...",
              "sections": [ { "kind": "attributes", "title": "Реквизиты",
                              "entries": [ { "name": "...", "synonym": "...",
                                             "typeText": "Строка(50)", "fillChecking": 0,
                                             "comment": "", "children": [] } ] } ],
              "modules": [ { "name": "МодульОбъекта", "size": 12345 } ] },
  "content": { "kind": "module", "name": "МодульОбъекта", "text": "..." }
}
```

- `kind` в `sections` — стабильные строки: `attributes`, `tabularSections`,
  `tabularAttributes`, `forms`, `templates`, `commands`, `dimensions`, `resources`,
  `accountingFlags`, `subcontoFlags`, `values`, `columns`, `journalColumns`.
- `object`/`content` равны `null`, если не запрошены. Модули — через `ModuleExtractor`;
  `--module` добавляет текст в `content`.
- Основное дерево — один вызов; для БП ≈ 1,8 МБ JSON (~0,2 МБ gzip).

### 5.2 `mxl` — экспорт макета

`mxl` получает `--template <имя>`, `--json` (сводка: строки, колонки, области,
печать) и `--html <файл|->` (самодостаточная HTML-таблица: объединения через
`rowspan/colspan`, стили из `CellFormat`, цвета/шрифты из таблиц `Document`,
экранирование текста, `@page` из `printSettings`, контур именованных областей).
Экспортёр — `src/core/MxlHtmlExporter`, без зависимости от QtGui.

### 5.3 Совместимость

Плагин требует CLI ≥ 0.14.0: при активации зовёт `--version`; при меньшей версии
панель показывает причину вместо пустого экрана. Приложение 0.14.0 выходит до плагина.

## 6. Host-половина плагина

Маршруты (все `GET`, без CORS-заголовков; запросы с чужим `Sec-Fetch-Site`
отклоняются):

| Маршрут | Ответ |
|---|---|
| `/v8reader/state?path=[&object=&module=]` | JSON из `view` |
| `/v8reader/browse?path=[&all=1]` | один уровень каталога для обзора файлов: `dir`, `parent`, `breadcrumbs`, `entries[{name,path,kind,size?}]`, `truncated`, `hiddenCount`; без `path` — домашний каталог, путь к файлу сводится к его каталогу |
| `/v8reader/form?path=&object=&form=` | HTML-страница формы: модель `form-model` вставлена в `model.html`, ассеты подключены через `<base href="/v8reader/assets/">` |
| `/v8reader/assets/<путь>` | файлы `assets/formpreview`, `Cache-Control: no-cache` + ETag |
| `/v8reader/health` | `{ ok, cli: { path, version }, schema }` |

Правила исполнения:

- Поиск бинарника: `config.cliPath` → `env.V8READER_CLI` → выходы сборки этого
  репозитория (`out/cli-deploy`, `out/mingw`, `out/vs`, `out/build` — удобство
  разработки, вне репозитория эти пути просто не существуют) → `PATH`; кэш пути
  до отказа.
- Конфигурация строки плагина (без объявленной схемы `Config` в v1): `cliPath`,
  `timeoutMs = 20000`, `maxJsonBytes = 16777216`, `maxConcurrent = 2`,
  `cacheEntries = 32`, `cacheBytes = 67108864`.
- Аргументы — массивом, без shell; `cwd` — каталог файла; таймаут → kill процесса
  (Windows — `taskkill /T`), ответ 504.
- Ошибка CLI (exit ≠ 0) → статус 4xx/5xx с `{ error: { code, message, detail? } }`;
  `code` из набора: `bad-path`, `unsupported-format`, `cli-failed`, `cli-missing`,
  `cli-old`, `too-large`, `timeout`, а также `forbidden`, `not-found`,
  `method-not-allowed` (добавлены на M2 для запросов не с того сайта, неизвестного
  маршрута и не-GET метода) и `denied` (нет доступа к каталогу при обзоре).
- Обзор каталога (`browse`) — только чтение: `readdir`/`stat`, без записи; dot-файлы
  и системные каталоги скрыты, по умолчанию видны только `cf|cfe|epf|erf`
  (`all=1` — все файлы), не больше 1000 записей на ответ.
- Проверка пути: абсолютный, существует, обычный файл, расширение из
  `cf|cfe|epf|erf`, без NUL; UNC-пути допустимы.
- LRU-кэш ответов `state` по `(path, size, mtimeMs, object, module)`, 32 записи /
  64 МБ; на изменение файла панель получает новый `version`.
- Регистрация маршрута — внутри `ctx.effect`; запросы прерываются при выгрузке.

## 7. Клиентская половина

**Тип вкладки-страницы**: `id = '@fishca/dsh-v8reader'`, `kind = 'v8reader'`,
`guide: [{ id: 'open', title, description }]`, `keepMounted: true`. Тело —
`PanelView` в режиме `path`: поле ввода абсолютного пути и обзор файлов на
диске (`FilePicker` поверх `/v8reader/browse`, стартует с последнего каталога
из `localStorage`).

**Тип документа**: `id = '@fishca/dsh-v8reader/config'`,
`extensions: ['cf','cfe','epf','erf']`, те же `binaryExtensions`,
`priority: 'extension'`, `loading: 'renderer'`. Тело — `PanelView` в режиме
`address`: `useResource<'file'>(tab.contentId)` → `absolutePath` → `state`.

**PanelView** (общий): слева фильтр и дерево «категория → объект» (виртуализация);
справа заголовок объекта, вкладки разделов, таблица реквизитов, деревья табличных
частей, списки форм/макетов, выбор модуля. Все строки через `ctx.locale`, стили —
токены `--dsw-*`.

**CodeView**: BSL-токенайзер, портируемый из `src/gui/BslHighlighter.cpp` (порядок
правил: ключевые слова ru/en без регистра, `^\s*#\w+`, `&\w+`, числа, строки с `""`,
комментарии `//`), номера строк, перенос, копирование; цвета/размер по умолчанию из
`BslHighlightSettings` для светлой и тёмной темы, хранение в
`localStorage['dsh.v8reader.code.v1']`. Файл 1–2 МБ рендерится порциями.

**Сборка клиента**: esbuild `--format=cjs --bundle --external:react --jsx=transform
--jsx-factory=React.createElement`; `build.mjs` оборачивает бандл в обёртку
`window.__ModuleLoader__.load({ id, factory(require){ … return module.exports } })`;
`client.js` коммитится.

## 8. Формы

Отдельный рендерер не пишем. Используется существующая веб-страница точного вида
(`resources/formpreview/model.html`, `form-preview.js`, `viewer.css`,
`form-preview-85.css`, `icons.svg`, `platform-*.png`, `std-pictures/*`), которая
читает модель из `window.__QV8_MODEL__` или `?model=`. Отличия от GUI:

- `scripts/sync-assets.mjs` копирует `resources/formpreview/**` в
  `assets/formpreview` (429 файлов, ≈2,4 МБ, копия закоммичена);
- `/v8reader/form` отдаёт `model.html` с вставленной моделью и
  `<base href="/v8reader/assets/">`, поэтому относительные URL страницы
  разрешаются под `/v8reader/assets/`, а ошибка CLI приходит JSON-ответом до
  загрузки страницы;
- модель кэшируется на хосте по `(path, size, mtime, object, form)`;
- `iframe sandbox="allow-scripts allow-same-origin"` внутри вкладки; панель
  следит за `window.__QV8_STATE` и показывает баннер при ошибке рендерера;
- масштаб (50–150 %) пишется в `documentElement.style.zoom` кадра, поэтому
  перерисовка не перезагружает форму.

Ограничение: формы 8.2 с короткой записью макета показываются с известными
упрощениями (см. ограничения в `docs/research/forms-format.md`).

## 9. Макеты

1. `src/core/MxlHtmlExporter` — `Document` → HTML.
2. `mxl --html` в CLI, `--template` для выбора макета, `--json` для сводки.
3. Панель: список макетов, `<iframe src="/v8reader/mxl?...&zoom=100">`; масштаб
   25–400 % и «по ширине» меняют `zoom` в URL.

## 10. Ошибки и крайние случаи

| Случай | Поведение |
|---|---|
| `.cfu`, `.mxl`, неизвестное расширение | рендерер не заявляет их; открывается штатный предпросмотр |
| повреждённый или зашифрованный контейнер | `error.code = cli-failed` + текст stderr в панели |
| `.epf/.erf` | работают как одиночный объект: дерево из одной категории |
| кириллица/пробелы/UNC в пути | `encodeURIComponent` на клиенте, декодирование один раз на хосте |
| файл перезаписан при открытой вкладке | сравнение `size`/`modified`, баннер «Обновить» |
| форма > `maxJsonBytes` | `too-large` с подсказкой открыть в GUI |
| CLI медленнее `timeoutMs` | процесс убит, 504, панель предлагает повторить |
| закрытие вкладки/выгрузка плагина | `abort` fetch, kill дочерних процессов, dispose маршрута |
| плагин установлен, CLI нет | панель показывает пути поиска и как задать `cliPath` |

## 11. Тесты и проверка

**C++** (`ctest`):
- `tests/test_cli.cpp`: `view --json` на синтетическом контейнере
  (`TestContainerBuilder`) — `schema`, `file.kind`, `categories`, `object.sections`,
  `content.kind=module`; без `--json` текстовый вывод не меняется; `--object` по
  GUID/имени/синониму; неизвестный объект → exit 1.
- `tests/test_mxl.cpp`: экспортёр на собранном в памяти `mxl::Document` —
  объединения, экранирование `<&"`, цвета/шрифты, `format=0` без падений.
- Гейт `control_fixture_check.py` продолжает работать: текстовые команды не меняются.

**Node** (`node --test`):
- юнит: парсинг и валидация пути, коды ошибок, ключ кэша, экранирование,
  проверка `schema`/`--version`;
- интеграция: `V8READER_CLI` + `tests/fixtures/controlbase/1Cv8_controls.cf` —
  `state` без объекта и с объектом, `/v8reader/form` и HTML формы, `mxl --html` на макет;
- сборка: `client.js` содержит `__ModuleLoader__` и не содержит `import React`.

**Ручная проверка в GUI**: установка бандла, открытие фикстуры из дерева Files,
обход дерева, форма, макет, темы light/dark, консоль без ошибок, отсутствие
«висящих» процессов `v8reader_cli`.

**CI** — job `plugin` в `build.yml`: Node 22, `pnpm install --frozen-lockfile`,
`pnpm run build`, `pnpm test` с `V8READER_CLI`, `npm pack --dry-run`.

## 12. Этапы

- [x] **M1** CLI 0.14.0: `view --json`, `mxl --template/--json/--html`,
      `MxlHtmlExporter`, тесты, справка, CHANGELOG.
      Готово: 6/6 `ctest`, `test_mxl` — 13 кейсов, `test_cli` — 21 кейс;
      на БП `mxl --json` — 2 макета за 1,2 с, HTML первого — 129 КБ,
      страница снята headless Chrome и сверена глазами (таблица, объединения,
      контур областей).
- [x] **M2** Каркас плагина: пакет, patch, локаль/иконка/DETAIL; Host: маршрут
      `state`, spawn, кэш, ошибки; Client: тип вкладки-страницы + `PanelView`
      (путь, дерево, разделы, реквизиты); локальная установка.
      Готово: `node --test` — 19 кейсов (host 14, client 5); бандл установлен в
      профиль `desktop` (`application: applied`, symlink на каталог репозитория),
      живой `/v8reader/health` нашёл `out/cli-deploy/v8reader_cli.exe` 0.14.0,
      `/v8reader/state` на фикстуре `1Cv8_controls.cf` — 200 за 84 мс (2
      категории, etag), состав объекта и модуль «Кнопки» — 272 символа, ошибки
      `bad-path` 400 и `unsupported-format` 415. Клиент собран esbuild
      (`npm run build`), добавлен кандидат `out/cli-deploy` в поиск бинарника.
      Клиентская половина в живой графе `/plugins/events`: запись
      `@fishca/dsh-v8reader` с `immediately` и `inject`, её бандл отдаётся 200
      и содержит `__ModuleLoader__` и `v8r-toolbar`.
      Не проверено автоматически: отрисовка панели в GUI — браузерный канал
      этой сессии не открывает loopback-адрес DSH (прокси и отсутствие
      авторизованной вкладки), нужен взгляд человека.
- [x] **M2.1** Обзор файлов вместо слепого «Открыть»: маршрут `browse`
      (`dir`, `parent`, `breadcrumbs`, `entries`, `hiddenCount`, `truncated`,
      ошибки `bad-path`/`denied`), `FilePicker` в панели (крошки, поле пути
      для другого диска и UNC, фильтр, «Все файлы», недоступные файлы не
      выбираются), «Открыть» с пустым полем открывает обзор, «Обзор…» и
      «Выбрать файл…» в пустом состоянии, последний каталог в
      `localStorage['dsh.v8reader.dir.v1']`.
      Готово: `node --test` — 29 кейсов (host 19, client 10), живой
      `/v8reader/browse?path=…controlbase` — 200, 7 крошек, `1Cv8_controls.cf`
      в списке; в GUI руками не проверено (см. выше).
- [x] **M2.2** Дерево как в 1С: корень «Конфигурация» → категории → объекты →
      разделы объекта (реквизиты, табличные части, формы, команды, макеты,
      модули) → листья; формы и модули открываются из дерева, листья разделов
      переключают вкладку; фильтр по имени/синониму с сохранением родителей
      совпавших вложенных реквизитов; порции по 200 объектов и 100 листьев.
      Готово: `test/client.test.js` — группы, вложенность, фильтрация, счётчики;
      живой `/v8reader/state` на фикстуре отдаёт 2 категории и 2 объекта.
- [x] **M3** Код: `CodeView`, порт BSL-правил, настройки, лимит строк, копирование.
      Готово: `src/client/bsl.js` повторяет правила `BslHighlighter.cpp`
      (ключевые слова ru/en, `#…` в начале строки, `&…`, числа, строки с
      `""`, `//`); палитра light/dark из `BslHighlightSettings`, схема берётся
      из сервиса `theme` и обновляется по `theme/change`; номера строк,
      перенос, размер шрифта, отключение подсветки, копирование, порции по
      1500 строк; настройки в
      `localStorage['dsh.v8reader.code.v1']`; `test/bsl.test.js` — 6 кейсов,
      клиентские тесты настроек; `node --test` — 49 кейсов.
- [ ] **M4** Документ-рендерер: `documentPreviews.register` + тело по `absolutePath`,
      Reload, баннер изменения файла.
- [x] **M5** Формы: `sync-assets`, маршрут `form`, ассеты, iframe, кэш.
      Готово: `scripts/sync-assets.mjs` (429 файлов, 2,45 МБ, копия
      закоммичена); `/v8reader/form` вставляет модель в `model.html` и отдаёт
      HTML одним запросом, ошибки — JSON до страницы; `/v8reader/assets/…` с
      `no-cache` и ETag, защитой от выхода из каталога; панель следит за
      `__QV8_STATE`, показывает баннер при ошибке, даёт масштаб 50–150 % и
      «Обновить»; модель кэшируется по `(путь, размер, mtime, объект, форма)`.
      Проверено: `node --test` — 27 host-кейсов, включая реальный
      `form-model` на фикстуре; живой `/v8reader/form` (объект по GUID и по
      имени) — 200 с вставленной моделью, `/v8reader/assets/css/viewer.css` —
      200. Отрисовка панели глазами не проверена: браузерного канала к
      loopback-GUI в этой сессии нет.
- [ ] **M6** Макеты и полировка: iframe макета, масштаб, объявленный `Config`,
      i18n en, job CI, релиз плагина 0.1.0.

Критерии приёмки каждого этапа — раздел 1 и соответствующие строки разделов 5–9.

## 13. Риски и допущения

1. Точные имена полей Route API и опций `documentPreviews` уточняются на M2/M4 по
   исходникам установленных пакетов (`WebRoute`, `DocumentPreviewRegistration`).
   Если рендерер документа не заработает — остаётся вкладка-страница.
2. Скорость: один вызов CLI на экран ≈ 0,9 с на БП. Если p95 > 2 с — отдельный
   этап `v8reader_cli serve` (построчный JSON); архитектура не меняется.
3. Безопасность маршрута: чтение произвольного абсолютного `.cf/.cfe/.epf/.erf` в
   рамках того же origin. Защита — loopback, отсутствие CORS, `Sec-Fetch-Site`,
   отсутствие записи. При необходимости — токен-капабилити в M6.
4. Установка в профиль: `plugin_manager install_bundle` может требовать
   подтверждения; тогда локальная установка через **Plugins → Add plugin**.
5. Ресурсы рендерера формы копируются шагом сборки; `npm pack` обязан их включать
   (проверка в CI).
6. Допущение: профиль DSH содержит `ui-sidebar-documentpreview`; при его отсутствии
   плагин активируется без рендерера документа, вкладка-страница работает.

## 14. Документация и релизы

`plugins/dsh-v8reader/README.md`, `docs/DETAIL.md`, раздел в корневом README,
`docs/release-notes-v0.14.0.md`, запись в `CHANGELOG.md`; job `plugin` в
`.github/workflows/build.yml`; версия плагина независима от версии приложения,
`minCliVersion = 0.14.0`.
