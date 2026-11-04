#include "csv_cases.hpp"
#include "Book.hpp"
#include <algorithm>
#include <climits>
#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <tuple>

namespace TestCsv {

void require(bool condition, const std::string& message) {

    if (!condition) {
        throw std::runtime_error(message);
    }
}

int integer(const std::string& value) {

    size_t end = 0;
    const int result = std::stoi(value, &end);
    require(end == value.size(), "invalid integer: " + value);
    return result;
}

struct Event {
    std::string operation;
    int id;
    int price;
    int quantity;
    Side side;
    std::string expected;
};

struct Record {
    int price;
    int quantity;
    Side side;
    std::uint64_t priority;
};

using Model = std::map<int, Record>;
using LevelKey = std::pair<Side, int>;

// Independent instances exercise the primitives directly, outside Book's implementation.
struct Primitives {
    std::map<int, std::unique_ptr<Order>> orders;
    AVLTree bids;
    AVLTree asks;
    HashMap<int, Order*> index{1};
    std::map<LevelKey, LimitNode*> levels;

    LimitNode* level(Side side, int price) {

        const LevelKey key{side, price};
        auto found = levels.find(key);
        if (found != levels.end()) {
            return found->second;
        }
        auto* node = (side == Side::Buy ? bids : asks).insert(price);
        levels[key] = node;
        return node;
    }

    void prune(Side side, int price) {

        auto found = levels.find({side, price});
        if (found != levels.end() && found->second->isEmpty()) {
            (side == Side::Buy ? bids : asks).remove(found->second);
            levels.erase(found);
        }
    }

    void apply(const Event& event, int target) {

        if (event.operation == "insert") {
            auto order = std::make_unique<Order>(event.id, event.price, event.quantity, 0, event.side);
            auto* node = level(event.side, event.price);
            node->queue.pushBack(order.get(), node);
            index.insert(event.id, order.get());
            orders[event.id] = std::move(order);
            return;
        }
        Order* order = index.get(target);
        const Side side = order->side;
        const int oldPrice = order->price;
        if (event.operation == "reduce") {
            // Alternate both public reduction paths to catch double aggregate accounting.
            if (order == order->parentLimit->queue.head && event.id % 2 == 0) {
                order->parentLimit->queue.reduceQuantity(event.quantity);
            } else {
                order->reduceQuantity(event.quantity);
            }
        } else if (event.operation == "modify") {
            order->parentLimit->queue.remove(order);
            order->price = event.price;
            order->quantity = event.quantity;
            auto* node = level(side, event.price);
            node->queue.pushBack(order, node);
            prune(side, oldPrice);
        } else {
            if (event.operation == "pop") {
                require(order->parentLimit->queue.popFront() == order, "primitive FIFO pop");
            } else {
                order->parentLimit->queue.remove(order);
            }
            require(!order->isAttached() && !order->prev && !order->next && !order->parentLimit,
                    "primitive detach state");
            index.remove(target);
            orders.erase(target);
            prune(side, oldPrice);
        }
    }
};

int verifyTree(LimitNode* node, LimitNode* parent, long long low, long long high,
               std::map<int, LimitNode*>& nodes) {

    if (!node) {
        return 0;
    }
    require(node->price > low && node->price < high, "global BST ordering");
    require(node->parent == parent, "AVL parent link");
    require(nodes.emplace(node->price, node).second, "duplicate/cyclic AVL node");
    const int left = verifyTree(node->left, node, low, node->price, nodes);
    const int right = verifyTree(node->right, node, node->price, high, nodes);
    const int height = 1 + std::max(left, right);
    require(node->height == height && std::abs(left - right) <= 1, "AVL height/balance");
    return height;
}

void verify(Book& book, Primitives& primitives, const Model& model) {

    std::map<LevelKey, std::vector<int>> expected;
    std::map<int, long long> totalVolumes;
    for (const auto& item : model) {
        expected[{item.second.side, item.second.price}].push_back(item.first);
        totalVolumes[item.second.price] += item.second.quantity;
    }
    for (auto& item : expected) {
        std::sort(item.second.begin(), item.second.end(), [&](int a, int b) {

            return model.at(a).priority < model.at(b).priority;
        });
    }
    require(book.orderMap.getSize() == static_cast<int>(model.size()), "Book order map size");
    require(primitives.index.getSize() == static_cast<int>(model.size()), "primitive hash size");
    require(primitives.levels.size() == expected.size(), "primitive level count");
    for (Side side : {Side::Buy, Side::Sell}) {
        AVLTree& bookTree = side == Side::Buy ? book.buyTree : book.sellTree;
        AVLTree& primitiveTree = side == Side::Buy ? primitives.bids : primitives.asks;
        auto& levelMap = side == Side::Buy ? book.buyLimitMap : book.sellLimitMap;
        std::map<int, LimitNode*> bookNodes;
        std::map<int, LimitNode*> primitiveNodes;
        verifyTree(bookTree.root, nullptr, LLONG_MIN, LLONG_MAX, bookNodes);
        verifyTree(primitiveTree.root, nullptr, LLONG_MIN, LLONG_MAX, primitiveNodes);
        size_t count = 0;
        for (const auto& item : expected) {
            if (item.first.first != side) {
                continue;
            }
            ++count;
            const int price = item.first.second;
            require(bookNodes.count(price) && primitiveNodes.count(price), "missing AVL level");
            auto* actualLevel = levelMap.get(price);
            require(actualLevel == bookNodes.at(price), "Book map/tree identity");
            require(primitives.levels.at(item.first) == primitiveNodes.at(price), "primitive node identity");
            long long volume = 0;
            for (int id : item.second) {
                volume += model.at(id).quantity;
            }
            for (auto* level : {actualLevel, primitiveNodes.at(price)}) {
                require(level->getOrderCount() == static_cast<int>(item.second.size()), "queue count");
                require(level->getTotalVolume() == volume && !level->isEmpty(), "level volume/emptiness");
                Order* order = level->queue.head;
                Order* previous = nullptr;
                for (int id : item.second) {
                    require(order && order->id == id && order->prev == previous, "FIFO/previous link");
                    const auto& record = model.at(id);
                    require(order->price == price && order->side == side && order->quantity == record.quantity,
                            "Order fields");
                    require(order->isFullyFilled() == (record.quantity == 0), "filled status");
                    require(order->parentLimit == level && order->isAttached(), "Order attachment");
                    require((level == actualLevel ? book.getOrder(id) : primitives.index.get(id)) == order,
                            "hash order identity");
                    previous = order;
                    order = order->next;
                }
                require(!order && previous == level->queue.tail, "queue tail/termination");
            }
            require(book.getVolumeAt(price, side) == volume, "Book side volume");
        }
        require(bookNodes.size() == count && primitiveNodes.size() == count &&
                levelMap.getSize() == static_cast<int>(count), "stale price level");
        LimitNode* best = side == Side::Buy ? book.getBestBid() : book.getBestAsk();
        LimitNode* expectedBest = bookNodes.empty() ? nullptr :
            (side == Side::Buy ? bookNodes.rbegin()->second : bookNodes.begin()->second);
        require(best == expectedBest, "best price cache");
        size_t visited = 0;
        for (auto* node = best; node; node = book.getNextLevel(node)) {
            require(++visited <= count, "cyclic price traversal");
            auto it = bookNodes.find(node->price);
            auto* next = side == Side::Buy ? (it == bookNodes.begin() ? nullptr : std::prev(it)->second) :
                (std::next(it) == bookNodes.end() ? nullptr : std::next(it)->second);
            require(book.getNextLevel(node) == next, "next worse price");
        }
        require(visited == count, "incomplete price traversal");
    }
    for (const auto& item : totalVolumes) {
        const int price = item.first;
        const long long total = item.second;
        if (total > INT_MAX) {
            bool threw = false;
            try {
                book.getVolumeAt(price);
            } catch (const std::overflow_error&) {

                threw = true;
            }
            require(threw, "combined volume overflow must throw");
        } else {
            require(book.getVolumeAt(price) == total, "combined volume");
        }
    }
}

void execute(const Event& event, Book& book, Primitives& primitives, Model& model,
             std::uint64_t& clock, bool inspect = true) {

    auto found = model.find(event.id);
    int target = event.id;
    if (event.operation == "pop") {
        found = model.end();
        for (auto it = model.begin(); it != model.end(); ++it) {
            if (it->second.side == event.side && it->second.price == event.price &&
                (found == model.end() || it->second.priority < found->second.priority)) {

                found = it;
            }
        }
        if (found != model.end()) {
            target = found->first;
        }
    }
    bool valid = false;
    if (event.operation == "insert" || event.operation == "modify") {
        valid = event.price > 0 && event.quantity > 0 &&
            (event.operation == "insert" ? found == model.end() : found != model.end());
        const Side side = event.operation == "insert" || found == model.end() ? event.side : found->second.side;
        long long volume = event.quantity;
        for (const auto& item : model) {
            if (item.second.side == side && item.second.price == event.price &&
                !(event.operation == "modify" && item.first == target)) {

                volume += item.second.quantity;
            }
        }
        valid = valid && volume <= INT_MAX;
    } else if (event.operation == "reduce") {
        valid = found != model.end() && event.quantity >= 0 && event.quantity <= found->second.quantity;
    } else if (event.operation == "cancel" || event.operation == "pop") {
        valid = found != model.end();
    } else {
        throw std::runtime_error("unknown operation: " + event.operation);
    }
    require(event.expected == "auto" || event.expected == (valid ? "true" : "false"),
            "CSV expectation disagrees with independent model");
    bool actual = false;
    if (event.operation == "insert") {
        auto order = std::make_unique<Order>(event.id, event.price, event.quantity, 0, event.side);
        actual = book.insertOrder(order.get());
        if (actual) {
            order.release();
        }
    } else if (event.operation == "modify") {
        actual = book.modifyOrder(event.id, event.price, event.quantity);
    } else if (event.operation == "cancel") {
        actual = book.cancelOrder(event.id);
    } else if (event.operation == "pop") {
        auto* level = (event.side == Side::Buy ? book.buyLimitMap : book.sellLimitMap).get(event.price);
        std::unique_ptr<Order> order(book.popHeadOrder(level));
        actual = order != nullptr;
        if (order) {
            require(order->id == target && !order->isAttached() && !order->prev &&
                    !order->next && !order->parentLimit, "Book pop ownership/FIFO");
        }
    } else if (auto* order = book.getOrder(event.id)) {
        try {
            order->reduceQuantity(event.quantity);
            actual = true;
        } catch (const std::invalid_argument&) {

            actual = false;
        }
    }
    require(actual == valid, "operation result differs from model");
    if (!valid && event.operation == "reduce" && primitives.index.contains(target)) {
        bool threw = false;
        try {
            primitives.index.get(target)->reduceQuantity(event.quantity);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        require(threw, "primitive must reject invalid reduction");
    }
    if (valid) {
        primitives.apply(event, target);
        if (event.operation == "insert") {
            model[event.id] = {event.price, event.quantity, event.side, ++clock};
        } else if (event.operation == "modify") {
            auto& record = model.at(target);
            record.price = event.price;
            record.quantity = event.quantity;
            record.priority = ++clock;
        } else if (event.operation == "reduce") {
            model.at(target).quantity -= event.quantity;
        } else {
            model.erase(target);
        }
    }
    if (event.operation == "cancel" || (event.operation == "pop" && valid)) {
        require(!book.getOrder(target) && !primitives.index.contains(target), "removed order still indexed");
    }
    if (inspect) {
        verify(book, primitives, model);
    }
}

void scenario(const std::vector<CsvCases::Row>& rows) {

    Book book;
    Primitives primitives;
    Model model;
    std::uint64_t clock = 0;
    for (const auto& row : rows) {
        const auto& f = row.fields;
        try {
            if (f[2] == "stress" || f[2] == "stress_checkpoint") {
                const int seed = integer(f[7]);
                const int steps = integer(f[8]);
                const int interval = f[2] == "stress_checkpoint" ? integer(f[4]) : 1;
                require(seed >= 0 && steps > 0, "invalid stress seed/steps");
                require(interval > 0, "invalid stress checkpoint interval");
                std::mt19937 random(static_cast<unsigned int>(seed));
                for (int step = 0; step < steps; ++step) {
                    const char* operations[] = {"insert", "insert", "modify", "cancel", "reduce", "pop"};
                    Event event{operations[random() % 6], static_cast<int>(random() % 600) - 50,
                                static_cast<int>(random() % 80) - 2, static_cast<int>(random() % 150) - 2,
                                random() % 2 ? Side::Buy : Side::Sell, "auto"};
                    try {
                        // Return values are checked on every operation; full scans are checkpointed
                        // for the million-operation run to avoid timing the verifier repeatedly.
                        execute(event, book, primitives, model, clock,
                                (step + 1) % interval == 0 || step + 1 == steps);
                    } catch (const std::exception& error) {

                        throw std::runtime_error("seed " + f[7] + " step " + std::to_string(step) +
                                                 " " + event.operation + ": " + error.what());
                    }
                }
            } else {
                require(f[6] == "Buy" || f[6] == "Sell", "invalid side");
                execute({f[2], integer(f[3]), integer(f[4]), integer(f[5]),
                         f[6] == "Buy" ? Side::Buy : Side::Sell, f[9]},
                        book, primitives, model, clock);
            }
        } catch (const std::exception& error) {

            throw std::runtime_error("CSV line " + std::to_string(row.line) + ": " + error.what());
        }
    }
    // Drain every surviving order, checking deletion rebalancing and empty-state cleanup.
    while (!model.empty()) {
        execute({"cancel", model.begin()->first, 0, 0, Side::Buy, "true"},
                book, primitives, model, clock);
    }
}

bool runAll(TestFramework::TestSuite& suite) {

    suite.runTest("Shared CSV: schema and scenarios", [&]() {

        std::map<std::string, std::vector<CsvCases::Row>> scenarios;
        for (const auto& row : CsvCases::load()) {
            if (row.fields[0] == "Shared") {
                scenarios[row.fields[1]].push_back(row);
            }
        }
        require(!scenarios.empty(), "CSV must contain shared scenarios");
        for (const auto& item : scenarios) {
            suite.runTest("Shared CSV: " + item.first, [&]() {

                scenario(item.second);
            });
        }
    });
    return suite.allPassed();
}

} // namespace TestCsv
