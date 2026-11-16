cmake_minimum_required(VERSION 3.24)

project(qtv8reader
    VERSION 0.2.0
    DESCRIPTION "Read-only viewer for 1C:Enterprise configuration files"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Test Concurrent)
find_package(ZLIB REQUIRED)

qt_standard_project_setup()

add_library(qtv8reader_core STATIC
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
)

target_include_directories(qtv8reader_core PUBLIC src)
target_link_libraries(qtv8reader_core PUBLIC Qt6::Core Qt6::Concurrent ZLIB::ZLIB)

qt_add_executable(qtv8reader WIN32
    src/gui/main.cpp
    src/gui/MainWindow.h
    src/gui/MainWindow.cpp
    src/gui/PreviewDialog.h
    src/gui/PreviewDialog.cpp
)

target_link_libraries(qtv8reader PRIVATE qtv8reader_core Qt6::Widgets)

add_executable(v8reader_cli
    src/cli/main.cpp
)

target_link_libraries(v8reader_cli PRIVATE qtv8reader_core)

enable_testing()

qt_add_executable(qtv8reader_tests
    tests/test_v8text.cpp
    tests/test_v8container.cpp
    tests/test_metadata.cpp
    tests/TestContainerBuilder.h
    tests/TestContainerBuilder.cpp
)

target_link_libraries(qtv8reader_tests PRIVATE qtv8reader_core Qt6::Test)

add_test(NAME qtv8reader_tests COMMAND qtv8reader_tests)

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
