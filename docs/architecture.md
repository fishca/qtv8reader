# Архитектура qtv8reader

## Слои

```
src/
  core/                      без Qt Widgets, только Qt Core/Concurrent + zlib
    V8Container.*            контейнер 1C:V8: заголовки, TOC, цепочки блоков, zlib
    V8Text.*                 разбор текстового формата 1С (списки, строки, #base64)
    MetadataCatalog.*        категории, раскладка таблиц состава, GUID
    Configuration.*          модель: конфигурация → категории → объекты → состав
    MetadataTextUtils.*      общие обходы V8Text (описатели, GUID, локализация)
    TypeResolver.*           идентификаторы типов → «Строка(50)», «СправочникСсылка.…»
    SubscriptionParser.*      подписки: источник, событие, обработчик
    SectionBuilder.*         состав объекта: реквизиты, формы, макеты, ТЧ
    ConfigurationReader.*    фасад: контейнер → модель, ленивые секции и подсистемы
    ModuleExtractor.*        поиск текстов модулей
    Extractor.*              выгрузка объектов на диск
    MxlDocument.* / MxlReader.*       модель и разбор MXL
    FormDocument.* / FormReader.*     модель и разбор управляемых форм
    FormOwner.*              метаданные владельца: типы, заголовки, проверка заполнения
    FormBindings.*           пути данных полей: реквизиты, динамические списки
    FormCommands.*           стандартные команды формы и элементов
    FormStyles.*             стили оформления 1С → CSS
    FormPictures.*           общие и стандартные картинки, RowPictureDataPath
    FormWebModel.*           JSON-модель формы для HTML-рендерера «точного вида»
    BinaryText.*             эвристика «текст/бинарник» (общая для core/CLI/GUI)
    DisplayText.*            общие строки: событие, тип, дата
  gui/                       Qt Widgets
    MainWindow.*             координатор: меню, файл, настройки, связывание
    MainWindowSupport.*      порядок веток 1С, размеры, общие предикаты
    MainWindowTree.cpp       построение дерева и ленивая загрузка
    MainWindowDetails.cpp    свойства, состав, метаданные, модули
    MainWindowMxlForm.cpp    MXL/формы: просмотр, печать, PDF
    MetadataIcons.*          иконки metadata-icons
    ThemeController.*        светлая/тёмная тема и сохранение
    ModulePanel.*            вкладка «Модуль»
    PropertiesPanel.*        таблица «Свойства»
    RecentFilesController.*  недавние файлы и меню
    ExtractionController.*   выгрузка и текст отчёта
    MxlView/MxlRenderer/MxlPrinter, PreviewDialog
    FormView.*               просмотр формы: упрощённый рендер и режим «точный»
    web/WebView2Host.*       WebView2 (Chromium) и виртуальный хост qv8.local
    controls/                Qt-контролы: поля, таблицы, кнопки, панели, поиск
    BslHighlighter + BslHighlightSettings + HighlightSettingsDialog
  cli/                       консольная утилита
    CliApplication.*         диспетчеризация команд
    CommandUtils.*           поиск объекта, загрузка, запись файлов, отчёт
    CommandCommon.h          общие include для команд
    InfoCommand.cpp, ListCommand.cpp, TreeCommand.cpp, DumpCommand.cpp,
    MetadataCommand.cpp, ModulesCommand.cpp, CompositionCommand.cpp,
    FormModelCommand.cpp, MxlCommand.cpp, ExtractCommand.cpp,
    ExtractAllCommand.cpp, HelpCommand.cpp
    Output.*                 UTF-8 stdout/stderr
    main.cpp                 разбор аргументов и запуск
tests/                       Qt Test: контейнер, текст, метаданные, формы, CLI, GUI-панели
tools/                       form_xml_compare.py — сверка модели формы с Form.xml
resources/formpreview/       рендерер BslEdit, CSS, std-pictures, platform-*.png
```

## Поток данных

1. `ConfigurationReader::open` открывает `V8Container` и читает `root`/`configinfo`.
2. `MetadataTextUtils`, `TypeResolver`, `SubscriptionParser` и `SectionBuilder`
   строят `Configuration`; объекты разбираются параллельно, типы регистрируются
   после сборки.
3. `MainWindowTree.cpp` строит дерево и лениво загружает состав;
   `MainWindowDetails.cpp` заполняет свойства/состав/метаданные, `ModulePanel` —
   модули.
4. `Extractor` выгружает выбранный объект или всю конфигурацию.
5. Для «точного вида» `FormWebModel` сериализует `FormDocument` в JSON;
   `FormView` на Windows отдаёт его странице `model.html` через `WebView2Host`
   на виртуальном хосте `https://qv8.local/`. Ресурсы берутся из qrc, сеть
   и XML-выгрузка не используются; без WebView2 Runtime форма показывается
   упрощённо.
6. CLI использует те же core-классы и `CliApplication` для команд;
   `form-model` печатает ту же JSON-модель.

## Правила

- `core` не зависит от Qt Widgets.
- `ConfigurationReader` — единственный владелец контейнера, модели и кэшей.
- `MetadataObject*` стабильны: подсистемы хранятся в `std::vector<std::unique_ptr<...>>`.
- Во время параллельной сборки `TypeResolver` только читается; регистрация — после.
- Новые общие строки и эвристики добавляются в `DisplayText`/`BinaryText`, а не в GUI/CLI.
- «Точный вид» не использует XML-выгрузку: данные — из контейнера, ресурсы —
  из qrc; внешняя сверка с платформой (`tools/form_xml_compare.py`) работает
  по `Form.xml` и в сборку не входит.
