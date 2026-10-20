
cmake_minimum_required(VERSION 3.15)

project(LimitOrderBookMatchingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
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

# Main matching engine application
add_executable(matchingengine
    ${WORKLET1_SOURCES}
    ${WORKLET2_SOURCES}
    worklet2/main.cpp
)

# Header search paths
target_include_directories(matchingengine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/worklet2/include"
)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(matchingengine PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()