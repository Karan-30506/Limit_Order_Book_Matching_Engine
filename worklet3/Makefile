CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I../include -I../worklet2/include -Iinclude
LDLIBS += -pthread

BUILD_DIR ?= build

LIB_SOURCES := $(wildcard ../src/*.cpp ../worklet2/src/*.cpp src/*.cpp)
LIB_SOURCES := $(filter-out src/engine_benchmarks.cpp,$(LIB_SOURCES))
TEST_MAIN := tests/test_main.cpp
TEST_SOURCES := $(filter-out $(TEST_MAIN),$(wildcard tests/*.cpp))

.PHONY: all test clean tests

all: $(BUILD_DIR)/worklet3_tests $(BUILD_DIR)/worklet3_benchmarks

tests: $(BUILD_DIR)/worklet3_tests

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/worklet3_tests: $(TEST_MAIN) $(TEST_SOURCES) $(LIB_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/worklet3_benchmarks: src/engine_benchmarks.cpp $(LIB_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

test: $(BUILD_DIR)/worklet3_tests
	./$(BUILD_DIR)/worklet3_tests

clean:
	rm -f $(BUILD_DIR)/worklet3_tests $(BUILD_DIR)/worklet3_benchmarks