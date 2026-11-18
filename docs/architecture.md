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
    FormDocument.* / FormReader.*     модель и разбор форм
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
    MxlView/MxlRenderer/MxlPrinter, FormView, PreviewDialog, BslHighlighter
  cli/                       консольная утилита
    CliApplication.*         диспетчеризация команд
    CommandUtils.*           поиск объекта, загрузка, запись файлов, отчёт
    CommandCommon.h          общие include для команд
    InfoCommand.cpp, ListCommand.cpp, TreeCommand.cpp, DumpCommand.cpp,
    MetadataCommand.cpp, ModulesCommand.cpp, CompositionCommand.cpp,
    MxlCommand.cpp, ExtractCommand.cpp, ExtractAllCommand.cpp, HelpCommand.cpp
    Output.*                 UTF-8 stdout/stderr
    main.cpp                 разбор аргументов и запуск
tests/                       Qt Test: контейнер, текст, метаданные, CLI, GUI-панели
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
5. CLI использует те же core-классы и `CliApplication` для команд.

## Правила

- `core` не зависит от Qt Widgets.
- `ConfigurationReader` — единственный владелец контейнера, модели и кэшей.
- `MetadataObject*` стабильны: подсистемы хранятся в `std::vector<std::unique_ptr<...>>`.
- Во время параллельной сборки `TypeResolver` только читается; регистрация — после.
- Новые общие строки и эвристики добавляются в `DisplayText`/`BinaryText`, а не в GUI/CLI.
