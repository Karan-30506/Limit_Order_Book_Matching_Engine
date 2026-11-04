
cmake_minimum_required(VERSION 3.15)

project(LimitOrderBookMatchingEngine LANGUAGES CXX)

include(CTest)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Worklet 1 source files
file(GLOB WORKLET1_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Worklet 2 source files
file(GLOB WORKLET2_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet2/src/*.cpp"
)

# Worklet 3 library sources; the standalone benchmark has its own main().
file(GLOB WORKLET3_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet3/src/*.cpp"
)
list(FILTER WORKLET3_SOURCES EXCLUDE REGEX "/engine_benchmarks\\.cpp$")

# Main matching engine application
add_executable(matchingengine
    ${WORKLET1_SOURCES}
    ${WORKLET2_SOURCES}
    worklet2/main.cpp
)

# Shared project headers
set(PROJECT_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet2/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet3/include"
)
target_include_directories(matchingengine PRIVATE ${PROJECT_INCLUDE_DIRS})

# Compiler warnings
function(enable_project_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

enable_project_warnings(matchingengine)

# Core data-structure tests for the Worklet 1 order book.
add_executable(core_tests
    tests/main_test_runner.cpp
    tests/test_order.cpp
    tests/test_order_queue.cpp
    tests/test_limit_node.cpp
    tests/test_avl_tree.cpp
    tests/test_hash_map.cpp
    tests/test_book_combined.cpp
    ${WORKLET1_SOURCES}
)
target_include_directories(core_tests PRIVATE
    ${PROJECT_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/tests"
)
enable_project_warnings(core_tests)

# Focused regressions for previously fixed order-book behavior.
add_executable(regression_tests
    tests/regression_tests.cpp
    ${WORKLET1_SOURCES}
)
target_include_directories(regression_tests PRIVATE
    ${PROJECT_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/tests"
)
enable_project_warnings(regression_tests)

add_executable(unit_tests
    worklet3/tests/test_main.cpp
    worklet3/tests/unit_tests/benchmark_tests.cpp
    worklet3/tests/unit_tests/book_scalability_tests.cpp
    worklet3/tests/unit_tests/event_logger_replay_integration_tests.cpp
    worklet3/tests/unit_tests/full_workflow_tests.cpp
    worklet3/tests/unit_tests/market_data_feed_tests.cpp
    worklet3/tests/unit_tests/replay_engine_tests.cpp
    ${WORKLET1_SOURCES}
    ${WORKLET2_SOURCES}
    ${WORKLET3_SOURCES}
)
target_include_directories(unit_tests PRIVATE
    ${PROJECT_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet3/tests"
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet3/tests/unit_tests"
)
target_link_libraries(unit_tests PRIVATE Threads::Threads)
enable_project_warnings(unit_tests)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(unit_tests PRIVATE -UNDEBUG)
elseif(MSVC)
    target_compile_options(unit_tests PRIVATE /UNDEBUG)
endif()

# Run the end-to-end Worklet 1/2/3 workflow independently.
add_executable(full_workflow_tests
    worklet3/tests/unit_tests/full_workflow_test_main.cpp
    worklet3/tests/unit_tests/full_workflow_tests.cpp
    ${WORKLET1_SOURCES}
    ${WORKLET2_SOURCES}
    ${WORKLET3_SOURCES}
)
target_include_directories(full_workflow_tests PRIVATE
    ${PROJECT_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet3/tests"
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet3/tests/unit_tests"
)
target_link_libraries(full_workflow_tests PRIVATE Threads::Threads)
enable_project_warnings(full_workflow_tests)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(full_workflow_tests PRIVATE -UNDEBUG)
elseif(MSVC)
    target_compile_options(full_workflow_tests PRIVATE /UNDEBUG)
endif()

add_executable(worklet3_benchmarks
    worklet3/src/engine_benchmarks.cpp
    ${WORKLET1_SOURCES}
    ${WORKLET2_SOURCES}
    ${WORKLET3_SOURCES}
)
target_include_directories(worklet3_benchmarks PRIVATE ${PROJECT_INCLUDE_DIRS})
target_link_libraries(worklet3_benchmarks PRIVATE Threads::Threads)
enable_project_warnings(worklet3_benchmarks)

# Data-driven and randomized tests for Worklet 1/2, sharing the project engine.
add_executable(worklet3_harness
    worklet3/tests/harness/test_main.cpp
    worklet3/tests/harness/scenario_tests.cpp
    worklet3/tests/harness/differential_tests.cpp
    worklet3/tests/harness/ScenarioRunner.cpp
    worklet3/tests/harness/ReferenceModel.cpp
    ${WORKLET1_SOURCES}
    ${WORKLET2_SOURCES}
)
target_include_directories(worklet3_harness PRIVATE
    ${PROJECT_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet3/tests/harness"
)
target_compile_definitions(worklet3_harness PRIVATE
    "LOB_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/worklet3/tests/harness/data\""
)
target_link_libraries(worklet3_harness PRIVATE Threads::Threads)
enable_project_warnings(worklet3_harness)

# Parameterized Worklet 3/engine tests are independently runnable.
add_executable(parameterized_tests
    worklet3/tests/harness/test_main.cpp
    worklet3/tests/harness/param_tests.cpp
    ${WORKLET1_SOURCES}
    ${WORKLET2_SOURCES}
)
target_include_directories(parameterized_tests PRIVATE
    ${PROJECT_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet3/tests/harness"
)
target_link_libraries(parameterized_tests PRIVATE Threads::Threads)
enable_project_warnings(parameterized_tests)

# These suites replace selected collaborators with test doubles and therefore
# must not link the production implementations of those collaborators.
set(WORKLET2_STUB_SOURCES ${WORKLET2_SOURCES})
list(FILTER WORKLET2_STUB_SOURCES EXCLUDE REGEX
    "/(OrderValidator|LimitOrderMatcher|MarketOrderMatcher)\\.cpp$")

add_executable(worklet3_stub_routing_tests
    worklet3/tests/harness/stub_routing_test.cpp
    worklet3/tests/harness/stubs/StubMatchers.cpp
    ${WORKLET1_SOURCES}
    ${WORKLET2_STUB_SOURCES}
)
target_include_directories(worklet3_stub_routing_tests PRIVATE
    ${PROJECT_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet3/tests/harness"
)
enable_project_warnings(worklet3_stub_routing_tests)

set(WORKLET3_STUB_SOURCES ${WORKLET3_SOURCES})
list(FILTER WORKLET3_STUB_SOURCES EXCLUDE REGEX
    "/(EventLogger|EventPersistance|EventSerialization)\\.cpp$")

add_executable(worklet3_stub_integration_tests
    worklet3/tests/harness/stub_integration_test.cpp
    worklet3/tests/harness/stubs/StubEventLogger.cpp
    ${WORKLET1_SOURCES}
    ${WORKLET2_SOURCES}
    ${WORKLET3_STUB_SOURCES}
)
target_include_directories(worklet3_stub_integration_tests PRIVATE
    ${PROJECT_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet3/tests/harness"
)
target_link_libraries(worklet3_stub_integration_tests PRIVATE Threads::Threads)
enable_project_warnings(worklet3_stub_integration_tests)

if(BUILD_TESTING)
    add_test(NAME core_tests COMMAND core_tests)
    add_test(NAME regression_tests COMMAND regression_tests)
    add_test(NAME unit_tests COMMAND unit_tests)
    add_test(NAME full_workflow_tests COMMAND full_workflow_tests)
    add_test(NAME worklet3_harness COMMAND worklet3_harness)
    add_test(NAME parameterized_tests COMMAND parameterized_tests)
    add_test(NAME worklet3_stub_routing_tests COMMAND worklet3_stub_routing_tests)
    add_test(NAME worklet3_stub_integration_tests COMMAND worklet3_stub_integration_tests)

    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS
            core_tests
            regression_tests
            unit_tests
            full_workflow_tests
            worklet3_harness
            parameterized_tests
            worklet3_stub_routing_tests
            worklet3_stub_integration_tests
        USES_TERMINAL
    )
endif()