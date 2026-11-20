# Точный вид формы: контракт рендерера BslEdit

> Исследовательские заметки по встраиванию HTML-рендерера управляемых форм
> из проекта [BslEdit](https://github.com/alonehobo/BslEdit) (MIT) в qtv8reader.
> Описаны оба входа в бандл (XML-путь как эталон и JSON-модель, которая
> используется в приложении), полная схема JSON-модели, контракт ресурсов для
> C++-хоста, результаты абляций (что влияет на вид) и ограничения.
> Архитектура встраивания в приложение — [../form-exact-view.md](../form-exact-view.md).
> Пример модели, собранной этим исследованием — `model-ao-min.json` (не входит
> в репозиторий, генерируется командой `v8reader_cli form-model`).


Рабочая папка: `C:\work\scratch\qv8-exact`.
Источник рендерера: BslEdit (`C:\work\scratch\bsledit\bsledit-main`, MIT).
Дамп конфигурации: `C:\work\#tmp#\v8reader\configfiles` (XML-вид, получен `v8unpack -PARSE`).
Целевая форма: `Documents\АвансовыйОтчет\Forms\ФормаДокумента`.

---

## 1. Что получилось (кратко)

1. **XML-путь** — `bundle/index.html`: читает `Ext/Form.xml` и окружающие метаданные из дампа,
   строит контекст штатным `form-context.js` из BslEdit, парсит и рисует форму.
   Скриншот: `shots/form-ao.png`.
2. **JSON-путь (главный результат)** — `bundle/model.html`: **не читает XML вообще**.
   Принимает готовую JSON-модель (`bundle/model.json`, `model-ao.json`, `model-ao-min.json`
   или объект в `window.__QV8_MODEL__`) и вызывает `FormPreview.render(model, host, options)`.
   Скриншот: `shots/form-ao-model.png`.
   **Рендер из JSON-модели побайтово совпадает с XML-путём**:
   `form-ao.png`, `form-ao-model.png` и `form-ao-model-inline.png` имеют одинаковый SHA-256
   `524b16f8b1e2f4a5ae2b98aca294fc7dcd15dcd5c3c0cf4950b0cbd3b5243790`.
3. **Модель формы** описана в разделе 8. Это и есть контракт для C++:
   C++/Qt может строить такую модель из своего разбора скобочного формата `.cf`,
   не конвертируя его в XCF-XML.
4. **io-лог** полного XML-рендера: `io-log-ao.json` (1322 записей, 45,1 МБ),
   сгруппированный: `io-paths-ao.json`. Сводка — раздел 7.
5. Лицензия MIT BslEdit сохранена: `bundle/LICENSE-BslEdit-MIT.txt`.

### Ключевой вывод

Рендерер BslEdit **не требует XML в рантайме**. `FormPreview.render` принимает уже
разобранную («скомпилированную») модель — обычный JSON-объект. Вся сложность XML-пути
(`form-context.js`, 560 общих команд, 30-МБ `ConfigDumpInfo.xml`, `Picture.zip` и т.д.)
нужна только для того, чтобы **построить** эту модель. Если C++ отдаёт модель сама,
XML-контекст и весь дамп в рантайме не нужны.

При этом модель не является «голым деревом элементов»: для точных подписей, ширин и
иконок в ней должны быть заранее разрешены метаданные владельца (`model.objectMeta`),
общие команды (`model.commonCommands`), стили (`model.styleItems`) и картинки
(`model.commonPictures`). Это разобрано в 8.11–8.16. Если C++ умеет отдать эти сведения
из своего разбора `.cf` — вид будет точным без XML.

---

## 2. Стенд: запуск и скриншоты

### 2.1 Файлы стенда

```
C:\work\scratch\qv8-exact\
  bundle\              статический бандл (единственное, что нужно в проде)
  config\              junction -> C:\work\#tmp#\v8reader\configfiles
  model-ao.json        модель АО (полная, все смысловые блоки; 980 КБ)
  model-ao-min.json    модель АО (только то, что нужно для пиксель-в-пиксель; 674 КБ)
  model-minimal-ao.json эталон для C++ (копия model-ao-min.json; 674 КБ)
  model-ao-full.json   исходный полный дамп модели после parse() (1 036 КБ)
  model-ao-posting.json пример модели с добавленными «Провести/Провести и закрыть»
  io-log-ao.json       полный io-лог XML-рендера
  io-paths-ao.json     тот же лог, сгруппированный по областям конфигурации
  shots\               скриншоты и вспомогательные дампы
  inline-test.html     dev-пример инъекции window.__QV8_MODEL__
  variants\            dev-эксперименты по деградации модели (см. 8.19)
```

`config` — junction, чтобы всё отдавалось с одного origin:

```powershell
New-Item -ItemType Junction -Path C:\work\scratch\qv8-exact\config `
  -Target C:\work\#tmp#\v8reader\configfiles
python -m http.server 8777 --bind 127.0.0.1 --directory C:\work\scratch\qv8-exact
```

### 2.2 URL

XML-путь:

```
http://127.0.0.1:8777/bundle/index.html?src=/config&form=Documents/АвансовыйОтчет/Forms/ФормаДокумента
```

JSON-путь:

```
http://127.0.0.1:8777/bundle/model.html                        # bundle/model.json
http://127.0.0.1:8777/bundle/model.html?model=../model-ao.json # любой URL модели
```

Скриншот headless Edge (Windows):

```powershell
& 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe' `
  --headless=new --disable-gpu --hide-scrollbars --window-size=1280,900 `
  --virtual-time-budget=120000 --user-data-dir="$env:TEMP\qv8-edge-shot" `
  --screenshot="C:\work\scratch\qv8-exact\shots\form-ao-model.png" `
  "http://127.0.0.1:8777/bundle/model.html"
```

`--user-data-dir` обязателен, если Edge уже запущен; `--virtual-time-budget` нужен,
потому что модель рисуется после асинхронной загрузки JSON/картинок.

### 2.3 Скриншоты

| Файл | Что это |
|---|---|
| `shots/form-ao.png` | АО.ФормаДокумента, XML-путь (эталон) |
| `shots/form-ao-model.png` | та же форма из `bundle/model.json` (модель, без XML); SHA-256 совпадает с `form-ao.png` |
| `shots/form-ao-model-inline.png` | модель, переданная через `window.__QV8_MODEL__`; тоже совпадает |
| `shots/form-ao-model-tovary.png` | модель АО, вкладка «Товары» (клик по вкладке после рендера) |
| `shots/form-ao-model-posting.png` | модель АО + добавленные вручную «Провести и закрыть», «Записать», «Провести» |
| `shots/form-ao-list.png` | АО.ФормаСписка, XML-путь (проверка второго вида) |
| `shots/variant-*.png` | эксперименты по удалению блоков модели (раздел 8.19) |

---

## 3. Два входа в бандл

### 3.1 `index.html` — XML-путь (эталон/отладка)

Порядок работы `bundle/js/app.js`:

1. `fetch` формы `src + '/' + form + '/Ext/Form.xml'`.
2. `FormContext.createResolver(io, {maxBytes: 64MB, cache:{}}).resolve(formPath, formXml)`.
3. `PreviewProviders.parse(provider, formXml, context)` — это тонкая обёртка над
   `FormPreview.parse(formXml, objectMeta, styleItems, baseForm, commonCommands,
   commonPictures, refMeta, interfaceMode)`.
4. `FormPreview.render(parsed.model, host, {windowTitle, filePath})`, затем
   `FormPreview.reflow(host)` (два прохода; второй — после кадра).
5. `window.__QV8_READY`, `data-qv8-state="ready"`, `window.__QV8_INFO`, при `?dump=1` —
   io-лог в `<script id="qv8-io-log">`.

Параметры: `src`, `form`, `title`, `noctx=1` (диагностика без контекста), `debug=1`,
`dump=1`, `dumpmodel=1` (dev; выгружает `JSON.stringify(parsed.model)` в DOM).

### 3.2 `model.html` — JSON-модель (рекомендуется для C++)

`bundle/model.html` + `js/xml-util.js` + `js/form-preview.js` + `js/model-app.js`
(+ CSS и картинки). **`form-context.js`, `providers.js`, `qv8-io.js` и XML не нужны.**

`model-app.js`:

1. Берёт `window.__QV8_MODEL__`, иначе `fetch(?model=...)` (по умолчанию `model.json`).
2. Восстанавливает несериализуемое: `_fpFormVersion` на всех элементах (см. 8.17).
3. `FormPreview.render(model, host, {windowTitle: ?title, filePath: ''})`.
4. `FormPreview.reflow(host)`; при `?tab=Имя` — клик по вкладке и повторный reflow.
5. Готовность: `data-qv8-state="ready"`, `window.__QV8_INFO`, `window.__QV8_READY`.

Проверка «никакого XML»: при `?dump=1` страница пишет `performance.getEntriesByType('resource')`.
Для АО там ровно 9 ресурсов: 3 CSS, 3 JS, `model.json`, `favicon.ico`,
`platform-data-history.png`. Ни одного `.xml`.

---

## 4. Контракт для C++/Qt/WebView2

### 4.1 Origin

Всё — с одного origin, например `https://qv8.local/` (WebView2 virtual host).
Никаких `localhost`, CDN, `file://` и абсолютных путей в бандле.

Рекомендуемая раскладка:

```
https://qv8.local/                 -> bundle/
https://qv8.local/model.json       -> модель (или инъекция, см. 4.5)
https://qv8.local/config/...       -> только для XML-пути (если он вообще нужен)
```

Бандл использует **относительные** URL от текущей страницы. `model.html`,
`js/*`, `css/*`, `std-pictures/*`, `platform-*.png` должны лежать так, как лежат.

### 4.2 URL-параметры

`index.html`:

| Параметр | По умолчанию | Смысл |
|---|---|---|
| `src` | `''` | префикс корня конфигурации; на стенде `/config`; в проде можно `''`. Это **относительный** URL: `''` = каталог, где лежит `index.html`; если бандл лежит в `/bundle/`, а конфигурация в корне origin — задайте `src=..` (или разместите `index.html` в корне) |
| `form` | `Documents/АвансовыйОтчет/Forms/ФормаДокумента` | папка формы относительно корня; допустимы `.../Forms/<Name>.xml` и `.../Ext/Form.xml` |
| `title` | — | переопределить заголовок окна |
| `noctx` | — | `1` — не строить контекст (диагностика) |
| `debug` | — | `1` — панель диагностики |
| `dump` | — | `1` — выгрузить io-лог и info в DOM |
| `dumpmodel` | — | dev: выгрузить JSON модели в DOM |

`model.html`:

| Параметр | По умолчанию | Смысл |
|---|---|---|
| `model` | `model.json` | URL JSON-модели |
| `title` | — | переопределить заголовок окна |
| `tab` | — | заголовок вкладки, которую нажать после рендера |
| `dump` | — | `1` — выгрузить info и resources в DOM |

### 4.3 Файлы, MIME, 404, HEAD, кэш

Обязательные типы:

| Расширение | Content-Type | Зачем |
|---|---|---|
| `.html` | `text/html; charset=utf-8` | точки входа |
| `.js` | `text/javascript; charset=utf-8` | рендерер и обвязка |
| `.css` | `text/css; charset=utf-8` | стили |
| `.json` | `application/json; charset=utf-8` | модель |
| `.xml` | `application/xml; charset=utf-8` | **только XML-путь**; рендерер сам делает `DOMParser(...,'application/xml')` |
| `.png` | `image/png` | `std-pictures`, `platform-*.png` |
| `.svg` | `image/svg+xml` | `std-pictures/*.svg` |
| `.zip` | `application/zip` | **не отдаётся по HTTP**: `Picture.zip` приходит внутри модели как base64 и распаковывается браузером через `DecompressionStream` |
| `.mxl` | `application/octet-stream` | для формы не нужен; только если C++ позже откроет макеты |

Критично:

* `.js` должен быть именно JavaScript-MIME, иначе Chromium откажется исполнять модуль
  при строгом `X-Content-Type-Options: nosniff` (для классических `<script src>` браузер
  обычно снисходителен, но не полагайтесь на это).
* `.json` должен быть `application/json` (или хотя бы не `text/html`).
* UTF-8 без BOM предпочтителен; BOM в `.json` ломает `JSON.parse`, в JS-файлах допустим.
* `HEAD` нужен **только XML-пути** (`io.exists`). Model-путь `HEAD` не использует.
  Если XML-путь не нужен, `HEAD` можно не реализовывать.
* 404: пустой ответ, `Content-Type: text/plain`, без HTML-страницы и без редиректа.
  Рендерер трактует 404 как «файла нет» и продолжает; 200 с HTML-ошибкой — сломает
  разбор. 403/410 тоже допустимы как «нет».
* `Content-Length` желателен; `Range` не нужен.
* Кэш: `Cache-Control: no-store` для `model.json` (его читает `fetch(...,{cache:'no-store'})`);
  для JS/CSS/картинок можно `max-age=31536000, immutable` (они версионируются бандлом).
* CORS не нужен: всё same-origin.
* CSP: достаточно `default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline';
  script-src 'self'`. Рендерер не использует `eval`, внешние шрифты и сеть.

### 4.4 Отдача из контейнера `.cf`

C++ уже умеет читать потоки `.cf`. Для XML-пути контракт — «виртуальная файловая система»
с корнем конфигурации:

```
<config root>/Configuration.xml
<config root>/ConfigDumpInfo.xml
<config root>/Documents/АвансовыйОтчет.xml
<config root>/Documents/АвансовыйОтчет/Forms/ФормаДокумента.xml
<config root>/Documents/АвансовыйОтчет/Forms/ФормаДокумента/Ext/Form.xml
...
```

`form-context.js` сам строит пути-кандидаты и делает `HEAD`/`GET`. C++ должен
сопоставить путь → поток внутри `.cf` (или во временном дампе) и отдать 200/404.
Никакого листинга каталогов не требуется: `readdir`/`existsMany`-листинг рендереру
не нужны (см. раздел 6.1); `existsMany` — это просто батч `HEAD`-ов.

**Для model-пути ничего из этого не нужно.** Нужны только файлы бандла и JSON модели.
Модель можно отдавать одним `model.json` (674 КБ для АО) или инъекцией (4.5).

Что откуда отдавать:

| Что | Источник | Примечание |
|---|---|---|
| `index.html`, `model.html`, `js/*`, `css/*`, `platform-*.png`, `std-pictures/*` | ресурсы приложения (Qt `.qrc`, встроенные массивы, память) | неизменны, можно кэшировать навсегда |
| `model.json` | память/сгенерированный JSON | лучше инъекция `window.__QV8_MODEL__` |
| `Configuration.xml`, `ConfigDumpInfo.xml`, `Documents/**`, `CommonCommands/**`, ... | контейнер `.cf` по требованию | только если XML-путь вообще нужен; мэппинг путь→поток, 404 при отсутствии |
| `Picture.zip` | **не отдавать по HTTP** | уже внутри модели как base64 |

Индекс `ConfigDumpInfo.xml` (30 МБ) не стоит держать в памяти целиком: для XML-пути
его можно отдать потоком из `.cf` (рендерер всё равно декодирует его в строку),
а лучше — заменить `io.cacheGet/cachePut` на заранее посчитанный каталог команд
(см. 7.6).

### 4.5 Инъекция модели (рекомендуется)

Чтобы не гонять 674 КБ JSON и не парсить его в JS, C++ может сделать:

```cpp
webview->AddScriptToExecuteOnDocumentCreated(
  L"window.__QV8_MODEL__ = " + jsonLiteral + L";", nullptr);
webview->Navigate(L"https://qv8.local/model.html");
```

`model-app.js` увидит `window.__QV8_MODEL__` и не будет делать `fetch`.
Требование: `jsonLiteral` — валидный JS-литерал (JSON с экранированными `</script>`,
U+2028/U+2029; проще `JSON.parse(<строка>)` или передать объект через
`ExecuteScript`). Проверено: `inline-test.html` даёт пиксель-в-пиксель тот же вид.

---

## 5. Состав бандла и размеры

Полный бандл: **3 182 292 байта, 432 файла**.

| Путь | Файлов | Байт | Нужен |
|---|---:|---:|---|
| `bundle/index.html` | 1 | 909 | XML-путь |
| `bundle/model.html` | 1 | 563 | model-путь |
| `bundle/model.json` | 1 | 673 901 | model-путь (АО) |
| `bundle/js/form-preview.js` | 1 | 1 295 798 | всегда |
| `bundle/js/xml-util.js` | 1 | 51 406 | всегда (`FormPreview` использует `XmlUtil` для заголовков/терминов) |
| `bundle/js/form-context.js` | 1 | 97 867 | только XML-путь |
| `bundle/js/providers.js` | 1 | 9 804 | только XML-путь |
| `bundle/js/qv8-io.js` | 1 | 8 509 | только XML-путь |
| `bundle/js/app.js` | 1 | 8 840 | только XML-путь |
| `bundle/js/model-app.js` | 1 | 6 536 | только model-путь |
| `bundle/css/viewer.css` | 1 | 342 948 | всегда: здесь базовые `.fp-root` / Taxi-правила |
| `bundle/css/form-preview-85.css` | 1 | 289 214 | только интерфейс 8.5; для Taxi не нужен (проверено: без него пиксель-в-пиксель) |
| `bundle/css/qv8-host.css` | 1 | 926 | всегда (хост-страница) |
| `bundle/std-pictures/` | 383 | 381 894 | общая библиотека `StdPicture.*`; для АО не запрашивается, но нужна другим формам |
| `bundle/platform-*.png` | 35 | 12 093 | иконки инспектора/истории; для АО запрашивается только `platform-data-history.png` |
| `bundle/LICENSE-BslEdit-MIT.txt` | 1 | 1 084 | лицензия |

Минимальные подмножества:

| Режим | Состав | Байт |
|---|---|---:|
| **Model-путь, АО** | `model.html`, `model.json`, `xml-util.js`, `form-preview.js`, `model-app.js`, `viewer.css`, `qv8-host.css`, `platform-data-history.png` | **2 372 705** |
| Model-путь, «общий» (8.5 + std-картинки) | то же + `form-preview-85.css` + `std-pictures/` | **3 043 813** |
| XML-путь | `index.html`, 7 JS (кроме `model-app.js`), 3 CSS, `std-pictures/`, `platform-*.png` | **2 500 208** |

Бандл не требует npm, сборки, Node.js в рантайме и не тянет ничего из сети.
`viewer.css` из BslEdit содержит и «оболочку» приложения, но она не конфликтует:
на странице используется только `#host` + `.fp-root`; остальные правила не задействованы.

> Оптимизация: если C++ поддерживает только Taxi-конфигурации, `form-preview-85.css`
> (289 КБ) можно не отдавать. Если нужен только АО, `std-pictures/` (382 КБ) тоже можно
> не отдавать — но для произвольных форм оставьте: часть кнопок/декораций использует
> `StdPicture.*`.

---

## 6. XML-путь: как BslEdit строит контекст

### 6.1 io-контракт `form-context.js`

Модуль ожидает объект `io` (все методы возвращают Promise):

```
io.exists(path) -> boolean
io.readBytes(path, limit) -> Uint8Array | null
   // опционально, для батчинга:
io.existsMany(paths) -> boolean[]
io.readMany(paths, limit, filter) -> (Uint8Array|null)[]
io.statMany(paths) -> (string|null)[]        // "size:mtime"; включает кэш каталога команд
io.cacheGet(directory, key) -> string|null
io.cachePut(directory, key, text) -> void
io.baseConfigurations(extensionRoot) -> string[]     // только расширения
io.extensionConfigurations(configurationRoot) -> string[]
io.moduleFiles(configurationRoot) -> string[]        // глобальный поиск по модулям
```

`path` — путь в **виртуальном абсолютном дереве** (см. 6.2), разделитель `/`.
`limit` — максимум байт (0/Infinity = без ограничения). `filter` — подсказка
`'command-metadata' | 'command-blocks' | 'configuration-properties' | 'md-links'`;
хост может её игнорировать, резолвер всё равно применит фильтр сам.

Наш `js/qv8-io.js` реализует `exists/readBytes/existsMany/readMany` через `fetch`
(HEAD/GET), без серверного batch-эндпоинта. `statMany/cacheGet/cachePut` не реализованы:
каталог команд перестраивается на каждой загрузке (см. 7.6 — как это кэшировать).

BslEdit в WebView2 использует `FormContext.createHttpIo(fileUrl, batchUrl)`:
`GET fileUrl?p=<path>&exists=1` и `POST batchUrl` с текстовым протоколом
(`exists`, `read`, `stat`, `cache-get`, ...). Для статического сервера этот протокол
неудобен, поэтому в бандле свой io. Формат батча задокументирован в шапке
`form-context.js` — если C++ захочет повторить именно его, это возможно.

### 6.2 Виртуальный корень `/@cfg`

Резолвер получает **абсолютный** путь формы и ищет метаданные в её предках:
`ancestors(formPath)` + `CommonCommands/<name>.xml`, `ConfigDumpInfo.xml` и т.д.
Если передать путь, относительный от корня конфигурации, то в списке предков
**не будет самого корня**, и файлы `CommonCommands/*.xml`, `Configuration.xml`
никогда не найдутся. Именно поэтому первая попытка (`fp-harness`) рисовала кнопки
как «Ссылка»: контекст не был передан.

Решение: путь формы = `/@cfg/<путь от корня>`, например

```
/@cfg/Documents/АвансовыйОтчет/Forms/ФормаДокумента/Ext/Form.xml
```

`/@cfg` — виртуальный корень. `js/qv8-io.js` срезает его и строит URL
`src + '/' + <путь>`. В C++ это просто «корень конфигурации».

### 6.3 Что возвращает `resolve()`

```js
{
  baseForm: '',            // для расширений; у нашей формы пусто
  objectMeta: '<XML объекта-владельца>',   // Documents/АвансовыйОтчет.xml
  interfaceMode: 'Any',    // из дескриптора формы (UseInInterfaceCompatibilityMode)
  configInterfaceMode: 'TaxiEnableVersion8_2', // из Configuration.xml
  refMeta: { 'Catalog.Валюты': '<XML>', ... }, // 15 объектов
  commonCommands: { 'ПоказатьДвиженияДокумента': '<XML>', ..., '@global:СозданиеСвязанныхОбъектов': '<XML>' },
  commonPictures: { 'Внимание16': {mime:'application/zip', data:'<base64 Picture.zip>'}, ... },
  styleItems: { 'ЦветБаннераБежевый': '#FCEB9F', ... }
}
```

Контекст для АО: 12 общих картинок, 5 стилей, 15 `refMeta`, 6 общих команд
(5 ссылочных + 1 глобальная), `objectMeta` 219 425 байт.

### 6.4 `parse()` и `render()`

`FormPreview.parse` (8 аргументов):

```js
FormPreview.parse(
  formXml,          // Ext/Form.xml
  objectMetaXml,    // XML объекта-владельца или ''
  styleItems,       // { name: cssColor }
  baseFormXml,      // форма-основание (расширения) или ''
  commonCommands,   // { key: XML } — ключи plain/@global:/@global-fqn:/@fqn:/@group:
  commonPictures,   // { name: {mime, data:base64} } + '@item:<item>/<file>'
  refMetaXml,       // { 'Kind.Name': XML }
  interfaceMode     // 'Any' | 'Version8_5' | 'TaxiEnableVersion8_5' | ...
) -> { model } | { error }
```

`FormPreview.render(model, container, options)`:

* `container` — обычный элемент; рендерер ставит классы `fp-root fp-light ...`,
  строит `.fp-form-title` + `.fp-body#fp-canvas`, вешает `_fpCtx` на контейнер.
* `options` (все опциональны): `windowTitle`, `onSelect`, `onOpen`, `onHelp`,
  `sortByName`, `probe`, `io`, `queryEditor`, `onPropertyEdit`, `onSchemaEdit`,
  `onObjectEdit`, `onOutlineChanged`, `filePath`. Для статического вида важны только
  `windowTitle` (заголовок окна) и, при необходимости, `onSelect`.
* После рендера нужен `FormPreview.reflow(container)` — второй проход
  горизонтальной раскладки (ширины/подписи/таблицы). В `model-app.js` он вызывается
  дважды: сразу и после макротаска.
* `FormPreview.dispose(container)` снимает обработчики; `FormPreview.resetViewState()`
  сбрасывает запомненную вкладку/свёрнутые группы при открытии другого документа.

### 6.5 Режим интерфейса

`interfaceMode` попадает в `model.interfaceMode`; `configInterfaceMode` — в
`model.configInterfaceMode`. Для этой конфигурации:

* `Configuration.xml`: `InterfaceCompatibilityMode = TaxiEnableVersion8_2`;
* дескриптор формы: `<UseInInterfaceCompatibilityMode>` отсутствует → `Any`.

Итог: Taxi-профиль (`fp-taxi`, `fp-profile-taxi83`). Для 8.5 (`TaxiEnableVersion8_5`)
рендерер включает `fp-interface-85 fp-clean85` и использует `form-preview-85.css`.

### 6.6 Какие модули BslEdit обязательны

| Модуль | XML-путь | Model-путь | Зачем |
|---|---|---|---|
| `xml-util.js` | **да** | **да** | `form-preview.js` в начале делает `var XU = root.XmlUtil` и использует `XU.terms`, `localizedFrom`, `firstChild`; без него рендер падает |
| `form-preview.js` | **да** | **да** | сам рендерер |
| `form-context.js` | **да** | нет | строит `objectMeta`/`commands`/`pictures` из XML-дампа |
| `providers.js` | да (мы используем) | нет | реестр провайдеров; можно вызывать `FormPreview.parse` напрямую |
| `viewer.css` | **да** | **да** | базовые `.fp-root` / Taxi-стили; `form-preview-85.css` — только 8.5 |
| `form-preview-85.css` | для 8.5 | для 8.5 | профиль clean85; для Taxi не нужен |
| `form-properties.js` | нет | нет | инспектор свойств в редакторе BslEdit, не рендер |
| `form-edit.js`, `form-diff.js`, `form-diff-view.js` | нет | нет | редактирование/сравнение форм |
| `doc-edits.js`, `proj-form-converter.js`, `proj-metadata-converter.js` | нет | нет | правка XML и проекты `.mdo`/`.form` |
| `platform-85-assets.js` | нет | нет | генерируемые 8.5-ассеты (в публичной сборке отсутствуют; есть fallback) |
| `form-item-properties.js` | нет | нет | генерируемый словарь свойств для редактора |
| `icons.svg` | нет | нет | спрайт для интерфейса BslEdit; рендерер рисует SVG-иконки инлайн |
| `std-pictures/` | для общих StdPicture | для общих StdPicture | PNG/SVG библиотеки платформы; для АО не запрашивается |
| `platform-*.png` | точечно | точечно | `platform-data-history.png` и иконки инспектора; для АО — 1 файл |

`form-preview.js` не требует jQuery, Monaco, Node.js и сети. Он использует
`DOMParser` только в `parse()` (в model-пути не вызывается), `DecompressionStream`
для `Picture.zip`, `btoa/atob`, `requestAnimationFrame`, `getComputedStyle`.

---

## 7. io-лог формы АО

### 7.1 Сводка (`io-log-ao.json`)

| Показатель | Значение |
|---|---:|
| Записей в логе (все обращения) | 1 322 |
| GET | 1 014 |
| HEAD | 308 |
| HTTP 200 | 983 |
| HTTP 404 | 339 |
| Уникальных путей | 1 280 |
| Уникальных GET-200 файлов | 940 |
| Суммарный объём GET-200 | 45 133 607 байт (43,0 МиБ) |

Дубликаты — нормальны: `exists` + `read`, повторные обращения к тем же файлам
(`cachedText` кэширует внутри страницы, но один и тот же путь может встретиться
в разных проходах).

### 7.2 Группы (`io-paths-ao.json`)

| Группа | Уникальных путей | GET | HEAD | 200 | 404 | Байт |
|---|---:|---:|---:|---:|---:|---:|
| `ConfigDumpInfo.xml` | 1 | 1 | 1 | 1 | 0 | 30 417 163 |
| `Documents/**` (объекты + формы) | 241 | 76 | 165 | 51 | 190 | 5 441 824 |
| `Configuration.xml` | 1 | 1 | 1 | 1 | 0 | 1 447 216 |
| `Catalogs/**` | 65 | 60 | 26 | 52 | 13 | 3 043 686 |
| `DataProcessors/**` | 92 | 92 | 0 | 92 | 0 | 1 117 220 |
| `CommonCommands/**` | 560 | 565 | 5 | 560 | 0 | 1 021 214 |
| `InformationRegisters/**` | 42 | 42 | 0 | 42 | 0 | 991 002 |
| `Reports/**` | 46 | 46 | 0 | 46 | 0 | 944 841 |
| `form layout` (`Ext/Form.xml`) | 1 | 1 | 0 | 1 | 0 | 235 581 |
| `CommonPictures/**` | 24 | 24 | 0 | 24 | 0 | 176 422 |
| `DefinedTypes/**` | 20 | 20 | 2 | 20 | 0 | 125 576 |
| `CommandGroups/**` | 39 | 39 | 0 | 39 | 0 | 54 297 |
| `Tasks/**` | 1 | 1 | 0 | 1 | 0 | 49 734 |
| `ChartsOfAccounts/**` | 1 | 1 | 0 | 1 | 0 | 42 044 |
| `AccumulationRegisters/**` | 1 | 1 | 0 | 1 | 0 | 14 666 |
| `StyleItems/**` | 16 | 7 | 16 | 7 | 9 | 9 586 |
| `Forms/**` (дескрипторы, пробы) | 128 | 37 | 91 | 1 | 127 | 1 535 |
| `Ext/Help.xml` | 1 | 0 | 1 | 1 | 0 | 0 |

Самые большие файлы:

```
30 417 163  /@cfg/ConfigDumpInfo.xml
 1 447 216  /@cfg/Configuration.xml
   962 875  /@cfg/Documents/ВводНачальныхОстатков.xml
   322 018  /@cfg/Documents/ПоступлениеТоваровУслуг.xml
   253 683  /@cfg/Catalogs/Организации.xml
   235 581  /@cfg/Documents/АвансовыйОтчет/Forms/ФормаДокумента/Ext/Form.xml
   233 328  /@cfg/Documents/РеестрСведенийНеобходимыхДляНазначенияИВыплатыПособий.xml
   227 133  /@cfg/Documents/КорректировкаПоступления.xml
   219 425  /@cfg/Documents/АвансовыйОтчет.xml
   186 810  /@cfg/Documents/СчетФактураПолученный.xml
```

### 7.3 Почему так много

Основной объём — **построение каталога команд формы** (`buildCatalog` в `form-context.js`):
`ConfigDumpInfo.xml` (30 МБ) даёт индекс всех `CommonCommand`, `CommandGroup` и
`<Kind>.<Name>.Command.*`; затем читаются **все** 560 файлов `CommonCommands/*.xml`,
39 `CommandGroups/*.xml` и ~170 файлов объектов, у которых есть команды. Из них
отбираются только команды с группой `FormCommandBar*`, применимые к типу владельца.

Это нужно, потому что в форме `AutoCommandBar Autofill=false`, но есть
`ButtonGroup CommandSource=Form` — платформа (и BslEdit) добавляет в неё применимые
глобальные команды. Для АО это даёт, в частности, `СозданиеСвязанныхОбъектов`
в меню «Создать на основании».

### 7.4 404 — это нормально

339 ответов 404 — это пробы `exists` по путям-кандидатам:

* `ConfigDumpInfo.xml` / `Configuration.xml` во всех предках формы (пока не найден корень);
* `CommonCommands/<name>.xml` во всех предках (пока не найден корневой);
* `Documents/<Name>/<Name>.xml` (в дампе объект лежит рядом с папкой, а не внутри);
* `StyleItems/<name>/<name>.mdo`, `DefinedTypes/...` и т.п.

C++ должен отвечать на такие пути 404 (или 403), а не 200 с заглушкой.

### 7.5 Минимальный набор файлов конфигурации для этой формы

Если XML-путь всё-таки используется, без этих данных вид деградирует:

**Обязательно (иначе другой вид):**

| Что | Файлы | Эффект отсутствия |
|---|---|---|
| Сама форма | `Documents/АвансовыйОтчет/Forms/ФормаДокумента/Ext/Form.xml` | нет формы |
| Дескриптор формы | `.../Forms/ФормаДокумента.xml` | теряется `UseInInterfaceCompatibilityMode` |
| Объект-владелец | `Documents/АвансовыйОтчет.xml` | нет подписей/типов/ширин полей, нет стандартных команд |
| Индекс конфигурации | `ConfigDumpInfo.xml` | не строится каталог общих команд |
| Свойства конфигурации | `Configuration.xml` | неизвестен `InterfaceCompatibilityMode` (для Taxi не критично) |
| Общие команды | `CommonCommands/<5 имён>.xml` | кнопки без картинок/подписей |
| Картинки | `CommonPictures/<12 имён>/Ext/Picture.xml` + `Picture/Picture.zip` | нет иконок |
| Стили | `StyleItems/<5 имён>.xml` | другие цвета фона/текста |
| `refMeta` | 15 XML объектов (`Catalogs/*.xml`, `Documents/*.xml`, `DefinedTypes/*.xml`) | для этой формы не влияет на пиксели, но нужно спискам/типам |

**Можно не отдавать (для этой формы):**

* `Ext/Help.xml` — проверяется только `HEAD`, содержимое не читается; без него `hasHelp=false`,
  и кнопка «?» может уйти в «Ещё».
* `CommonForms`, `Languages`, `Subsystems`, `Roles`, `Ext/CommandInterface.xml` —
  в этом рендере не читаются.
* `Ext/Form/Items/*` — у АО нет собственных картинок элементов.
* `Picture.zip`-файлы, если картинки не нужны (но тогда иконки будут другими).

**Нельзя «угадывать» данные:** если файла нет, `readBytes` возвращает `null`,
резолвер молча пропускает блок. Лучше отдать 404, чем пустой XML.

Для АО фактически запрошены (успешно):

* общие команды (5 + 1 глобальная): `ПоказатьДвиженияДокумента`,
  `СвязанныеДокументы`, `ПрисоединенныеФайлыБП`,
  `ДополнительныеСведенияКоманднаяПанель`, `ИсторияИзменений`,
  `@global:СозданиеСвязанныхОбъектов`;
* общие картинки (12): `ЗаполнитьФорму`, `Отчеты`, `СостоянияДокумента`,
  `Внимание16`, `НекорректныйКонтрагент`, `ЗагрузитьЧекиПоQRКоду`,
  `СкопироватьВБуферОбмена`, `ВставитьИзБуфераОбмена`, `ЖурналПроводок`,
  `СтруктураПодчиненности`, `Скрепка`, `ДополнительныеСведения`;
* элементы стиля (5): `БыстрыеОтборыФонГруппы`, `ЗаголовкиСтрокЦветТекста`,
  `ДосьеГиперссылкаЦвет`, `ИтогиФонГруппы`, `ЦветБаннераБежевый`;
* `refMeta` (15): `DefinedType.ОписаниеПокупки`, `DefinedType.НомерДокументаОплаты`,
  `Валюты`, `Контрагенты`, `Организации`, `Склады`, `ТипыЦенНоменклатуры`,
  `ФизическиеЛица`, `Пользователи`, `ПодразделенияОрганизаций`, `Номенклатура`,
  `НомераГТД`, `СтраныМира`, `ДоговорыКонтрагентов`, `Билеты`.

`std-pictures/` при рендере АО **не запрашивается** (все иконки пришли из
`commonPictures`/inline SVG), но для других форм библиотека нужна.

### 7.6 Рекомендации C++-хосту

1. **Не отдавайте XML-путь, если C++ строит модель сама.** Это снимает 45 МБ,
   1300 запросов и 30-МБ `ConfigDumpInfo.xml`.
2. Если XML-путь нужен (отладка/сверка), реализуйте:
   * `HEAD` (или `GET` с `Range: bytes=0-0`) — для `exists`;
   * мэппинг путь → поток из `.cf`;
   * `statMany` + `cacheGet/cachePut`, чтобы закэшировать каталог команд между
     запусками (иначе каждый старт — ~1700 чтений). Ключ кэша —
     `${configRootDir}/command-catalog:v3`, значение — JSON из `buildCatalog()`;
     зависимости (`deps`) — список путей + штампы `size:mtime`.
   * `existsMany`/`readMany` можно не реализовывать — резолвер свалится на
     одиночные `exists/readBytes` с конкурентностью 16.
3. Если отдаёте модель — отдавайте её из памяти и инжектируйте (4.5),
   это быстрее и не требует даже `model.json`.
4. Кэшируйте `objectMeta`/`refMeta`/`commands`/`commonCommands` по объекту:
   при переходе между формами одного объекта они не меняются.

### 7.7 Что ещё нужно для полного вида (кроме самой формы)

| Данные | Где лежат | Зачем рендереру | Что будет без них |
|---|---|---|---|
| **Каталог команд** (`CommonCommands`, `CommandGroups`, команды объектов) | `ConfigDumpInfo.xml` + `CommonCommands/*.xml` + `CommandGroups/*.xml` + `<Kind>/<Object>.xml` | строит автоматические команды командной панели (общие, глобальные, объектные) | панель теряет часть команд; у АО — «Создать на основании» и глобальные команды |
| **Общие команды** | `CommonCommands/<Name>.xml` | заголовок, картинка, представление, группа | кнопки-иконки без картинок/подписей |
| **Общие картинки** | `CommonPictures/<Name>/Ext/Picture.xml` + `Picture/Picture.zip` | иконки кнопок/декораций | другие иконки/заглушки |
| **Элементы стиля** | `StyleItems/<Name>.xml` | цвета фона/текста, шрифты | другие цвета (баннер, итоги) |
| **Языки** | `Languages/*.xml` | **не нужны**: локализация уже разрешена в `Form.xml` (`v8:item` для `ru`) | ничего |
| **Подсистемы, роли, CommandInterface конфигурации** | `Subsystems/`, `Roles/`, `Ext/CommandInterface.xml` | **не нужны** для формы | ничего |
| **Справка** | `<Object>/Ext/Help.xml` | только факт наличия (`hasHelp`); содержимое не читается | «?» может уйти в «Ещё» |
| **Макеты** | `Templates/`, `CommonTemplates/` | не нужны для формы (только если рендерить макет) | ничего |

C++ для model-пути должен отдать первые четыре строки (или их эквивалент в модели);
языки/подсистемы/роли/макеты можно не отдавать вообще.

---

## 8. Модель формы (главный раздел)

### 8.1 Точка входа и жизненный цикл

```js
FormPreview.render(model, container, options)
```

* `model` — обычный JSON-объект (никаких DOM-узлов, функций, Map/Set).
* `container` — DOM-элемент; лучше дать ему `height:100vh` (как `#host` в бандле).
* `options` — см. 6.4.
* `render` **синхронный**: строит DOM, ставит `container._fpCtx`.
  Асинхронно дорисовываются только `Picture.zip`-иконки (через `DecompressionStream`).
* После `render` вызовите `FormPreview.reflow(container)`; при изменении размера —
  ещё раз.
* `FormPreview.dispose(container)` — снять обработчики.
* `FormPreview.resetViewState()` — сбросить активную вкладку/свёрнутые группы.
  Активная вкладка хранится в модульной переменной `activePageIdByPagesKey`
  по ключу `itemKey(pages)`; между разными формами её нужно сбрасывать.

### 8.2 Верхний уровень модели

| Поле | Тип | Обяз. | Что делает |
|---|---|---|---|
| `tag` | `"Form"` | да | корень |
| `name` | string | нет | имя формы |
| `properties` | object | да | свойства формы (см. 8.6) |
| `childItemsRoot` | Item[] | да | элементы формы по порядку |
| `autoCommandBar` | Item | нет | командная панель формы; без неё панели нет |
| `attributes` | Attr[] | да* | реквизиты формы; от них зависят подписи/типы DataPath |
| `baseAttributes` | Attr[] | нет | унаследованные (расширения) |
| `commands` | Command[] | да* | команды формы (`Form.Command.*`) |
| `excludedCommands` | string[] | нет | исключённые команды |
| `commandInterface` | object[] | нет | разобранный `CommandInterface/CommandBar` |
| `commonCommands` | map | да* | общие команды; **нужны для иконок/подписей кнопок** |
| `commandGroups` | map | нет | группы общих команд |
| `commonPictures` | map | да* | картинки конфигурации `{mime,data}`; без них другие иконки |
| `styleItems` | map | да* | `{имя: css-цвет}`; без них другие цвета |
| `objectMeta` | object | да* | разобранные метаданные владельца (8.11) |
| `refMeta` | map | нет | `{'Kind.Name': objectMeta}` (8.13) |
| `version` | string | нет | версия формата (`"2.20"`); для 8.5 нужна |
| `interfaceMode` | string | нет | `'Any'`, `'Version8_5'`, `'TaxiEnableVersion8_5'`… |
| `configInterfaceMode` | string | нет | режим совместимости конфигурации |
| `adoptedForm` | bool | нет | форма расширения |
| `events` | Event[] | нет | обработчики формы (на вид не влияют) |
| `structuredProperties` | object | нет | сложные значения формы |

`*` — «обязательно для точного вида»; для простой формы без команд/картинок можно
меньше (8.19).

### 8.3 Оболочка элемента (Item)

```json
{
  "tag": "InputField",
  "name": "Организация",
  "id": "13",
  "properties": { "DataPath": "Объект.Организация", "EditMode": "EnterOnInput" },
  "authoredProperties": { ... },          // опционально, для инспектора
  "childItems": [],
  "events": [],
  "autoCommandBar": { ... },             // у Table
  "searchStringAddition": { ... },       // у Table
  "viewStatusAddition": { ... },
  "searchControlAddition": { ... },
  "extendedTooltip": { ... },            // рисует «?»/подсказку
  "contextMenu": { ... },
  "structuredProperties": { ... },       // сложные XML-значения
  "complexProperties": { ... },          // устаревший алиас для InputField
  "runtime": { ... }                     // производные данные (8.17)
}
```

* `tag` — тип элемента (8.5).
* `name` — имя; используется как fallback-ключ и подпись.
* `id` — идентификатор для выбора/подсветки. Если не нужен интерактив, можно опустить
  (проверено: без `id` картинка не меняется). `itemKey(it) = id || name`.
* `childItems` — дети (в порядке отображения). Для `Table` здесь колонки;
  `autoCommandBar` таблицы лежит отдельным полем.
* `properties` — **главный** блок. Ключи нормализуются: регистр и `_ - пробелы`
  игнорируются, поддерживаются русские имена (`Ширина` = `Width`). `prop()` берёт
  первое непустое значение из списка алиасов.
* `authoredProperties` — исходные authored-значения; для рендера **не нужны**
  (проверено: удаление даёт 0 отличий). Нужны только инспектору свойств.
* `runtime` — производные поля; **нужны** для точных кнопок полей/ширин.
* `Visible=false` — элемент не рисуется (и не занимает место).

### 8.4 Соглашения о значениях

| Что | Как записано |
|---|---|
| boolean | `"true"` / `"false"` |
| enum | строка как в 1С: `"Left"`, `"Right"`, `"Top"`, `"TabsOnTop"`, `"PictureAndText"`, `"AlwaysHorizontal"`… |
| локальная подпись | `properties.Title` — уже **плоская строка** (`"Записать"`), не XML |
| цвет | `#RRGGBB`, `web:Gainsboro`, `style:ИмяСтиля`; `styleItems` разворачивает `style:` в CSS-значение |
| картинка | `StdPicture.Write`, `CommonPicture.Отчеты`, `Abs:Picture.png`, `@item:<item>/<file>` |
| шрифт | `FontSpec: {kind,ref,faceName,height,scale,bold,italic,underline,strikeout}` |
| ширина/высота | число в **символах** (1 символ ≈ 8 px в Taxi; у таблиц колонка ≈ 10 px) |
| форматированная строка | `TitleFormatted: "true"`; сам текст уже без тегов |
| сложное значение | `structuredProperties: { Tag: {name, attributes, children[], text} }` |

`TitleLeadingSpaces` — неавторское несериализуемое свойство (ведущие пробелы
плоской подписи); для модели не нужно.

**Булевы значения**: пишите строки `"true"`/`"false"` (как в XCF). JSON-булевы
`true`/`false` тоже работают, потому что `prop()` делает `String(v)`, но строки —
безопаснее и совпадают с тем, что отдаёт `parse()`.

**Перечисления** (сравнение регистронезависимое, `_`/`-`/пробелы игнорируются):

| Поле | Допустимые значения |
|---|---|
| `TitleLocation` | `Auto`, `Left`, `Top`, `Right`, `Bottom`, `None` |
| `Representation` (контейнер) | `Auto`, `None`, `Weak`, `Strong`, `Normal` |
| `Representation` (кнопка) | `Auto`, `Text`, `Picture`, `PictureAndText` |
| `Group` | `Vertical`, `Horizontal`, `AlwaysHorizontal`, `AlwaysVertical` |
| `Behavior` | `Usual`, `Collapsible`, `Popup` |
| `PagesRepresentation` | `TabsOnTop`, `TabsOnBottom`, `None`, `LeftHorizontal` |
| `CommandBarLocation` | `Auto`, `Top`, `Bottom`, `None` |
| `LocationInCommandBar` | `Auto`, `InCommandBar`, `InCommandBarAndInAdditionalSubmenu`, `InAdditionalSubmenu` |
| `FixingInTable` | `Left`, `Right`, `None` |
| `HeaderHorizontalAlign` / `FooterHorizontalAlign` | `Auto`, `Left`, `Center`, `Right` |
| `Type` (кнопка) | `CommandBarButton`, `UsualButton`, `Hyperlink` |
| `CheckBoxType` | `CheckBox`, `Tumbler`, `Switcher` |
| `ChoiceButtonRepresentation` | `ShowInInputField`, `ShowInDropList`, `ShowInDropListAndInInputField` |
| `EditMode` | `EnterOnInput`, `ImmediatelyAfterInput`, … |

`Never` для `LocationInCommandBar` рендерер не понимает (это значение не
встречается в XCF; чтобы скрыть кнопку — `Visible=false`, чтобы убрать всю
панель — `CommandBarLocation=None`).

### 8.5 Типы элементов

| `tag` | Роль | Ключевые свойства | Особенности отрисовки |
|---|---|---|---|
| `Form` | корень | `Title`, `AutoTitle`, `ShowTitle`, `Width`, `VerticalScroll`, `CommandBarLocation`, `Group` | задаёт окно, заголовок, ширину, скролл, панель |
| `UsualGroup` | группа | `Group`, `Behavior`, `Representation`, `ShowTitle`, `United`, `ThroughAlign`, `ChildItemsWidth`, `BackColor` | вертикальная/горизонтальная, сворачиваемая, попап |
| `Pages` | страницы | `PagesRepresentation` (`TabsOnTop`/`TabsOnBottom`/`None`/`LeftHorizontal`) | вкладки; активна первая видимая |
| `Page` | страница | `Title`, `TitleDataPath`, `Visible` | вкладка + содержимое |
| `Table` | таблица | `Representation`, `DataPath`, `AutoInsertNewRow`, `HeaderHeight`, `FooterHeight`, `HorizontalLines`, `VerticalLines` | шапка/тело/подвал, тулбар, пустая таблица |
| `ColumnGroup` | группа колонок | `Title`, `Group`, `Width`, `ShowInHeader`, `FixingInTable` | многоуровневая шапка |
| `InputField` | поле ввода | `DataPath`, `Title`, `TitleLocation`, `Width`, `MultiLine`, `EditMode`, `ReadOnly`, `ChoiceButton*`, `Format`, `HeaderHorizontalAlign`, `ShowInFooter`, `FooterText` | подпись + редактор + кнопки выбора |
| `LabelField` | поле-метка | `DataPath`, `Title`, `Hiperlink`, `BackColor` | подпись + пустое место значения |
| `CheckBoxField` | флажок | `CheckBoxType`, `EditMode`, `Title` | флажок/тумблер/переключатель |
| `RadioButton` / `RadioButtonField` | переключатель | `RadioButtonType`, `ColumnsCount`, `ChoiceList` | список/сегменты |
| `Button` | кнопка | `Type`, `CommandName`, `Title`, `Picture`, `Representation`, `DefaultButton`, `LocationInCommandBar` | текст/иконка/жёлтая по умолчанию |
| `ButtonGroup` | группа кнопок | `CommandSource`, `Representation`, `Title` | контейнер в панели |
| `Popup` | подменю | `Title`, `Picture`, `Representation` | кнопка + меню |
| `AutoCommandBar` / `CommandBar` | командная панель | `Autofill`, `CommandSource`, `Location`, `ChildItems` | см. 8.8 |
| `LabelDecoration` | надпись | `Title`, `AutoMaxWidth`, `Width`, `BackColor` | текст, баннеры |
| `PictureDecoration` | картинка | `Picture`, `Width`, `Height`, `Hyperlink`, `GroupHorizontalAlign` | иконка/крестик |
| `PictureField` | поле-картинка | `DataPath`, `ValuesPicture`, `Width`, `Height` | состояние документа |
| `SearchStringAddition` | строка поиска | `Width` | плейсхолдер «Поиск» |
| `ViewStatusAddition` | чипы | — | демо-чипы в превью |
| `SearchControlAddition` | управление поиском | — | подпись в превью |
| `ListBox` / `ListField` | список | `DataPath`, `Title` | мок-список |
| `Hyperlink` | гиперссылка | `Title`, `DataPath` | текст-ссылка |
| `SpreadSheetDocumentField` | поле табличного документа | `VerticalScrollBar`, `HorizontalScrollBar` | пустая сетка |
| `FormattedDocumentField`, `ChartField`, `GanttChartField`, `CalendarField`, `PeriodField`, `ProgressBarField`, `TrackBarField`, `TextDocumentField`, `HTMLDocumentField`, `GeographicsField`, `PDFDocumentField` | редкие поля | — | упрощённые заглушки |

### 8.6 Основные свойства (влияют на вид)

**Общие для всех элементов:**

| Свойство | Тип | По умолчанию | Влияние |
|---|---|---|---|
| `Title` | string | `''` | подпись; для полей — слева/сверху, для групп — заголовок, для кнопок — текст |
| `TitleLocation` | enum | Auto | `Left`/`Top`/`Right`/`Bottom`/`None`; Auto = Left, у таблиц/многострочных = Top |
| `Visible` | bool | true | скрыть элемент и его место |
| `Enabled` | bool | true | серый/недоступный вид |
| `ReadOnly` | bool | false | серый фон редактора |
| `Width` | int (символы) | auto | ширина; у групп — 10 px/символ, у полей — 8 px/символ |
| `Height` | int (строки) | auto | высота; у полей — многострочность |
| `AutoMaxWidth` | bool | true | разрешить растягивание по ширине |
| `MaxWidth` | int | — | ограничение ширины в символах |
| `HorizontalStretch` | bool | auto | растягивать по ширине |
| `VerticalStretch` | bool | auto | растягивать по высоте |
| `GroupHorizontalAlign` | enum | Left | выравнивание элемента в группе (`Right` прижимает «Ещё»/«?») |
| `GroupVerticalAlign` | enum | Top | по вертикали |
| `HorizontalAlign` | enum | — | выравнивание содержимого (таблицы/группы) |
| `VerticalAlign` | enum | — | по вертикали |
| `BackColor` | цвет | — | фон; `style:` разворачивается |
| `TextColor` | цвет | — | цвет текста |
| `FontSpec` | FontSpec | — | шрифт |
| `ToolTip` | string | — | всплывающая подсказка |
| `ToolTipRepresentation` | enum | Auto | `Button` рисует «?», `Top`/`Bottom` — серый текст |
| `Hyperlink` | bool | false | ссылочный вид |
| `MarkRequiredComplete` | bool | — | звёздочка обязательного поля (8.5/в некоторых режимах) |

**Контейнеры (`UsualGroup`, `Pages`, `Page`, `Form`):**

| Свойство | Значения | Влияние |
|---|---|---|
| `Group` | `Vertical`, `Horizontal`, `AlwaysHorizontal`, `AlwaysVertical` | направление раскладки |
| `Behavior` | `Usual`, `Collapsible`, `Popup` | обычная/сворачиваемая/попап |
| `Representation` | `None`, `Weak`, `Strong`, `Normal`, … | рамка/фон группы |
| `ShowTitle` | bool | показывать заголовок группы |
| `United` | bool | объединять рамки детей |
| `ThroughAlign` | enum | сквозное выравнивание колонок |
| `ChildItemsWidth` | `Equal`, `LeftWidest`, `LeftWide`, `LeftNarrowest`, … | распределение ширин детей |
| `HorizontalSpacing` / `VerticalSpacing` | `None`, `Half`, `Single`, `OneAndHalf`, `Double` | интервалы |
| `PagesRepresentation` | `TabsOnTop`, `TabsOnBottom`, `None`, `LeftHorizontal` | вид страниц |
| `CommandBarLocation` | `Auto`, `Top`, `Bottom`, `None` | положение панели |
| `ShowCommandBar` | bool | жёстко скрыть панель |
| `Autofill` (у AutoCommandBar) | bool | автозаполнение панели |
| `Width` (у Form) | int | максимальная ширина окна, если 1..200 символов |
| `VerticalScroll` | `Auto`, `UseIfNecessary`, `UseWithoutStretch` | поведение прокрутки |

**Поля (`InputField`, `LabelField`, `CheckBoxField`, `PictureField`):**

| Свойство | Влияние |
|---|---|
| `DataPath` | путь к данным; по нему берутся подпись/тип/ширина из `objectMeta`/`attributes` |
| `Title` | явная подпись (иначе из `DataPath`) |
| `EditMode` | `EnterOnInput`, `ImmediatelyAfterInput` и т.п. (влияет на вид кнопок) |
| `MultiLine` | многострочный редактор |
| `ReadOnly` | серый фон |
| `ChoiceButton`, `DropListButton`, `OpenButton`, `ClearButton`, `SpinButton`, `ChoiceListButton` | кнопки поля; по умолчанию выводятся из типа |
| `ChoiceButtonRepresentation` | `ShowInInputField`, `ShowInDropList`, … |
| `ChoiceButtonPicture` | картинка кнопки выбора |
| `Format`, `EditFormat` | формат (в превью не заполняет значение) |
| `HeaderHorizontalAlign`, `FooterHorizontalAlign` | выравнивание в шапке/подвале таблицы |
| `ShowInHeader`, `ShowInFooter` | участие колонки в шапке/подвале |
| `FooterText` | текст подвала (итог) |
| `FixingInTable` | `Left`/`Right` — закрепление колонки |
| `Width` | ширина в символах |
| `AutoCellHeight` | **не поддерживается** (есть только в словаре свойств; на вид не влияет) |

**Кнопки:**

| Свойство | Влияние |
|---|---|
| `Type` | `CommandBarButton` (панель), `UsualButton`, `Hyperlink`, … |
| `CommandName` | `Form.Command.X`, `Form.StandardCommand.Write`, `CommonCommand.X`, `Document.X.StandardCommand.Post`, … |
| `Title` | текст кнопки |
| `Picture` | `StdPicture.Write`, `CommonPicture.X`, … |
| `Representation` | `Auto`, `Text`, `Picture`, `PictureAndText` |
| `DefaultButton` | `true` — жёлтая кнопка по умолчанию |
| `LocationInCommandBar` | `InCommandBar`, `InAdditionalSubmenu` («Ещё») |
| `ShapeRepresentation` | `None` — плоская кнопка |
| `GroupHorizontalAlign` | `Right` — прижать вправо |

### 8.7 Раскладка, ширины и подписи

**Быстрая карта «что за что отвечает»:**

| Что нужно | Поля модели |
|---|---|
| Заголовок окна | `options.windowTitle` → иначе `properties.Title`; `AutoTitle`, `ShowTitle`, `TitleFormatted`, `Form.Width` |
| Командная панель | `autoCommandBar` (дерево `Button`/`ButtonGroup`/`Popup`), `properties.CommandBarLocation`, `ShowCommandBar`, `Autofill` |
| Шапка таблицы | `Table.childItems` (колонки/`ColumnGroup`), `Title`, `ShowInHeader`, `HeaderHeight`, `HeaderHorizontalAlign` |
| Панель итогов | у `Table`: `ShowInFooter`, `FooterText`, `FooterHorizontalAlign`, `FooterHeight`; в АО — обычная `UsualGroup` с `BackColor: style:ИтогиФонГруппы` и полями |
| Выравнивание подписей | `TitleLocation` (`Left`/`Top`/`Right`/`Bottom`/`None`/Auto); CSS: текст подписи слева; `GroupHorizontalAlign` — положение самого элемента |
| Ширины | `Width` (символы), `AutoMaxWidth`, `MaxWidth`, `HorizontalStretch`, `ChildItemsWidth`, `runtime.defaultChars` (из типа) |
| Высоты | `Height` (строки), `VerticalStretch`, `HeaderHeight`/`FooterHeight` (строки таблицы) |
| Цвета/шрифты | `BackColor`, `TextColor`, `FontSpec`, `styleItems` |

**Порядок разрешения подписи** (`titleOf`/`displayLabel`):

1. явный `properties.Title` (русский `Заголовок`) элемента;
2. для кнопки/команды — `commandMeta`: `properties.Title` из
   `model.commonCommands[...]` / `model.commands[...]` / `commandInterface`;
3. для поля/колонки — `captionForPath(DataPath, captionIndex)`;
   `captionIndex` собран из:
   * `model.attributes` (реквизиты формы; `Title` по имени/пути, колонки ТЧ);
   * `model.objectMeta.captions` + `stdCaptions` (реквизиты объекта и колонки ТЧ,
     с префиксом `Объект.`/`Запись.`);
   * `model.refMeta` — для полей динамических списков (по `QueryText`/`MainTable`);
   * `DefinedType.*` — разворачивание через `refMeta`;
4. `lastSeg(DataPath)` / `humanizeIdent(name)` — технический fallback.

Поэтому если C++ не заполнил `objectMeta.captions`/`stdCaptions`, поля покажут
`ФизЛицо:` вместо `Подотчетное лицо:` (см. 8.11.1), даже если `Title` у поля нет
специально — так и должно быть в 1С.

* **Символ = 8–10 px**: у полей базовая сетка 8 px (`CHAR_PX = 8`), в authored-формулах
  ширины часто используется 10 px (`REF_AUTHORED_CHAR_PX = 10`); у групп `Width` —
  10 px/символ, `Height` — 16 px/строка. У таблиц колонка — ~10 px/символ.
* Если `Width` не задан, ширина поля вычисляется из `runtime.defaultChars`
  (он, в свою очередь, из `objectMeta.types/stringLen`), с минимумом 80 px
  и кнопками поля (+21 px на кнопку).
* `AutoMaxWidth=false` + `MaxWidth` — редактор не растягивается сверх `MaxWidth`.
* `HorizontalStretch` — поле занимает остаток строки.
* `ChildItemsWidth` (`Equal`, `LeftWidest`, …) распределяет ширины в горизонтальной группе.
* **Подписи**: `TitleLocation=Auto` → слева, кроме `Table`, многострочного поля и
  `RadioButtonField` с `ColumnsCount=1` (там сверху). Явные `Left/Top/Right/Bottom/None`
  работают как в 1С. Подпись слева — отдельная колонка, текст выровнен **по левому краю**
  (CSS `.fp-field-label{text-align:left}`), редактор справа. Свойства «выровнять текст
  подписи по правому краю» в рендерере нет; `TitleLocation=Right` переносит подпись
  за редактор, а не меняет выравнивание.
* После первичной раскладки `reflow()` делает несколько проходов: переносит слишком
  длинные auto-подписи наверх, подгоняет ширины таблиц, выравнивает command bar.
* Высота контейнера: `.fp-root { height:100% }`; хост должен задать высоту
  (`#host {height:100vh}`), иначе форма схлопнется.

### 8.8 Командная панель

Модель уже содержит **скомпилированную** панель: `parse()` добавил стандартные
команды (`_stdWrite`, `_stdPost`, `_stdPostAndClose`, `_stdHelp`,
`_global_create_based_on`) и общие команды (`_global_<Name>`), если это нужно.

```json
"autoCommandBar": {
  "tag": "AutoCommandBar", "name": "ФормаКоманднаяПанель", "id": "-1",
  "properties": { "Autofill": "false" },
  "childItems": [
    { "tag": "ButtonGroup", "name": "ГруппаКнопкиКоманднойПанели",
      "properties": { "CommandSource": "Form" },
      "childItems": [
        { "tag": "Button", "name": "_stdWrite",
          "properties": { "Title": "Записать",
                          "CommandName": "Form.StandardCommand.Write",
                          "Representation": "Text" } },
        { "tag": "Button", "name": "ОбщаяКомандаСтруктураПодчинённости",
          "properties": { "CommandName": "CommonCommand.СвязанныеДокументы" } }
      ] }
  ]
}
```

* `Button.properties.Title` — текст. Если его нет, рендерер попробует
  `CommandName` → `model.commands`/`model.commonCommands` → стандартный словарь.
  Поэтому **`commonCommands` обязателен** для иконок/подписей общих команд
  (без него — 8 098 px отличий на АО).
* `Button.properties.Picture` — картинка; для `StdPicture.*` ищется файл
  `std-pictures/<транслит>.png|svg`, для `CommonPicture.*` — `model.commonPictures`.
* `Representation=Auto` разрешается по наличию картинки и типу кнопки:
  иконка без текста, текст, или иконка+текст.
* `DefaultButton=true` — жёлтая кнопка (`.fp-primary-write`/`fp-postandclose`).
* `LocationInCommandBar=InAdditionalSubmenu` — кнопка уезжает в «Ещё».
* `GroupHorizontalAlign=Right` — «Ещё»/«?» прижимаются вправо.
* «Ещё» добавляется автоматически, если в панели есть скрытые/дополнительные кнопки;
  «?» — если `_stdHelp` не исключён и есть справка.

**Важное расхождение BslEdit с рантаймом 1С:** при `AutoCommandBar Autofill=false`
BslEdit вставляет только `_stdWrite`; `Провести и закрыть`/`Провести` не появляются
(см. `formStdCommandButtons()` — при `Autofill=false` он возвращает `[]`).
`model-ao-posting.json` показывает, как хост может добавить их сам:
в `childItems` группы вставляются `_stdPostAndClose` (`DefaultButton=true`) и
`_stdPost`; рендерер рисует их правильно (скриншот `shots/form-ao-model-posting.png`).
Если C++ строит модель сам и хочет вид «как в 1С», эти кнопки нужно добавить
(стандартные команды `Form.StandardCommand.PostAndClose`/`Post`).

### 8.9 Таблицы, шапка, итоги

`Table.childItems` — **колонки в порядке отображения**. Колонка может быть
`ColumnGroup` (объединяет вложенные колонки под общим заголовком) или листовым
`InputField`/`LabelField`/`PictureField`.

Пример (АО, «Товары»):

```json
{ "tag": "Table", "name": "Товары",
  "properties": { "Representation": "List", "DataPath": "Объект.Товары",
                  "AutoInsertNewRow": "true" },
  "childItems": [
    { "tag": "LabelField", "name": "ТоварыНомерСтроки",
      "properties": { "DataPath": "Объект.Товары.LineNumber",
                      "FixingInTable": "Left", "Width": "3" } },
    { "tag": "ColumnGroup", "name": "ГруппаТоварыНоменклатура",
      "properties": { "Title": "Номенклатура и сописание" },
      "childItems": [
        { "tag": "InputField", "name": "ТоварыНоменклатура",
          "properties": { "DataPath": "Объект.Товары.Номенклатура", "Width": "15" } },
        { "tag": "InputField", "name": "ТоварыОписание",
          "properties": { "DataPath": "Объект.Товары.Описание", "Width": "25",
                          "FontSpec": { "kind": "StyleItem", "ref": "style:СодержаниеУслугиШрифт" } } }
      ] }
  ],
  "autoCommandBar": { ... } }
```

* Шапка строится из `Title` колонок/групп; `ShowInHeader=false` убирает колонку
  из шапки; `HeaderHeight` (в строках) увеличивает высоту; `HeaderHorizontalAlign`
  выравнивает текст.
* Ширина колонки — `Width` в символах (≈10 px/символ); без `Width` — по содержимому.
  `FixingInTable=Left/Right` — закреплённые колонки.
* `ColumnGroup.Group=Horizontal` — подколонки в ряд; без него — друг под другом.
* Подвал: `ShowInFooter=true` и/или `FooterText`; `FooterHorizontalAlign`.
  В АО подвал таблицы не используется — «Итоги» сделаны **обычной группой**
  (`UsualGroup` с `BackColor: style:ИтогиФонГруппы` и полями), а над ней баннер
  «Убедитесь, что билеты оплачены» (`LabelDecoration` + `PictureDecoration`).
* `AutoInsertNewRow=true` рисует пустую строку ввода; `Representation=List`
  (в превью также Tree/иерархический список).
* `AutoCellHeight` не поддерживается; `RowHeight`/`HeaderHeight`/`FooterHeight`
  читаются для метрик.
* Таблица без `autoCommandBar` не имеет тулбара; `CommandBarLocation=None` —
  тоже. Внешний `CommandBar` перед таблицей «прилипает» к ней (`.fp-table-source-bar`).

### 8.10 Заголовок окна

`render()`:

```js
var formTitle = options.windowTitle || rawTitle(model); // rawTitle = model.properties.Title
if (options.windowTitle || (formTitle && (!isFalse(model.properties.ShowTitle)
    || isFalse(model.properties.AutoTitle)))) { ... }
```

* `options.windowTitle` — сильный приоритет; именно так `app.js` подставляет
  синоним объекта (`objectMeta.synonym`), потому что у формы `AutoTitle=false`
  и своего `Title` нет.
* Если `options.windowTitle` пуст, берётся `model.properties.Title`.
* `ShowTitle=false` скрывает заголовок, если только не задан `options.windowTitle`.
* `AutoTitle=false` разрешает показ `Title` даже при пустом `ShowTitle`.
* Полоска заголовка (`.fp-form-title`) есть всегда (23 px), даже без текста.
* `Form.Width` (1..200 символов) ограничивает ширину формы.
* `VerticalScroll` (`UseWithoutStretch`/`UseIfNecessary`) включает вертикальную прокрутку.

Для АО в `model-ao*.json` добавлено `properties.Title = "Авансовый отчет"`;
`model-app.js` может дополнительно передать `?title=`.

### 8.11 `objectMeta` — метаданные владельца

Это **разобранный** XML объекта (`Documents/АвансовыйОтчет.xml`), а не строка.
Именно он даёт подписи «Номер:», «Подотчетное лицо:», ширину поля «Номер» и вид
кнопок выбора у «Организация».

| Поле | Тип | Что задаёт |
|---|---|---|
| `kindTag`, `metaClass` | string | тег объекта: `Document`, `Catalog`, `Report`, … |
| `kind` | string | `'document'`, `'catalog'` (для стандартных команд/списков) |
| `name` | string | имя объекта; используется в `Document.X.Command.*` |
| `synonym` | string | синоним; подпись окна/команд |
| `listPresentation` | string | представление списка |
| `captions` | map path→string | подписи реквизитов и колонок ТЧ (`Товары.Номенклатура`) |
| `stdCaptions` | map | подписи стандартных полей (`Date`, `Number`, `Posted`, …) |
| `types` | map path→`cfg:Type.Name` | типы полей (для выбора вида редактора/ширины) |
| `stringLen` | map path→int | длины строк (ширина) |
| `numberQ` | map path→`{digits,fractionDigits,allowedSign}` | разрядность чисел |
| `dateFraction` | map path→string | `Date`/`DateTime`/`Time` — ширина даты |
| `multiLine` | map path→bool | многострочность |
| `fillChecking` | map path→string | проверка заполнения (звёздочка/ошибка) |
| `hasHelp` | bool | есть ли `Ext/Help.xml`; без него «?» может уйти в «Ещё» |
| `useStandardCommands` | bool | стандартные команды |
| `autonumbering` | `"true"`/`"false"` | «Номер»/«Код» неактивны по умолчанию |
| `numberLength` | int | ширина поля «Номер» |
| `descriptionLength`, `codeLength`, `codeType` | int/string | ширина «Наименование»/«Код» (справочники) |
| `objectBelonging` | string | `Adopted` у расширений |
| `definedType` | object/null | `{types,stringLen,numberQ,dateFraction}` для `DefinedType.X` |
| `hasFolders`, `hierarchical` | bool | иерархия списков |

Минимум для точного вида документа (проверено `variants/om-simple.json` — 0 отличий):

```json
{
  "kind": "document", "kindTag": "Document", "metaClass": "Document",
  "name": "АвансовыйОтчет", "synonym": "Авансовый отчет",
  "listPresentation": "Авансовые отчеты",
  "hasHelp": true, "useStandardCommands": true, "autonumbering": "true",
  "numberLength": 11,
  "captions": { "ФизЛицо": "Подотчетное лицо", "Организация": "Организация", ... },
  "stdCaptions": { "Date": "Дата", "Number": "Номер", "Posted": "Проведен", ... },
  "types": { "Организация": "cfg:CatalogRef.Организации", "Сумма": "xs:decimal", ... },
  "stringLen": { "Номер": 11, "Комментарий": 0, ... },
  "numberQ": { "Сумма": { "digits": 15, "fractionDigits": 2, "allowedSign": "Any" } },
  "dateFraction": { "Дата": "DateTime" },
  "multiLine": {}, "fillChecking": { ... },
  "definedType": null, "objectBelonging": ""
}
```

Что ломается без `objectMeta` (замеры на АО):

| Удалено | Отличий, px | Что видно |
|---|---:|---|
| `types/stringLen/numberQ/dateFraction` | 3 262 | другие ширины полей и кнопок выбора |
| `captions/stdCaptions` | 7 198 | подписи «Номер:», «Подотчетное лицо:» пропадают/заменяются |
| весь `objectMeta` | 8 972 | и подписи, и ширины, и стандартные команды |
| `autonumbering` (один флаг) | 294 | «Номер» становится активным (белым) |

Вывод: **упрощённо можно** — но не «только имя и вид», а именно:
`kind/name/synonym + hasHelp/useStandardCommands/autonumbering/numberLength +
captions/stdCaptions + types/stringLen/numberQ/dateFraction + multiLine/fillChecking`.
Это ровно то, что C++ уже имеет в разборе метаданных объекта: реквизиты с типами
и синонимами, табличные части с колонками, стандартные реквизиты и флаги.
Формат — как в примере выше; `parseObjectMeta()` в BslEdit — эталон.

#### 8.11.1 Что визуально меняется без `objectMeta`

Скриншоты: `shots/variant-no-objectmeta.png`, `variant-om-no-captions.png`,
`variant-om-no-types.png`, `variant-no-autonumbering.png` (сравнение с
`shots/variant-base.png` = `form-ao-model.png`).

| Без чего | diff, px | Что видно |
|---|---:|---|
| весь `objectMeta` | 8 972 | подписи `ФизЛицо:` вместо `Подотчетное лицо:`, `ПодразделениеОрганизации:` вместо `Подразделение:`, `ДокументАванса` вместо `Документ аванса`; другие ширины (`Номер` шире, `Организация` уже); другие кнопки выбора у ссылочных полей |
| `captions`/`stdCaptions` | 7 198 | то же, но без изменения типов/ширин |
| `types`/`stringLen`/`numberQ`/`dateFraction` | 3 262 | подписи на месте, но ширины/кнопки полей другие |
| `autonumbering` (только флаг) | 294 | `Номер` становится активным (белым) вместо неактивного серого |
| `hasHelp` | — | если `false` (объект есть), `Form.StandardCommand.Help` без явного `LocationInCommandBar` уезжает в «Ещё»; если `objectMeta` нет вообще, кнопка остаётся (эвристика BslEdit) |

От `objectMeta` также зависят:

* **подписи стандартных реквизитов** (`Date` → «Дата», `Number` → «Номер»,
  `Posted` → «Проведен») — через `stdCaptions`;
* **подписи и ширины реквизитов объекта и колонок ТЧ** — через `captions`/`types`;
* **ширина поля «Номер»** — через `numberLength` (11 символов);
* **кнопки выбора/открытия/календаря** у полей — через `types` (ссылка/дата/число);
* **стандартные команды и их заголовки** (`UseStandardCommands`, `kind/name`) —
  для форм, где панель автозаполняется;
* **иконки стандартных команд** — косвенно: `commandMeta` берёт `Picture` из
  `commonCommands`, а `objectMeta` задаёт `kind/name` для `Document.X.Command.*`;
* **«Создать на основании»/«Добавить»** — `createBasedOnButtons`/list-команды
  используют `objectMeta.kindTag/kind/name`; без них меню может быть пустым.

`variant-no-objectmeta.png` всё ещё рисует командную панель, потому что у АО она
authoring-овая (`AutoCommandBar` с явными кнопками), а `commonCommands`/`commonPictures`
остались. Если вместе с `objectMeta` убрать `commonCommands` — панель потеряет
иконки/подписи (см. 8.19).

### 8.12 `attributes` — реквизиты формы

Массив, каждый элемент:

```json
{
  "name": "ЦеныИВалюта",
  "id": "2",
  "properties": { "Type": "xs:string", "SavedData": "true" },
  "typeDescription": { "types": ["xs:string"], "typeSets": [], "stringQualifiers": {"length":"0","allowedLength":"Variable"} },
  "typeRefs": "xs:string",
  "columns": [],
  "stringLen": -1,
  "numberQ": null,
  "dateFraction": ""
}
```

Зачем:

* `MainAttribute=true` — определяет главный реквизит (`Объект`/`Запись`) и объект-владелец;
* `Title`/`captions` — подписи полей, у которых нет `Title`;
* `types`/`stringLen`/`numberQ`/`dateFraction` — ширина и вид редактора;
* `columns`/`columnStructure` — колонки табличных частей формы;
* `settings` — динамический список (MainTable, QueryText, FieldTitles) для форм списков.

Без `attributes` на АО — 8 396 px отличий в шапке (подписи/ширины).
Для C++: отдавайте реквизиты формы, которые реально есть в скобочном формате
(они там есть), с теми же полями; `properties` можно сократить до `Title`, `Type`,
`MainAttribute`, `SavedData`.

### 8.13 `refMeta` — метаданные ссылочных объектов

`map { 'Kind.Name': objectMeta }`, где `objectMeta` — та же схема, что 8.11.
Нужен для:

* колонок динамических списков (подписи/типы полей запроса);
* `DefinedType.*` (разворачивание состава);
* `StandardCommand` чужих объектов (`Report.X.StandardCommand.OpenList` и т.п.);
* ширины ссылочных полей (`CatalogRef.X` → длина представления).

Для АО удаление `refMeta` **не меняет пиксели** (0 отличий) — все нужные подписи
пришли из `objectMeta`/`attributes`. Но для форм списков и динамических списков
он обязателен. C++ может отдавать его лениво: только для типов, реально
встречающихся в `DataPath`/`QueryText`.

### 8.14 `commands`, `commonCommands`, `commandGroups`, `commandInterface`

`commands` — команды формы:

```json
{ "name": "ПодборТовары",
  "properties": { "Title": "Подбор", "Action": "ПодборТовары" },
  "typeDescription": null, "typeRefs": "", "columns": [], "stringLen": 0, "numberQ": null, "dateFraction": "" }
```

`commonCommands` — **общие команды**, ключи:

* `"Имя"` — обычная ссылка `CommonCommand.Имя`;
* `"@global:Имя"` — команда, применимая глобально (добавлена платформой);
* `"@global-fqn:Kind.Owner.Command.Name"` — команда объекта;
* `"@fqn:Kind.Owner.Command.Name"` — ссылка на команду другого объекта;
* `"@group:ИмяГруппы"` — группа команд.

Значение: `{ name, properties }`, где `properties` содержит
`Title` (синоним), `Picture`, `Representation`, `Group`, `CommandFqn`,
`GlobalApplicable`, `CommandParameterType`.

`commandGroups` — `{ 'Имя': {name, properties} }` для групп.

`commandInterface` — массив:

```json
{ "command": "0:...", "group": "FormCommandBarImportant", "type": "Auto",
  "index": "", "defaultVisible": "false", "visible": "false" }
```

`excludedCommands` — имена исключённых команд.

Для АО обязателен `commonCommands`: без него 5 кнопок теряют картинки/представление
(8 098 px отличий). `commands` для АО на пиксели не влияет (у кнопок есть явные
`Title`/`Picture`), но нужен для корректных подсказок и других форм.

### 8.15 `styleItems`

`map { 'ИмяСтиля': 'css-значение' }`. Например:

```json
{ "ЦветБаннераБежевый": "#FCEB9F",
  "ЗаголовкиСтрокЦветТекста": "#807A59",
  "ДосьеГиперссылкаЦвет": "#1C55AE",
  "ИтогиФонГруппы": "#FCEB9F",
  "FormBackColor": "web:Gainsboro" }
```

Рендерер подставляет значение вместо `style:Имя` в `BackColor`/`TextColor`/`BorderColor`.
Без `styleItems` на АО — 33 473 px отличий (цвета баннера и панели итогов).
C++: отдавайте `StyleItems/<Name>.xml` → `Value` как CSS-цвет; это дешёво.

### 8.16 `commonPictures`

`map { 'Имя': { mime, data } }`, `data` — base64 **без** префикса `data:`.

* `mime: "image/png" | "image/svg+xml" | "image/gif" | ...` — прямая картинка;
* `mime: "application/zip"` — 1C `Picture.zip`; рендерер распаковывает его
  через `DecompressionStream('deflate-raw')` и выбирает `Picture.png`/`100.png`/
  `l.png`/SVG по `manifest.xml`/плотности. Это штатный путь для дампа, где
  все общие картинки лежат как `CommonPictures/<Name>/Ext/Picture/Picture.zip`;
* ключи `"@item:<ИмяЭлемента>/<Файл>"` — картинки, authored на самой форме
  (`Ext/Form/Items/<item>/<file>`).

Для АО 12 картинок, ~230 КБ base64; без них — 413 px отличий (часть иконок).
Не сериализуйте служебные поля `_pictureUrlPromise`/`_pictureUrlPromise85`:
`JSON.stringify` превратит Promise в `{}`, и при повторной загрузке рендерер
получит `[object Object]` вместо картинки (мы на это наступили — см. 8.17).

### 8.17 `runtime`, производные и несериализуемые поля

`parse()` добавляет к каждому элементу `runtime`:

```json
"runtime": {
  "typeFeatures": { "selection": "extended", "clear": false, "adjust": false, "open": true },
  "inputButtons": { "choiceButton": true, "dropListButton": true, "openButton": true,
                    "clearButton": false, "spinButton": false, "choiceListButton": false,
                    "choiceButtonRepresentation": "showInDropList" },
  "inputButtonKinds": ["caret-down", "open-1c"],
  "defaultChars": 15, "presentationLength": 15,
  "minChars": 6, "maxChars": 40, "buttonsCount": 2
}
```

* `inputButtons`/`inputButtonKinds` — какие кнопки рисовать у поля (`...`, `▾`,
  календарь, очистка, `+`, спиннер, «открыть»); вычисляются из типа и authored-свойств.
* `defaultChars`/`presentationLength`/`minChars`/`maxChars` — ширина поля.
* `ignoredProps` — какие authored-свойства игнорировать (редкий случай `MaxWidth`).
* Для `Table` — `rowFilter`, `period`; для `InputField` — `choiceButtonPicture`,
  `choiceParameterLinks`, `choiceParameters`; для декораций — `border`, `backPicture`,
  `footerPicture`, `createButtonsGroupPicture`.

Без `runtime` на АО — 111 px отличий (пропадает кнопка у поля «Подотчетное лицо»).
C++ должен либо повторить вычисление, либо получить `runtime` из BslEdit-парсера
на этапе генерации модели.

**Несериализуемые (`Object.defineProperty`, `enumerable:false`) поля:**

* `_fpFormVersion` — версия формы на каждом элементе; нужна для 8.5-раскладки.
  После `JSON.parse` её нет; `model-app.js` восстанавливает её из `model.version`
  (`restoreModelRuntime`). Для Taxi на АО это не влияет, для 8.5 — может.
* `_fpOwnerTable` — ссылка search-addition на таблицу (для поиска).
* `_pictureUrlPromise*` — кэш распаковки `Picture.zip`; **никогда** не должна
  попадать в JSON (см. 8.16).
* `TitleLeadingSpaces` — ведущие пробелы подписи (для точной вёрстки кнопок).

`properties` vs `authoredProperties`: для рендера достаточно `properties`;
`authoredProperties` — исходные значения для инспектора. Удаление
`authoredProperties` даёт 0 отличий.

### 8.18 Правила сериализации модели

1. Модель — **чистый JSON**: без DOM, функций, Promise, Map/Set, `undefined`.
   `JSON.stringify` должен проходить без потерь (кроме перечисленных
   несериализуемых полей).
2. Не включайте `_pictureUrlPromise*` — они появляются только после `render`.
3. Сохраняйте `version`; при загрузке восстанавливайте `_fpFormVersion`.
4. `id` можно опустить для чисто статического вида, но он нужен для
   выбора/подсветки/интерактивности. `name` не опускайте: он участвует в
   fallback-подписях.
5. `properties` могут быть неполными: `prop()` вернёт `''`, и рендерер применит
   значение по умолчанию. Но для точного вида лучше сохранить все поля,
   которые есть в `model-ao.json`.
6. Порядок `childItems` — это порядок отображения; не сортируйте.
7. `Visible`/`ShowTitle`/`Representation` — строки `"true"`/`"false"`, не boolean.

### 8.19 Обязательное и опциональное: таблица деградации

Эксперименты: из `model-ao.json` удалялся один блок, рендер 1280×900 сравнивался
с базовым (`shots/variant-*.png`). «0 px» = пиксель-в-пиксель.

| Удалено | Отличий | Что ломается |
|---|---:|---|
| `authoredProperties` | **0** | ничего (нужно только инспектору) |
| `events` | **0** | ничего для вида |
| `contextMenu` | **0** | ничего для вида |
| `structuredProperties` | **0** | ничего для этой формы |
| `complexProperties` | **0** | ничего |
| `refMeta` | **0** | для АО ничего; для списков/DefinedType — может |
| `id` у элементов | **0** | ничего для вида (ломает выбор/подсветку) |
| `commands` | **0** | для АО ничего (кнопки с явными Title) |
| `commandGroups` | **0** | для АО ничего |
| `commandInterface` | **0** | для АО ничего |
| `excludedCommands` | **0** | для АО ничего |
| `interfaceMode`/`configInterfaceMode` | **0** для Taxi | для 8.5 — профиль/раскладка |
| `extendedTooltip` | 15 907 | пропадает «?» и серые подсказки |
| `runtime` | 111 | пропадает кнопка у поля |
| `objectMeta` | 8 972 | подписи/ширины/стандартные команды |
| `attributes` | 8 396 | подписи/ширины шапки |
| `commonPictures` | 413 | часть иконок |
| `styleItems` | 33 473 | цвета баннера/итогов |
| `autoCommandBar` | 7 809 | командной панели нет |
| `commonCommands` | 8 098 | кнопки общих команд без иконок/подписей |
| `name` у элементов | 8 396 | fallback-подписи/ключи |
| `om-simple` (без code/description полей) | **0** | для документа не нужны |

Итого:

* **Минимум для пиксель-в-пиксель АО**: `tag`, `properties` (включая `Title`),
  `childItemsRoot`, `autoCommandBar`, `attributes`, `commonCommands`,
  `commonPictures`, `styleItems`, `objectMeta`, `extendedTooltip`, `runtime`,
  `name`; `version`/`interfaceMode` — для 8.5.
* **Можно выкинуть**: `authoredProperties`, `events`, `contextMenu`,
  `structuredProperties`, `complexProperties`, `refMeta` (для этой формы),
  `commands` (для этой формы), `commandGroups`, `commandInterface`,
  `excludedCommands`, `id` (если не нужен интерактив).
* Для других форм список «можно выкинуть» может быть короче: например,
  `refMeta` нужен динамическим спискам, `commands` — кнопкам без явных `Title`.

### 8.20 Файлы моделей

| Файл | Размер | Что внутри |
|---|---:|---|
| `model-ao-full.json` | 1 036 109 | полный дамп `parsed.model` (с `authoredProperties`, `events`, `refMeta`, …) |
| `model-ao.json` | 979 967 | без `authoredProperties`; все смысловые блоки |
| `model-ao-min.json` | 673 901 | только нужное для пиксель-в-пиксель АО (без `authoredProperties`, `events`, `contextMenu`, `structuredProperties`, `complexProperties`, `refMeta`) |
| `model-minimal-ao.json` | 673 901 | копия `model-ao-min.json` — эталон для сравнения с генерацией C++ (см. 8.22) |
| `bundle/model.json` | 673 901 | копия `model-ao-min.json` для `model.html` по умолчанию |
| `model-ao-posting.json` | ~674 КБ | `model-ao-min` + `_stdPostAndClose`/`_stdPost` (пример для «как в 1С») |

`model-ao.json`/`model-ao-min.json` содержат `properties.Title = "Авансовый отчет"`,
поэтому рендерятся без `options.windowTitle`. `commonPictures` уже внутри
(12 `Picture.zip` в base64). `objectMeta`/`attributes`/`commands` — тоже.

### 8.21 Как C++ строит модель из `.cf`

C++ уже разбирает скобочный формат формы. Соответствие:

| Модель | Источник в `.cf` / действие |
|---|---|
| `childItemsRoot`, `autoCommandBar`, `attributes`, `commands` | дерево формы из скобочного формата |
| `properties` элемента | свойства элемента; имена можно оставить русскими — `prop()` их понимает |
| `objectMeta` | метаданные объекта-владельца: имя, синоним, вид, реквизиты (синоним/тип/качество), ТЧ и колонки, стандартные реквизиты, флаги (`Автонумерация`, `ИспользоватьСтандартныеКоманды`, наличие справки) |
| `refMeta` | лениво: метаданные типов из `DataPath`/`QueryText` (списки/DefinedType) |
| `commonCommands` | общие команды: имя, синоним, картинка, представление, группа, применимость; для АО достаточно 6 |
| `styleItems` | `StyleItems/*.xml` → цвет |
| `commonPictures` | `CommonPictures/*/Ext/Picture/Picture.zip` → base64 + mime `application/zip` |
| `runtime` | вычислить из типа (или взять из BslEdit-парсера): `defaultChars`, `inputButtons`, `inputButtonKinds` и т.д. |
| `interfaceMode`, `configInterfaceMode` | из флагов совместимости формы/конфигурации |
| `version` | версия формата (`2.20`) |

Если C++ не хочет повторять `runtime`/ширины/подписи, самый дешёвый путь:
на этапе разработки один раз прогнать XML-путь BslEdit и сохранить
`model-ao.json` как эталон; в рантайме C++ строит модель из своего разбора
и сверяет/дополняет её. Но для точного вида без XML всё равно нужны
`objectMeta`/`attributes`/`commonCommands`/`styleItems`/`commonPictures` —
это и есть «метаданные владельца и окружения», а не XML-документ формы.

---

### 8.22 Минимальная модель

**Рендерер не падает** на отсутствующих необязательных блоках: `prop()` возвращает
`''`, массивы подставляются как `[]`, и форма рисуется в деградированном виде.
Проверено на «скелете» `variants/skeleton.json` (скриншот `shots/variant-skeleton.png`):
там есть только `tag`, `properties`, `childItemsRoot` (группа + поле + флажок) и
`autoCommandBar` (одна кнопка) — форма рисуется, заголовок и кнопка на месте.

Минимальный рабочий JSON:

```json
{
  "tag": "Form",
  "properties": { "Title": "Минимальная форма", "AutoTitle": "false" },
  "childItemsRoot": [
    { "tag": "UsualGroup", "name": "Группа",
      "properties": { "Group": "Vertical", "Representation": "None", "ShowTitle": "false" },
      "childItems": [
        { "tag": "InputField", "name": "Поле",
          "properties": { "Title": "Поле", "Width": "15" }, "childItems": [] }
      ] }
  ],
  "autoCommandBar": {
    "tag": "AutoCommandBar", "name": "ФормаКоманднаяПанель", "id": "-1",
    "properties": {},
    "childItems": [
      { "tag": "Button", "name": "Кнопка",
        "properties": { "Title": "Кнопка", "Type": "CommandBarButton", "Representation": "Text" },
        "childItems": [] }
    ]
  }
}
```

Что обязательно / что можно опускать:

| Поле | Обязательно? | Если нет |
|---|---|---|
| `tag` | для нашего загрузчика — да; сам `render()` его не читает | `model-app.js` отвергает JSON без `tag` |
| `properties` | нет (но без `Title` не будет заголовка) | значения по умолчанию; заголовок пуст |
| `childItemsRoot` | нет (но форма будет пустой) | «Нет элементов» |
| `autoCommandBar` | нет | командной панели нет (7 809 px на АО) |
| `attributes` | нет | подписи/ширины полей по `DataPath` деградируют (8 396 px на АО) |
| `objectMeta` | нет | подписи/ширины/стандартные команды (8 972 px на АО) |
| `commands` | нет | кнопки без явного `Title` могут остаться без текста |
| `commonCommands` | нет | кнопки `CommonCommand.*` без картинок/подписей (8 098 px) |
| `commonPictures` | нет | иконки `CommonPicture.*` пропадают (413 px на АО) |
| `styleItems` | нет | цвета `style:*` не разворачиваются (33 473 px на АО) |
| `refMeta` | нет | для АО 0 px; для списков/DefinedType — подписи/типы |
| `runtime` | нет | кнопки полей/ширины (111 px на АО) |
| `extendedTooltip` | нет | «?»/подсказки (15 907 px на АО) |
| `name` | нет | fallback-подписи/ключи (8 396 px на АО) |
| `id` | нет | выбор/подсветка не работают; на вид 0 px |
| `version` | нет | для Taxi 0; для 8.5 нужна |
| `interfaceMode` | нет | Taxi по умолчанию; для 8.5 нужна |
| `events`, `contextMenu`, `structuredProperties`, `complexProperties`, `authoredProperties`, `commandInterface`, `commandGroups`, `excludedCommands` | нет | на вид АО 0 px; нужны для интерактивности/инспектора/других форм |

**`model-minimal-ao.json`** — эталонная модель АО «максимально близко к эталону»:
673 901 байт, SHA-256 `cc8dfe295b798db30b276831728c87d028eaf1c02a3b53527ea20ffc6e96ac30`.
Это `model-ao-min.json`: полный `childItemsRoot`, `autoCommandBar` (включая `_stdWrite`),
`attributes`, `commands`, `objectMeta`, `commonCommands` (6), `commonPictures` (12),
`styleItems` (5), `extendedTooltip`, `runtime`; без `authoredProperties`, `events`,
`contextMenu`, `structuredProperties`, `complexProperties`, `refMeta`.
Рендерится **пиксель-в-пиксель** с XML-путём (`shots/form-ao-model.png`).
Если C++ сравнивает свою генерацию с этим файлом — расхождения будут ровно в
`commonPictures`/`styleItems`/`commonCommands`/`runtime`/`extendedTooltip`
(если они пустые), плюс, возможно, в `attributes`/`objectMeta`.

**Чек-лист для C++-генератора модели:**

1. Всегда пишите `tag`, `name`, `id`, `properties`, `childItems` (массив, даже пустой).
2. `childItemsRoot` — массив в порядке отображения; `autoCommandBar` — отдельный Item.
3. Для таблиц колонки — в `childItems` (могут быть `ColumnGroup` с вложенными),
   `autoCommandBar` таблицы — отдельное поле.
4. Булевы — строками `"true"/"false"`; перечисления — строками 1С.
5. Ширины — в символах; у таблицы `Width`/`Height` — колонки/строки.
6. `runtime` считается из типов; если не считать — поля потеряют кнопки и ширины.
7. `commonCommands`/`commonPictures`/`styleItems` заполняйте из `.cf`, иначе
   иконки/цвета будут другими; `Picture.zip` — base64 + `mime:"application/zip"`.
8. Не сериализуйте `_pictureUrlPromise*`; `JSON.stringify` после `render`
   может превратить Promise в `{}`.
9. `objectMeta.captions`/`stdCaptions` — ключи **как в DataPath** (`ФизЛицо`,
   `Товары.Номенклатура`, `Date`, `Number`), значения — русские подписи.
10. `objectMeta.types` — `cfg:CatalogRef.X`, `cfg:DocumentRef.X`, `xs:decimal`,
    `xs:dateTime` и т.п.; `stringLen`/`numberQ`/`dateFraction` — из разбора типов.
11. `attributes` — включайте `MainAttribute=true` у `Объект`/`Запись`; от него
    зависит `formObjectKind` и стандартные команды.
12. `version`/`interfaceMode`/`configInterfaceMode` — если форма 8.5, иначе Taxi.

---

### 8.23 Что подтянуть в C++-генератор (относительно текущей генерации)

Сейчас C++ отдаёт `commonCommands`, `commonPictures`, `styleItems`, `refMeta`
пустыми. Для АО это объясняет основные отличия от `model-minimal-ao.json`.
По убыванию эффекта:

| Что добавить | Сколько для АО | Что даст | Без этого |
|---|---:|---|---|
| `styleItems` | 5 значений | цвета баннера и панели итогов | 33 473 px, серые/белые полосы |
| `commonCommands` | 6 команд | иконки/подписи/представление кнопок `CommonCommand.*` | 8 098 px, пустые/текстовые кнопки |
| `extendedTooltip` у элементов | все, что есть | «?» и подсказки | 15 907 px, нет «?» |
| `runtime` у полей/таблиц | все элементы | кнопки `...`/`▾`/календарь/очистка и ширины | 111 px + другие ширины |
| `commonPictures` | 12 `Picture.zip` (base64) | иконки `CommonPicture.*` | 413 px, другие иконки |
| `objectMeta` — полный набор карт | 1 объект | подписи/ширины/типы/«Номер» | 8 972 px |
| `attributes` — `MainAttribute`, `Title`, `Type` | 43 реквизита | подписи/ширины шапки | 8 396 px |
| `hasHelp`/`autonumbering`/`numberLength` | флаги | «?», неактивный «Номер», ширина | точечно |
| `refMeta` | 0 для АО | ничего для АО | 0 px; нужно спискам |
| `commands` | 9 | подсказки/кнопки без явного `Title` | 0 px для АО |

Проверка «как в эталоне»: сравните свою модель с `model-minimal-ao.json` по
ключевым путям (`childItemsRoot`, `autoCommandBar`, `objectMeta`, `attributes`,
`commonCommands`, `commonPictures`, `styleItems`, `runtime`). Если C++-модель
рендерится иначе, начните с этих блоков, а не с `childItemsRoot`.

**Типичные ошибки сериализации**, которые мы уже поймали:

1. `_pictureUrlPromise`/`_pictureUrlPromise85` попали в JSON как `{}` после
   `render` → `zipPictureDataUrl` возвращает объект, `<img src="[object Object]">`.
   Сериализуйте модель **до** `render` или вырезайте ключи `_pictureUrlPromise*`.
2. `commonPictures[name]` должны быть `{mime,data}`, не готовая `data:`-строка;
   `mime:"application/zip"` для `Picture.zip` — иначе картинка не распакуется.
3. `properties.Title` — плоская строка; `TitleFormatted` — отдельный флаг.
   Если заголовок окна не задан ни `properties.Title`, ни `options.windowTitle`,
   полоска заголовка будет пустой.
4. Булевы `true`/`false` — лучше строками; `""`/`undefined` = «Auto/по умолчанию».
5. `name` не опускайте: от него зависят fallback-подписи и `itemKey`.
6. `id` не влияет на вид, но без него не работает выбор/подсветка.
7. `version` сохраняйте; после `JSON.parse` вызывайте восстановление
   `_fpFormVersion` (как в `model-app.js`), иначе 8.5-раскладка может отличаться.
8. `Width`/`Height` — числа в символах/строках, не px. Для таблиц `Width` —
   колонки, `Height` — строки.

---

## 9. Ограничения и что осталось за кадром

1. **BslEdit не ставит «Провести и закрыть»/«Провести»** при `Autofill=false`.
   Это ограничение эвристики BslEdit, а не рендерера. Решение: добавить
   стандартные кнопки в модель (пример `model-ao-posting.json`).
2. **`Picture.zip`** поддерживается браузером через `DecompressionStream`
   (Chromium/WebView2 — да, старый Edge/IE — нет). Если картинка не распаковалась,
   рисуется SVG-заглушка.
3. **`AutoCellHeight`** не реализован; `AutoCellWidth` в рендерере отсутствует.
4. **8.5-профиль** (`fp-clean85`) требует `form-preview-85.css` и корректного
   `interfaceMode`/`configInterfaceMode`/`_fpFormVersion`. Для Taxi-конфигураций
   можно не отдавать 289 КБ CSS.
5. **Шрифты**: используется системный `Segoe UI`/`Arial`; внешних шрифтов нет.
   В WebView2 на Windows вид совпадает с конфигуратором; на Linux-сборке Qt
   шрифты могут отличаться.
6. **Данные**: превью не подставляет реальные значения (номер, даты, суммы) —
   это статический макет конфигуратора. Пустые поля/таблицы — норма.
7. **Интерактивность**: рендерер рисует кнопки/вкладки/флажки, но не выполняет
   команды 1С. Вкладки переключаются, попапы раскрываются, выбор элементов
   работает через `onSelect`.
8. **`io`-провайдер** в бандле не реализует `statMany/cacheGet/cachePut`,
   поэтому каталог команд строится заново. Для продакшена с XML-путём это
   стоит закэшировать (7.6).

---

## 10. Лицензия и происхождение файлов

Рендерер и CSS взяты из BslEdit (MIT). Файл лицензии:
`bundle/LICENSE-BslEdit-MIT.txt`. Он должен оставаться в бандле и в дистрибутиве
qtv8reader. BslEdit, в свою очередь, указывает MIT-происхождение части вёрстки
(CDT 41 form editor, `VScodePluginFor1CDev`); см. шапки `form-preview.js`
и `bsl-console-MIT-LICENSE.txt` в исходном проекте.

| Файл бандла | Источник | Изменения |
|---|---|---|
| `js/form-preview.js` | `bsledit-main/packages/1c-preview-core/browser/form-preview.js` | нет, байт-в-байт |
| `js/form-context.js` | `.../browser/form-context.js` | нет |
| `js/xml-util.js` | `.../browser/xml-util.js` | нет |
| `js/providers.js` | `.../browser/providers.js` | нет |
| `css/form-preview-85.css` | `.../browser/form-preview-85.css` | нет |
| `css/viewer.css` | `.../browser/viewer.css` | нет |
| `std-pictures/*` | `.../browser/std-pictures/*` | нет |
| `platform-*.png` | `.../browser/platform-*.png` | нет |
| `LICENSE-BslEdit-MIT.txt` | `bsledit-main/LICENSE` | нет |
| `index.html`, `model.html`, `css/qv8-host.css`, `js/app.js`, `js/model-app.js`, `js/qv8-io.js` | оригинальный код qv8-exact | — |

`form-preview.js` в шапке содержит ссылку на CDT 41 / `VScodePluginFor1CDev` (MIT);
`LICENSE` BslEdit покрывает весь пакет `1c-preview-core`. При переносе в репозиторий
qtv8reader положите `LICENSE-BslEdit-MIT.txt` рядом с бандлом и сохраните шапки
файлов (в них уже есть copyright-заметки).

---

## Приложение A. Полный список свойств, встреченных в `model-ao.json`

См. `shots/model-props.txt` (сгенерирован из модели): 143 уникальных
`(tag, property)`-пары. Основные группы:

* `Form`: `Title`, `AutoTitle`, `AutoTime`, `UsePostingMode`, `RepostOnWrite`,
  `CommandInterface`, `Width`, `VerticalScroll`, `CommandBarLocation`, `Group`;
* `UsualGroup`/`ButtonGroup`/`ColumnGroup`: `Title`, `Group`, `Behavior`,
  `Representation`, `ShowTitle`, `United`, `ThroughAlign`, `ChildItemsWidth`,
  `HorizontalSpacing`, `VerticalSpacing`, `CommandSource`, `Width`, `ShowInHeader`,
  `FixingInTable`, `GroupHorizontalAlign`, `HorizontalStretch`, `BackColor`;
* `InputField`: `DataPath`, `Title`, `TitleLocation`, `Width`, `Height`,
  `AutoMaxWidth`, `MaxWidth`, `HorizontalStretch`, `MultiLine`, `EditMode`,
  `ReadOnly`, `ChoiceButton`, `DropListButton`, `OpenButton`, `ClearButton`,
  `SpinButton`, `ChoiceListButton`, `ChoiceButtonRepresentation`,
  `ChoiceButtonPicture`, `ChoiceParameterLinks`, `ChoiceParameters`,
  `Format`, `EditFormat`, `TextColor`, `FontSpec`, `HeaderHorizontalAlign`,
  `FooterHorizontalAlign`, `ShowInHeader`, `ShowInFooter`, `FooterText`,
  `FixingInTable`, `InputHint`, `ToolTip`;
* `LabelField`: `DataPath`, `Title`, `Hiperlink`, `BackColor`, `TextColor`,
  `FontSpec`, `Width`, `ShowInHeader`, `ShowInFooter`, `FixingInTable`;
* `CheckBoxField`: `CheckBoxType`, `EditMode`, `Title`;
* `RadioButton(Field)`: `RadioButtonType`, `ColumnsCount`, `ChoiceList`;
* `Button`/`Popup`: `Type`, `CommandName`, `Title`, `Picture`, `Representation`,
  `DefaultButton`, `LocationInCommandBar`, `ShapeRepresentation`,
  `GroupHorizontalAlign`;
* `Table`: `Representation`, `DataPath`, `AutoInsertNewRow`, `HeaderHeight`,
  `FooterHeight`, `HorizontalLines`, `VerticalLines`, `UseAlternatingRowColoring`,
  `RowFilter`, `SearchStringLocation`, `ViewStatusLocation`, `SearchControlLocation`,
  `CommandBarLocation`, `Width`, `Height`;
* `Pages`/`Page`: `PagesRepresentation`, `Title`, `TitleDataPath`;
* `LabelDecoration`/`PictureDecoration`: `Title`, `Picture`, `AutoMaxWidth`,
  `Width`, `Height`, `Hyperlink`, `GroupHorizontalAlign`, `GroupVerticalAlign`,
  `ToolTipRepresentation`;
* `AutoCommandBar`: `Autofill`.

## Приложение B. Быстрые проверки

```powershell

Invoke-WebRequest http://127.0.0.1:8777/bundle/model.html -UseBasicParsing | % StatusCode


python -c "import json; json.load(open(r'C:\work\scratch\qv8-exact\model-ao-min.json',encoding='utf-8'))"






```

Ожидаемо:

* `model.html?dump=1` → 9 ресурсов, ни одного `.xml`, `data-qv8-state="ready"`.
* `index.html?...&dump=1` → `requests: 1322`, `bytes: 45133607`,
  `commonCommands` содержит 11 ключей, `commonPictures` — 12,
  `styleItems` — 5, `refMeta` — 15.
* `form-ao.png` = `form-ao-model.png` = `form-ao-model-inline.png` (SHA-256).

