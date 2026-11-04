
cmake_minimum_required(VERSION 3.15)

project(LimitOrderBookMatchingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

file(GLOB WORKLET1_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB WORKLET2_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet2/src/*.cpp"
)
file(GLOB WORKLET3_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet3/src/*.cpp"
)

add_library(lob_engine STATIC
    ${WORKLET1_SOURCES}
    ${WORKLET2_SOURCES}
    ${WORKLET3_SOURCES}
)
target_include_directories(lob_engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet2/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet3/include"
)
target_link_libraries(lob_engine PUBLIC Threads::Threads)

add_executable(matchingengine worklet2/main.cpp)
target_link_libraries(matchingengine PRIVATE lob_engine)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lob_engine PRIVATE
        -Wall -Wextra -Wpedantic
    )
    target_compile_options(matchingengine PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(lob_tests tests/test_main.cpp)
    target_link_libraries(lob_tests PRIVATE lob_engine)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lob_tests PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()
    add_test(NAME lob_tests COMMAND lob_tests)
    set_tests_properties(lob_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
endif()