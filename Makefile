CXX = g++
LDFLAGS = -static-libgcc -static-libstdc++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude -Itests -O2

SRC = src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp src/AVLTree.cpp src/Book.cpp
TEST_SRC = tests/test_order.cpp tests/test_order_queue.cpp tests/test_limit_node.cpp \
           tests/test_avl_tree.cpp tests/test_hash_map.cpp tests/test_book_combined.cpp \
           tests/test_csv.cpp tests/main_test_runner.cpp

HEADERS = $(wildcard include/*.hpp include/*.tpp tests/*.hpp)

ALL_TEST_EXE = run_all_tests.exe

.PHONY: all test test_debug test_csv_validation benchmark failure_test failure_debug_test clean test_order test_order_queue test_limit_node test_avl_tree test_hash_map test_book_combined

all: $(ALL_TEST_EXE)

$(ALL_TEST_EXE): $(SRC) $(TEST_SRC) $(HEADERS) tests/testcases.csv
	$(CXX) $(CXXFLAGS) $(SRC) $(TEST_SRC) -o $(ALL_TEST_EXE) $(LDFLAGS)

test: $(ALL_TEST_EXE)
	./$(ALL_TEST_EXE)

run_debug_tests.exe: $(SRC) $(TEST_SRC) $(HEADERS) tests/testcases.csv
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Iinclude -Itests -Og -g -D_GLIBCXX_DEBUG $(SRC) $(TEST_SRC) -o $@ $(LDFLAGS)

test_debug: run_debug_tests.exe
	./run_debug_tests.exe

test_csv_validation: $(ALL_TEST_EXE)
	powershell -NoProfile -ExecutionPolicy Bypass -File tests/validate_csv.ps1

benchmark.exe: $(SRC) tests/benchmark.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -Werror $(SRC) tests/benchmark.cpp -o $@ $(LDFLAGS)

benchmark: benchmark.exe
	./benchmark.exe
	powershell -NoProfile -ExecutionPolicy Bypass -File tests/write_benchmark_report.ps1

failure_tests.exe: $(SRC) tests/failure_cases.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -Werror $(SRC) tests/failure_cases.cpp -o $@ $(LDFLAGS)

failure_test: failure_tests.exe
	./failure_tests.exe

failure_debug_tests.exe: $(SRC) tests/failure_cases.cpp $(HEADERS)
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Iinclude -Itests -Og -g -D_GLIBCXX_DEBUG $(SRC) tests/failure_cases.cpp -o $@ $(LDFLAGS)

failure_debug_test: failure_debug_tests.exe
	./failure_debug_tests.exe

test_order: tests/test_order.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp
	$(CXX) $(CXXFLAGS) -DSTANDALONE_TEST_ORDER tests/test_order.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp -o test_order.exe $(LDFLAGS)
	./test_order.exe

test_order_queue: tests/test_order_queue.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp
	$(CXX) $(CXXFLAGS) -DSTANDALONE_TEST_ORDER_QUEUE tests/test_order_queue.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp -o test_order_queue.exe $(LDFLAGS)
	./test_order_queue.exe

test_limit_node: tests/test_limit_node.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp
	$(CXX) $(CXXFLAGS) -DSTANDALONE_TEST_LIMIT_NODE tests/test_limit_node.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp -o test_limit_node.exe $(LDFLAGS)
	./test_limit_node.exe

test_avl_tree: tests/test_avl_tree.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp src/AVLTree.cpp
	$(CXX) $(CXXFLAGS) -DSTANDALONE_TEST_AVL_TREE tests/test_avl_tree.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp src/AVLTree.cpp -o test_avl_tree.exe $(LDFLAGS)
	./test_avl_tree.exe

test_hash_map: tests/test_hash_map.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp
	$(CXX) $(CXXFLAGS) -DSTANDALONE_TEST_HASH_MAP tests/test_hash_map.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp -o test_hash_map.exe $(LDFLAGS)
	./test_hash_map.exe

test_book_combined: tests/test_book_combined.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -DSTANDALONE_TEST_BOOK_COMBINED tests/test_book_combined.cpp $(SRC) -o test_book_combined.exe $(LDFLAGS)
	./test_book_combined.exe

clean:
	cmd /c "del /Q *.exe *.o 2>nul || exit 0"
