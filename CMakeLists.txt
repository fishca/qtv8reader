cmake_minimum_required(VERSION 3.22)

project(qtv8reader
    VERSION 0.8.0
    DESCRIPTION "Read-only viewer for 1C:Enterprise configuration files"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets PrintSupport Test Concurrent)
find_package(ZLIB REQUIRED)

qt_standard_project_setup()

add_library(qtv8reader_core STATIC
    src/core/BinaryText.h
    src/core/BinaryText.cpp
    src/core/DisplayText.h
    src/core/DisplayText.cpp
    src/core/MetadataTextUtils.h
    src/core/MetadataTextUtils.cpp
    src/core/TypeResolver.h
    src/core/TypeResolver.cpp
    src/core/SubscriptionParser.h
    src/core/SubscriptionParser.cpp
    src/core/SectionBuilder.h
    src/core/SectionBuilder.cpp
    src/core/V8Text.h
    src/core/V8Text.cpp
    src/core/V8Container.h
    src/core/V8Container.cpp
    src/core/MetadataCatalog.h
    src/core/MetadataCatalog.cpp
    src/core/Configuration.h
    src/core/Configuration.cpp
    src/core/ConfigurationReader.h
    src/core/ConfigurationReader.cpp
    src/core/ModuleExtractor.h
    src/core/ModuleExtractor.cpp
    src/core/Extractor.h
    src/core/Extractor.cpp
    src/core/MxlDocument.h
    src/core/MxlDocument.cpp
    src/core/MxlReader.h
    src/core/MxlReader.cpp
    src/core/FormDocument.h
    src/core/FormDocument.cpp
    src/core/FormOwner.h
    src/core/FormOwner.cpp
    src/core/FormWebModel.h
    src/core/FormWebModel.cpp
    src/core/FormBindings.h
    src/core/FormBindings.cpp
    src/core/FormCommands.h
    src/core/FormCommands.cpp
    src/core/FormReader.h
    src/core/FormReader.cpp
    src/core/FormStyles.h
    src/core/FormStyles.cpp
    src/core/FormPictures.h
    src/core/FormPictures.cpp
)

target_include_directories(qtv8reader_core PUBLIC src)
target_link_libraries(qtv8reader_core PUBLIC Qt6::Core Qt6::Concurrent ZLIB::ZLIB)

add_library(qtv8reader_gui STATIC
    src/gui/MainWindow.h
    src/gui/MainWindow.cpp
    src/gui/MetadataIcons.h
    src/gui/MetadataIcons.cpp
    src/gui/ThemeController.h
    src/gui/ThemeController.cpp
    src/gui/ModulePanel.h
    src/gui/ModulePanel.cpp
    src/gui/PropertiesPanel.h
    src/gui/PropertiesPanel.cpp
    src/gui/RecentFilesController.h
    src/gui/RecentFilesController.cpp
    src/gui/BslHighlightSettings.h
    src/gui/BslHighlightSettings.cpp
    src/gui/HighlightSettingsDialog.h
    src/gui/HighlightSettingsDialog.cpp
    src/gui/ExtractionController.h
    src/gui/ExtractionController.cpp
    src/gui/MainWindowSupport.h
    src/gui/MainWindowSupport.cpp
    src/gui/MetadataTreeRoles.h
    src/gui/MainWindowTree.cpp
    src/gui/MainWindowDetails.cpp
    src/gui/MainWindowMxlForm.cpp
    src/gui/PreviewDialog.h
    src/gui/PreviewDialog.cpp
    src/gui/MxlView.h
    src/gui/MxlView.cpp
    src/gui/MxlRenderer.h
    src/gui/MxlRenderer.cpp
    src/gui/MxlPrinter.h
    src/gui/MxlPrinter.cpp
    src/gui/FormView.h
    src/gui/FormView.cpp
    src/gui/web/WebView2Host.h
    src/gui/web/WebView2Host.cpp
    src/gui/controls/ButtonControl.h
    src/gui/controls/ButtonControl.cpp
    src/gui/controls/CommandBarControl.h
    src/gui/controls/CommandBarControl.cpp
    src/gui/controls/ControlGallery.h
    src/gui/controls/ControlGallery.cpp
    src/gui/controls/FieldControl.h
    src/gui/controls/FieldControl.cpp
    src/gui/controls/FormControl.h
    src/gui/controls/FormStyle.h
    src/gui/controls/SearchControl.h
    src/gui/controls/SearchControl.cpp
    src/gui/controls/ScrollBarControl.h
    src/gui/controls/ScrollBarControl.cpp
    src/gui/controls/StaticControl.h
    src/gui/controls/StaticControl.cpp
    src/gui/controls/TabControl.h
    src/gui/controls/TabControl.cpp
    src/gui/controls/TableControl.h
    src/gui/controls/TableControl.cpp
    src/gui/BslHighlighter.h
    src/gui/BslHighlighter.cpp
)

target_link_libraries(qtv8reader_gui PUBLIC qtv8reader_core Qt6::Widgets Qt6::PrintSupport)

if (WIN32)
    # Точный вид формы: Microsoft Edge WebView2 (Chromium).
    # Заголовки SDK нужны только на этапе сборки, WebView2Loader.dll —
    # источник функции CreateCoreWebView2EnvironmentWithOptions; интерфейсы
    # COM вызываются напрямую через виртуальные таблицы, поэтому SDK-библиотека
    # не линкуется.
    target_include_directories(qtv8reader_gui PRIVATE 3rdparty/webview2/include)
    target_compile_definitions(qtv8reader_gui PRIVATE QTV8READER_WEBVIEW2=1)
    target_link_libraries(qtv8reader_gui PUBLIC shlwapi ole32 oleaut32)
endif()

qt_add_executable(qtv8reader WIN32
    resources/resources.qrc
    src/gui/main.cpp
)

target_link_libraries(qtv8reader PRIVATE qtv8reader_gui)
target_compile_definitions(qtv8reader PRIVATE QTV8READER_VERSION="${PROJECT_VERSION}")

if (WIN32)
    add_custom_command(TARGET qtv8reader POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/webview2/x64/WebView2Loader.dll"
            "$<TARGET_FILE_DIR:qtv8reader>/WebView2Loader.dll"
        COMMENT "Копирование WebView2Loader.dll")
endif()

add_library(qtv8reader_cli_lib STATIC
    src/cli/CliApplication.h
    src/cli/CliApplication.cpp
    src/cli/CommandCommon.h
    src/cli/Commands.h
    src/cli/CommandUtils.h
    src/cli/CommandUtils.cpp
    src/cli/Output.h
    src/cli/Output.cpp
    src/cli/HelpCommand.cpp
    src/cli/InfoCommand.cpp
    src/cli/ListCommand.cpp
    src/cli/TreeCommand.cpp
    src/cli/DumpCommand.cpp
    src/cli/MetadataCommand.cpp
    src/cli/ModulesCommand.cpp
    src/cli/CompositionCommand.cpp
    src/cli/FormModelCommand.cpp
    src/cli/MxlCommand.cpp
    src/cli/ExtractCommand.cpp
    src/cli/ExtractAllCommand.cpp
)

target_link_libraries(qtv8reader_cli_lib PUBLIC qtv8reader_core)

add_executable(v8reader_cli
    src/cli/main.cpp
)

target_link_libraries(v8reader_cli PRIVATE qtv8reader_cli_lib)
target_compile_definitions(v8reader_cli PRIVATE QTV8READER_VERSION="${PROJECT_VERSION}")

enable_testing()

qt_add_executable(qtv8reader_tests
    tests/test_display_text.cpp
    tests/test_v8text.cpp
    tests/test_v8container.cpp
    tests/test_metadata.cpp
    tests/test_mxl.cpp
    tests/test_form.cpp
    tests/TestContainerBuilder.h
    tests/TestContainerBuilder.cpp
)

target_link_libraries(qtv8reader_tests PRIVATE qtv8reader_core Qt6::Test)

add_test(NAME qtv8reader_tests COMMAND qtv8reader_tests)

qt_add_executable(qtv8reader_cli_tests
    tests/test_cli.cpp
    tests/TestContainerBuilder.h
    tests/TestContainerBuilder.cpp
)

target_link_libraries(qtv8reader_cli_tests PRIVATE qtv8reader_cli_lib Qt6::Test)
add_test(NAME qtv8reader_cli_tests COMMAND qtv8reader_cli_tests)

qt_add_executable(qtv8reader_gui_tests
    tests/test_gui_panels.cpp
)

target_link_libraries(qtv8reader_gui_tests PRIVATE qtv8reader_gui Qt6::Test)
add_test(NAME qtv8reader_gui_tests COMMAND qtv8reader_gui_tests)
set_tests_properties(qtv8reader_gui_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

if (MSVC)
    target_compile_options(qtv8reader_core PRIVATE /W4 /utf-8)
    target_compile_options(qtv8reader PRIVATE /W4 /utf-8)
    target_compile_options(v8reader_cli PRIVATE /W4 /utf-8)
    target_compile_options(qtv8reader_tests PRIVATE /W4 /utf-8)
else ()
    target_compile_options(qtv8reader_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(qtv8reader PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(v8reader_cli PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(qtv8reader_tests PRIVATE -Wall -Wextra -Wpedantic)
endif ()
