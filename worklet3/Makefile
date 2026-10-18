CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
CPPFLAGS += -I$(PROJECT_ROOT)/include -I$(PROJECT_ROOT)/worklet2/include -I$(PROJECT_ROOT)/worklet3/include
LDLIBS += -pthread

BUILD_DIR ?= $(PROJECT_ROOT)/build

LIB_SOURCES := $(wildcard $(PROJECT_ROOT)/src/*.cpp $(PROJECT_ROOT)/worklet2/src/*.cpp $(PROJECT_ROOT)/worklet3/src/*.cpp)
LIB_SOURCES := $(filter-out $(PROJECT_ROOT)/worklet3/src/engine_benchmarks.cpp,$(LIB_SOURCES))
TEST_MAIN := $(PROJECT_ROOT)/worklet3/tests/test_main.cpp
TEST_SOURCES := $(filter-out $(TEST_MAIN),$(wildcard $(PROJECT_ROOT)/worklet3/tests/*.cpp))

.PHONY: all test clean

all: $(BUILD_DIR)/worklet3_tests $(BUILD_DIR)/worklet3_benchmarks

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/worklet3_tests: $(TEST_MAIN) $(TEST_SOURCES) $(LIB_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/worklet3_benchmarks: $(PROJECT_ROOT)/worklet3/src/engine_benchmarks.cpp $(LIB_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

test: $(BUILD_DIR)/worklet3_tests
	$(BUILD_DIR)/worklet3_tests

clean:
	rm -f $(BUILD_DIR)/worklet3_tests $(BUILD_DIR)/worklet3_benchmarks
