
cmake_minimum_required(VERSION 3.15)

project(LimitOrderBookMatchingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

file(GLOB ORDER_BOOK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Limit_Order_Book/src/*.cpp"
)
file(GLOB MATCHING_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Matching_Engine/src/*.cpp"
)
list(REMOVE_ITEM MATCHING_ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Matching_Engine/src/main.cpp"
)
file(GLOB TESTING_PERFORMANCE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Testing_and_Performance/src/*.cpp"
)

add_library(lob_core STATIC
    ${ORDER_BOOK_SOURCES}
    ${MATCHING_ENGINE_SOURCES}
)
target_include_directories(lob_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Limit_Order_Book/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/Matching_Engine/include"
)

add_library(lob_engine STATIC
    ${TESTING_PERFORMANCE_SOURCES}
)
target_include_directories(lob_engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Testing_and_Performance/include"
)
target_link_libraries(lob_engine PUBLIC lob_core Threads::Threads)

add_executable(matchingengine Matching_Engine/src/main.cpp)
target_link_libraries(matchingengine PRIVATE lob_engine)

function(use_static_gnu_runtime target)
    if(WIN32 AND MINGW)
        target_link_options(${target} PRIVATE
            -static-libgcc
            -static-libstdc++
        )
    endif()
endfunction()

use_static_gnu_runtime(matchingengine)

option(BUILD_QT_GUI "Build the Qt 6 desktop application" OFF)
if(BUILD_QT_GUI)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets OpenGLWidgets PrintSupport)

    include(FetchContent)

    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(
        qcustomplot_source
        URL https://www.qcustomplot.com/release/2.1.1/QCustomPlot-source.tar.gz
        URL_HASH SHA256=5E2D22DEC779DB8F01F357CBDB25E54FBCF971ADAEE75EAE8D7AD2444487182F
    )
    FetchContent_GetProperties(qcustomplot_source)
    if(NOT qcustomplot_source_POPULATED)
        FetchContent_Populate(qcustomplot_source)
    endif()

    add_library(qcustomplot STATIC
        "${qcustomplot_source_SOURCE_DIR}/qcustomplot.cpp"
        "${qcustomplot_source_SOURCE_DIR}/qcustomplot.h"
    )
    set_target_properties(qcustomplot PROPERTIES AUTOMOC ON)
    target_link_libraries(qcustomplot PUBLIC Qt6::Widgets Qt6::PrintSupport)
    target_include_directories(qcustomplot PUBLIC "${qcustomplot_source_SOURCE_DIR}")

    add_executable(lob_gui
        GUI/src/main.cpp
        GUI/src/MainWindow.cpp
        GUI/src/EngineService.cpp
        GUI/src/EngineWorker.cpp
        GUI/src/CsvWorker.cpp
        GUI/src/CsvTableModel.cpp
        GUI/include/MainWindow.hpp
        GUI/include/EngineService.hpp
        GUI/include/EngineWorker.hpp
        GUI/include/CsvWorker.hpp
        GUI/include/CsvTableModel.hpp
    )
    set_target_properties(lob_gui PROPERTIES AUTOMOC ON WIN32_EXECUTABLE ON)
    target_include_directories(lob_gui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/GUI/include")
    target_link_libraries(lob_gui PRIVATE
        lob_engine
        Qt6::Widgets
        Qt6::OpenGLWidgets
        qcustomplot
    )
    if(WIN32)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt
            HINTS "${Qt6_DIR}/../../../bin" "$ENV{QTDIR}/bin"
        )
        if(NOT WINDEPLOYQT_EXECUTABLE)
            message(FATAL_ERROR "windeployqt was not found in the selected Qt 6 kit.")
        endif()
        set(GUI_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist")
        add_custom_target(package_lob_gui
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${GUI_PACKAGE_DIR}"
            COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:lob_gui>"
                    "${GUI_PACKAGE_DIR}/$<TARGET_FILE_NAME:lob_gui>"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --compiler-runtime --no-translations
                    --dir "${GUI_PACKAGE_DIR}"
                    "${GUI_PACKAGE_DIR}/$<TARGET_FILE_NAME:lob_gui>"
            DEPENDS lob_gui
            COMMENT "Preparing a Windows Qt deployment in ${GUI_PACKAGE_DIR}"
            VERBATIM
        )
        install(TARGETS lob_gui RUNTIME DESTINATION .)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lob_core PRIVATE
        -Wall -Wextra -Wpedantic
    )
    target_compile_options(lob_engine PRIVATE
        -Wall -Wextra -Wpedantic
    )
    target_compile_options(matchingengine PRIVATE
        -Wall -Wextra -Wpedantic
    )
    if(TARGET lob_gui)
        target_compile_options(lob_gui PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(lob_tests tests/test_main.cpp)
    target_link_libraries(lob_tests PRIVATE lob_engine)
    use_static_gnu_runtime(lob_tests)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lob_tests PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()
    add_test(NAME lob_tests COMMAND lob_tests)
    set_tests_properties(lob_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
    add_executable(engine_service_tests
        tests/engine_service_test.cpp
        GUI/src/EngineService.cpp
    )
    target_include_directories(engine_service_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/GUI/include"
    )
    target_link_libraries(engine_service_tests PRIVATE lob_engine)
    use_static_gnu_runtime(engine_service_tests)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(engine_service_tests PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()
    add_test(NAME engine_service_tests COMMAND engine_service_tests)
    set_tests_properties(engine_service_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
    if(TARGET lob_gui)
        add_executable(engine_worker_tests
            tests/engine_worker_test.cpp
            GUI/src/EngineService.cpp
            GUI/src/EngineWorker.cpp
            GUI/include/EngineWorker.hpp
        )
        set_target_properties(engine_worker_tests PROPERTIES AUTOMOC ON)
        target_include_directories(engine_worker_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/GUI/include"
        )
        target_link_libraries(engine_worker_tests PRIVATE lob_engine Qt6::Core)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(engine_worker_tests PRIVATE
                -Wall -Wextra -Wpedantic
            )
        endif()
        add_test(NAME engine_worker_tests COMMAND engine_worker_tests)
        set_tests_properties(engine_worker_tests PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        )
        add_executable(csv_viewer_tests
            tests/csv_viewer_test.cpp
            GUI/src/CsvWorker.cpp
            GUI/src/CsvTableModel.cpp
            GUI/include/CsvWorker.hpp
        )
        set_target_properties(csv_viewer_tests PROPERTIES AUTOMOC ON)
        target_include_directories(csv_viewer_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/GUI/include"
        )
        target_link_libraries(csv_viewer_tests PRIVATE Qt6::Core Qt6::Gui)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(csv_viewer_tests PRIVATE
                -Wall -Wextra -Wpedantic
            )
        endif()
        add_test(NAME csv_viewer_tests COMMAND csv_viewer_tests)
        set_tests_properties(csv_viewer_tests PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        )
        add_executable(gui_smoke_tests
            tests/gui_smoke_test.cpp
            GUI/src/MainWindow.cpp
            GUI/src/EngineService.cpp
            GUI/src/EngineWorker.cpp
            GUI/src/CsvWorker.cpp
            GUI/src/CsvTableModel.cpp
            GUI/include/MainWindow.hpp
            GUI/include/EngineService.hpp
            GUI/include/EngineWorker.hpp
            GUI/include/CsvWorker.hpp
            GUI/include/CsvTableModel.hpp
        )
        set_target_properties(gui_smoke_tests PROPERTIES AUTOMOC ON)
        target_include_directories(gui_smoke_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/GUI/include"
        )
        target_link_libraries(gui_smoke_tests PRIVATE
            lob_engine Qt6::Widgets Qt6::OpenGLWidgets qcustomplot
        )
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(gui_smoke_tests PRIVATE
                -Wall -Wextra -Wpedantic
            )
        endif()
        add_test(NAME gui_smoke_tests COMMAND gui_smoke_tests)
        set_tests_properties(gui_smoke_tests PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_OPENGL=software"
        )
    endif()
endif()