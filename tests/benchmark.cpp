#include "csv_cases.hpp"
#include "Book.hpp"
#include <algorithm>
#include <chrono>
#include <climits>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <numeric>
#include <random>

namespace {

using Clock = std::chrono::steady_clock;

void check(bool condition, const char* message) {

    if (!condition) {
        throw std::runtime_error(message);
    }
}

int number(const std::string& text) {

    size_t end = 0;
    const int value = std::stoi(text, &end);
    check(end == text.size(), "invalid benchmark integer");
    return value;
}

template <typename Function>
double timed(Function function) {

    const auto start = Clock::now();
    function();
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

struct Timing {
    double fill = 0;
    double verify = 0;
    double drain = 0;
};

int treeHeight(LimitNode* node, LimitNode* parent, long long low, long long high, int& count) {

    if (!node) {
        return 0;
    }
    check(node->price > low && node->price < high && node->parent == parent, "AVL ordering/parent");
    ++count;
    const int left = treeHeight(node->left, node, low, node->price, count);
    const int right = treeHeight(node->right, node, node->price, high, count);
    check(std::abs(left - right) <= 1 && node->height == 1 + std::max(left, right), "AVL balance/height");
    return 1 + std::max(left, right);
}

void verifyTree(AVLTree& tree, int expected) {

    int count = 0;
    const int height = treeHeight(tree.root, nullptr, LLONG_MIN, LLONG_MAX, count);
    check(count == expected && tree.getHeightOfTree() == height, "AVL count/root height");
}

Timing orders(const std::vector<int>& keys) {

    Timing result;
    std::vector<std::unique_ptr<Order>> entries;
    entries.reserve(keys.size());
    result.fill = timed([&]() {

        for (int key : keys) {
            entries.push_back(std::make_unique<Order>(key, key, 1, key, Side::Buy));
        }
    });
    result.verify = timed([&]() {

        for (size_t i = 0; i < keys.size(); ++i) {
            auto& order = *entries[i];
            check(order.id == keys[i] && order.price == keys[i] && order.quantity == 1 &&
                  order.timestamp == keys[i] && order.side == Side::Buy && !order.isAttached() &&
                  !order.prev && !order.next && !order.parentLimit, "Order initial state");
            order.reduceQuantity(1);
            check(order.isFullyFilled(), "Order full fill");
        }
    });
    result.drain = timed([&]() {

        entries.clear();
    });
    return result;
}

Timing queue(const std::vector<int>& keys) {

    Timing result;
    // Precreate caller-owned orders: this measures queue linking, not Order allocation.
    std::vector<std::unique_ptr<Order>> entries;
    entries.reserve(keys.size());
    for (int key : keys) {
        entries.push_back(std::make_unique<Order>(key, 100, 1, key, Side::Buy));
    }
    LimitNode level(100);
    result.fill = timed([&]() {

        for (const auto& order : entries) {
            level.queue.pushBack(order.get(), &level);
        }
    });
    result.verify = timed([&]() {

        check(level.getOrderCount() == static_cast<int>(keys.size()) &&
              level.getTotalVolume() == static_cast<int>(keys.size()), "queue aggregates");
        Order* order = level.queue.head;
        Order* previous = nullptr;
        for (const auto& expected : entries) {
            check(order == expected.get() && order->prev == previous && order->parentLimit == &level &&
                  order->isAttached(), "queue FIFO/link/owner");
            previous = order;
            order = order->next;
        }
        check(!order && previous == level.queue.tail, "queue tail");
    });
    bool correct = true;
    result.drain = timed([&]() {

        for (const auto& order : entries) {
            correct = (level.queue.popFront() == order.get() && !order->isAttached()) && correct;
        }
    });
    check(correct && level.isEmpty() && !level.queue.head && !level.queue.tail &&
          level.getTotalVolume() == 0, "queue drain");
    return result;
}

Timing levels(const std::vector<int>& keys) {

    Timing result;
    std::vector<std::unique_ptr<LimitNode>> entries;
    entries.reserve(keys.size());
    result.fill = timed([&]() {

        for (int key : keys) {
            entries.push_back(std::make_unique<LimitNode>(key));
        }
    });
    result.verify = timed([&]() {

        for (size_t i = 0; i < entries.size(); ++i) {
            auto& node = *entries[i];
            check(node.price == keys[i] && node.height == 1 && !node.parent && !node.left && !node.right &&
                  node.isEmpty() && node.getTotalVolume() == 0 && node.getOrderCount() == 0, "LimitNode initial state");
        }
    });
    result.drain = timed([&]() {

        entries.clear();
    });
    return result;
}

Timing avl(const std::vector<int>& keys) {

    Timing result;
    AVLTree tree;
    std::vector<LimitNode*> nodes;
    nodes.reserve(keys.size());
    result.fill = timed([&]() {

        for (int key : keys) {
            nodes.push_back(tree.insert(key));
        }
    });
    result.verify = timed([&]() {

        verifyTree(tree, static_cast<int>(keys.size()));
        for (size_t i = 0; i < keys.size(); ++i) {
            check(nodes[i]->price == keys[i] && tree.insert(keys[i]) == nodes[i], "AVL duplicate identity");
        }
    });
    result.drain = timed([&]() {

        for (auto* node : nodes) {
            tree.remove(node);
        }
    });
    check(tree.isEmpty(), "AVL drain");
    return result;
}

Timing hash(const std::vector<int>& keys) {

    Timing result;
    HashMap<int, int> map(1);
    result.fill = timed([&]() {

        for (int key : keys) {
            map.insert(key, -key);
        }
    });
    result.verify = timed([&]() {

        check(map.getSize() == static_cast<int>(keys.size()) && map.loadFactor() <= 0.75, "hash size/load");
        for (int key : keys) {
            check(map.contains(key) && map.get(key) == -key, "hash rehash/lookup");
            map.insert(key, key);
            check(map.get(key) == key, "hash overwrite");
        }
        check(map.getSize() == static_cast<int>(keys.size()) && !map.contains(-1), "hash overwrite/miss");
    });
    result.drain = timed([&]() {

        for (int key : keys) {
            map.remove(key);
        }
    });
    check(map.getSize() == 0, "hash drain");
    return result;
}

Timing book(const std::vector<int>& keys, int priceLevels) {

    Timing result;
    Book storage;
    const int n = static_cast<int>(keys.size());
    const int distinct = priceLevels ? std::min(priceLevels, n) : n;
    std::vector<int> counts(static_cast<size_t>(distinct) + 1, 0);
    std::vector<std::vector<int>> fifo(static_cast<size_t>(distinct) + 1);
    // Build the reference outside every measured phase.
    for (int key : keys) {
        const int price = priceLevels ? (key - 1) % distinct + 1 : key;
        ++counts[price];
        fifo[price].push_back(key);
    }
    bool correct = true;
    result.fill = timed([&]() {

        for (int key : keys) {
            const int price = priceLevels ? (key - 1) % distinct + 1 : key;
            auto order = std::make_unique<Order>(key, price, 1, key, Side::Buy);
            if (storage.insertOrder(order.get())) {
                order.release();
            } else {
                correct = false;
            }
        }
    });
    check(correct, "Book fill rejected valid order");
    result.verify = timed([&]() {

        check(storage.orderMap.getSize() == n && storage.buyLimitMap.getSize() == distinct &&
              storage.getBestBid() && storage.getBestBid()->price == distinct && !storage.getBestAsk(), "Book size/best");
        verifyTree(storage.buyTree, distinct);
        for (int price = 1; price <= distinct; ++price) {
            auto* level = storage.buyLimitMap.get(price);
            check(level && level->getOrderCount() == counts[price] &&
                  storage.getVolumeAt(price) == counts[price], "Book level aggregate");
            Order* order = level->queue.head;
            Order* previous = nullptr;
            for (int id : fifo[price]) {
                check(order && order->id == id && storage.getOrder(id) == order && order->price == price &&
                      order->quantity == 1 && order->side == Side::Buy && order->timestamp == id &&
                      order->prev == previous && order->parentLimit == level && order->isAttached(), "Book FIFO/index/fields");
                previous = order;
                order = order->next;
            }
            check(!order && level->queue.tail == previous, "Book queue tail");
        }
    });
    result.drain = timed([&]() {

        for (int key : keys) {
            correct = storage.cancelOrder(key) && correct;
        }
    });
    check(correct && storage.orderMap.getSize() == 0 && storage.buyLimitMap.getSize() == 0 &&
          storage.buyTree.isEmpty() && !storage.getBestBid(), "Book drain/index cleanup");
    return result;
}

Timing run(const std::string& structure, const std::vector<int>& keys, int priceLevels) {

    if (structure == "Order") return orders(keys);
    if (structure == "OrderQueue") return queue(keys);
    if (structure == "LimitNode") return levels(keys);
    if (structure == "AVLTree") return avl(keys);
    if (structure == "HashMap") return hash(keys);
    if (structure == "Book" || structure == "BookShared") return book(keys, priceLevels);
    throw std::runtime_error("unknown benchmark structure: " + structure);
}

double median(std::vector<double> values) {

    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

} // namespace

int main(int argc, char** argv) {

    try {
        check(argc <= 2, "usage: benchmark.exe [output-directory]");
        const std::filesystem::path output = argc == 2 ? argv[1] : "tests/results";
        std::filesystem::create_directories(output);
        std::ofstream raw(output / "benchmark_raw.csv");
        std::ofstream summary(output / "benchmark_summary.csv");
        check(raw.good() && summary.good(), "cannot open benchmark output files");
        raw << "structure,pattern,entries,price_levels,seed,repeat,fill_ms,verify_ms,drain_ms,fill_ns_per_entry,status\n";
        summary << "structure,pattern,entries,price_levels,repeats,median_fill_ms,min_fill_ms,max_fill_ms,median_drain_ms,fill_ns_per_entry,status\n";
        raw << std::fixed << std::setprecision(6);
        summary << std::fixed << std::setprecision(6);
        int cases = 0;
        for (const auto& row : CsvCases::load()) {
            const auto& f = row.fields;
            if (f[0] != "Benchmark") continue;
            ++cases;
            const int repeats = number(f[3]);
            const int priceLevels = number(f[4]);
            const int seed = number(f[7]);
            const int n = number(f[8]);
            check(repeats > 0 && repeats <= 9 && repeats % 2 == 1 && n > 0 && n <= 1000000 &&
                  seed >= 0 && priceLevels >= 0 && f[5] == "1" && f[6] == "Buy" && f[9] == "true", "invalid benchmark configuration");
            check((f[1] == "BookShared") == (priceLevels > 0), "only BookShared uses a price-level cap");
            std::vector<int> keys(static_cast<size_t>(n));
            std::iota(keys.begin(), keys.end(), 1);
            if (f[2] == "descending") {
                std::reverse(keys.begin(), keys.end());
            } else if (f[2] == "shuffled") {
                std::mt19937 random(static_cast<unsigned int>(seed));
                std::shuffle(keys.begin(), keys.end(), random);
            } else {
                check(f[2] == "ascending", "unknown benchmark pattern");
            }
            std::vector<double> fills;
            std::vector<double> drains;
            for (int repeat = 1; repeat <= repeats; ++repeat) {
                const Timing result = run(f[1], keys, priceLevels);
                fills.push_back(result.fill);
                drains.push_back(result.drain);
                raw << f[1] << ',' << f[2] << ',' << n << ',' << priceLevels << ',' << seed << ',' << repeat << ','
                    << result.fill << ',' << result.verify << ',' << result.drain << ',' << result.fill * 1e6 / n << ",PASS\n";
                raw.flush();
            }
            const double fill = median(fills);
            summary << f[1] << ',' << f[2] << ',' << n << ',' << priceLevels << ',' << repeats << ',' << fill << ','
                    << *std::min_element(fills.begin(), fills.end()) << ',' << *std::max_element(fills.begin(), fills.end())
                    << ',' << median(drains) << ',' << fill * 1e6 / n << ",PASS\n";
            summary.flush();
            std::cout << "[PASS] " << f[1] << ' ' << f[2] << " n=" << n << " median fill=" << fill << " ms" << std::endl;
        }
        check(cases > 0, "no Benchmark cases in CSV");
        check(raw.good() && summary.good(), "failed to write benchmark results");
        std::cout << cases << " benchmark configurations passed; results in " << output << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << error.what() << std::endl;
        return 1;
    }
}
