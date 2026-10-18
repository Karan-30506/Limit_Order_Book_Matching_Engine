CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -Iinclude -Iworklet2/include -Iworklet3/include
LDLIBS += -pthread

BUILD_DIR ?= build

LIB_SOURCES := $(wildcard src/*.cpp worklet2/src/*.cpp worklet3/src/*.cpp)
LIB_SOURCES := $(filter-out worklet3/src/engine_benchmarks.cpp,$(LIB_SOURCES))
TEST_MAIN := worklet3/tests/test_main.cpp
TEST_SOURCES := $(filter-out $(TEST_MAIN),$(wildcard worklet3/tests/*.cpp))

.PHONY: all test clean

all: $(BUILD_DIR)/worklet3_tests $(BUILD_DIR)/worklet3_benchmarks

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/worklet3_tests: $(TEST_MAIN) $(TEST_SOURCES) $(LIB_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/worklet3_benchmarks: worklet3/src/engine_benchmarks.cpp $(LIB_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

test: $(BUILD_DIR)/worklet3_tests
	./$(BUILD_DIR)/worklet3_tests

clean:
	rm -f $(BUILD_DIR)/worklet3_tests $(BUILD_DIR)/worklet3_benchmarks
