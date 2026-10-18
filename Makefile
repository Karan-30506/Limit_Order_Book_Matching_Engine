CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude -Itests -O2

SRC = src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp src/AVLTree.cpp src/Book.cpp
TEST_SRC = tests/test_order.cpp tests/test_order_queue.cpp tests/test_limit_node.cpp \
           tests/test_avl_tree.cpp tests/test_hash_map.cpp tests/test_book_combined.cpp \
           tests/main_test_runner.cpp

ALL_TEST_EXE = run_all_tests.exe

.PHONY: all test clean test_order test_order_queue test_limit_node test_avl_tree test_hash_map test_book_combined

all: $(ALL_TEST_EXE)

$(ALL_TEST_EXE): $(SRC) $(TEST_SRC)
	$(CXX) $(CXXFLAGS) $(SRC) $(TEST_SRC) -o $(ALL_TEST_EXE)

test: $(ALL_TEST_EXE)
	./$(ALL_TEST_EXE)

test_order: tests/test_order.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp
	$(CXX) $(CXXFLAGS) -DSTANDALONE_TEST_ORDER tests/test_order.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp -o test_order.exe
	./test_order.exe

test_order_queue: tests/test_order_queue.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp
	$(CXX) $(CXXFLAGS) -DSTANDALONE_TEST_ORDER_QUEUE tests/test_order_queue.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp -o test_order_queue.exe
	./test_order_queue.exe

test_limit_node: tests/test_limit_node.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp
	$(CXX) $(CXXFLAGS) -DSTANDALONE_TEST_LIMIT_NODE tests/test_limit_node.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp -o test_limit_node.exe
	./test_limit_node.exe

test_avl_tree: tests/test_avl_tree.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp src/AVLTree.cpp
	$(CXX) $(CXXFLAGS) -DSTANDALONE_TEST_AVL_TREE tests/test_avl_tree.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp src/AVLTree.cpp -o test_avl_tree.exe
	./test_avl_tree.exe

test_hash_map: tests/test_hash_map.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp
	$(CXX) $(CXXFLAGS) -DSTANDALONE_TEST_HASH_MAP tests/test_hash_map.cpp src/Order.cpp src/OrderQueue.cpp src/LimitNode.cpp -o test_hash_map.exe
	./test_hash_map.exe

test_book_combined: tests/test_book_combined.cpp $(SRC)
	$(CXX) $(CXXFLAGS) -DSTANDALONE_TEST_BOOK_COMBINED tests/test_book_combined.cpp $(SRC) -o test_book_combined.exe
	./test_book_combined.exe

clean:
	cmd /c "del /Q *.exe *.o 2>nul || exit 0"
