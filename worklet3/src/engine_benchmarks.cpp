#include "Benchmark.hpp"

#include "Book.hpp"
#include "MatchingEngine.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

using worklet3::BenchmarkConfig;
using worklet3::BenchmarkResult;
using worklet3::runBenchmark;

Order makeOrder(int id, int price, int quantity, long long timestamp, Side side) {
    return Order(id, price, quantity, timestamp, side);
}

bool submitLimit(MatchingEngine& engine, int id, int price, int quantity,
                 long long timestamp, Side side) {
    const Order order = makeOrder(id, price, quantity, timestamp, side);
    return engine.submitOrder(order, OrderType::Limit);
}

bool submitMarket(MatchingEngine& engine, int id, int quantity,
                  long long timestamp, Side side) {
    // Market order price is ignored by Worklet 2; zero makes that explicit.
    const Order order = makeOrder(id, 0, quantity, timestamp, side);
    return engine.submitOrder(order, OrderType::Market);
}

BenchmarkConfig configFor(std::size_t n, std::uint32_t seed) {
    BenchmarkConfig config;
    config.operations = n;
    // These workloads mutate the book; use a separate setup for any future warm-up.
    // Keeping warm-up at zero prevents warm-up operations from contaminating correctness checks.
    config.warmupOperations = 0;
    config.seed = seed;
    return config;
}

BenchmarkResult benchmarkInsert(std::size_t n, std::uint32_t seed) {
    Book book;
    MatchingEngine engine(book);
    const auto config = configFor(n, seed);

    auto result = runBenchmark(
        "limit-order insertion",
        config,
        [&](std::size_t i) {
            const int id = static_cast<int>(i + 1);
            // Separate, non-crossing buy levels keep this workload focused on inserts.
            const int price = 100000 - static_cast<int>(i % 50000);
            return submitLimit(engine, id, price, 10, static_cast<long long>(i), Side::Buy);
        },
        [&]() {
            for (std::size_t i = 0; i < n; ++i) {
                if (book.getOrder(static_cast<int>(i + 1)) == nullptr) return false;
            }
            return true;
        },
        "Unique non-crossing limit buys; fixed seed recorded for reproducibility."
    );
    return result;
}

BenchmarkResult benchmarkSuccessfulMatch(std::size_t n, std::uint32_t seed) {
    Book book;
    MatchingEngine engine(book);

    // Seed asks before timing; incoming market buys should execute successfully.
    for (std::size_t i = 0; i < n; ++i) {
        if (!submitLimit(engine, static_cast<int>(i + 1),
                         100 + static_cast<int>(i % 20), 5,
                         static_cast<long long>(i), Side::Sell)) {
            return BenchmarkResult{"successful matching", n, 0, n, 0, 0, {}, false,
                                   "Failed while preparing resting asks."};
        }
    }
    engine.clearTrades();
    const std::size_t expectedTradesAtLeast = n;
    auto result = runBenchmark(
        "successful market matching",
        configFor(n, seed),
        [&](std::size_t i) {
            return submitMarket(engine, static_cast<int>(n + i + 1), 1,
                                static_cast<long long>(n + i), Side::Buy);
        },
        [&]() { return engine.getTradeCount() >= expectedTradesAtLeast; },
        "Resting asks are prepared outside the timed region."
    );
    return result;
}

BenchmarkResult benchmarkPartialFills(std::size_t n, std::uint32_t seed) {
    Book book;
    MatchingEngine engine(book);
    // Each incoming market buy fills only part of a large resting ask.
    if (!submitLimit(engine, 1, 100, static_cast<int>(n * 2 + 100),
                     1, Side::Sell)) {
        return BenchmarkResult{"partial fills", n, 0, n, 0, 0, {}, false,
                               "Could not seed resting ask."};
    }
    engine.clearTrades();
    auto result = runBenchmark(
        "partial fills",
        configFor(n, seed),
        [&](std::size_t i) {
            return submitMarket(engine, static_cast<int>(i + 2), 1,
                                static_cast<long long>(i + 2), Side::Buy);
        },
        [&]() {
            const Order* remaining = book.getOrder(1);
            return remaining != nullptr && remaining->quantity == static_cast<int>(n * 2 + 100 - n)
                   && engine.getTradeCount() == n;
        },
        "Each operation takes one unit from a resting ask with remaining quantity."
    );
    return result;
}

BenchmarkResult benchmarkCancellation(std::size_t n, std::uint32_t seed) {
    Book book;
    MatchingEngine engine(book);

    for (std::size_t i = 0; i < n; ++i) {
        if (!submitLimit(engine, static_cast<int>(i + 1),
                         100000 - static_cast<int>(i % 50000), 10,
                         static_cast<long long>(i), Side::Buy)) {
            return BenchmarkResult{"cancellation", n, 0, n, 0, 0, {}, false,
                                   "Failed while preparing orders."};
        }
    }

    auto result = runBenchmark(
        "order cancellation",
        configFor(n, seed),
        [&](std::size_t i) { return book.cancelOrder(static_cast<int>(i + 1)); },
        [&]() {
            for (std::size_t i = 0; i < n; ++i) {
                if (book.getOrder(static_cast<int>(i + 1)) != nullptr) return false;
            }
            return true;
        },
        "Orders are inserted before timing; cancellation itself is timed."
    );
    return result;
}

BenchmarkResult benchmarkMultiLevel(std::size_t n, std::uint32_t seed) {
    Book book;
    MatchingEngine engine(book);

    const std::size_t levels = 20;
    const int ordersPerLevel = static_cast<int>(std::max<std::size_t>(1, n / levels));
    int nextId = 1;
    for (std::size_t level = 0; level < levels; ++level) {
        for (int j = 0; j < ordersPerLevel; ++j) {
            const int id = nextId++;
            if (!submitLimit(engine, id, 100 + static_cast<int>(level),
                             2, id, Side::Sell)) {
                return BenchmarkResult{"multi-level matching", n, 0, n, 0, 0, {}, false,
                                       "Failed while preparing multi-level asks."};
            }
        }
    }
    engine.clearTrades();

    auto result = runBenchmark(
        "multi-level market matching",
        configFor(n, seed),
        [&](std::size_t i) {
            // This quantity crosses multiple resting orders and often multiple levels.
            return submitMarket(engine, nextId + static_cast<int>(i), 30,
                                static_cast<long long>(i + 100000), Side::Buy);
        },
        [&]() { return engine.getTradeCount() > 0; },
        "Resting asks span 20 price levels; each incoming order requests 30 units."
    );
    return result;
}

BenchmarkResult benchmarkMixed(std::size_t n, std::uint32_t seed) {
    Book book;
    MatchingEngine engine(book);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> action(0, 3);
    std::uniform_int_distribution<int> priceOffset(0, 99);
    std::vector<int> cancellable;
    int nextId = 1;

    // Prepare a small resting spread outside timing to enable matching.
    for (int i = 0; i < 100; ++i) {
        submitLimit(engine, nextId++, 200 + i % 10, 100, i, Side::Sell);
        submitLimit(engine, nextId++, 150 - i % 10, 100, i, Side::Buy);
        cancellable.push_back(nextId - 1);
    }

    auto result = runBenchmark(
        "mixed workload",
        configFor(n, seed),
        [&](std::size_t i) {
            const int choice = action(rng);
            if (choice == 0) {
                const int id = nextId++;
                const int price = 50 + priceOffset(rng);
                const bool ok = submitLimit(engine, id, price, 1,
                                            static_cast<long long>(i + 1000), Side::Buy);
                if (ok) cancellable.push_back(id);
                return ok;
            }
            if (choice == 1) {
                return submitMarket(engine, nextId++, 1,
                                    static_cast<long long>(i + 2000), Side::Buy);
            }
            if (choice == 2 && !cancellable.empty()) {
                const int index = static_cast<int>(rng() % cancellable.size());
                const int id = cancellable[static_cast<std::size_t>(index)];
                cancellable[static_cast<std::size_t>(index)] = cancellable.back();
                cancellable.pop_back();
                return book.cancelOrder(id);
            }
            return submitLimit(engine, nextId++, 250 + priceOffset(rng), 1,
                               static_cast<long long>(i + 3000), Side::Sell);
        },
        [&]() { return true; },
        "Deterministic pseudo-random mix using the configured fixed seed."
    );
    return result;
}

} // namespace

int main(int argc, char** argv) {
    std::size_t operations = 20000;
    std::uint32_t seed = 20261017;

    if (argc >= 2) {
        try {
            operations = static_cast<std::size_t>(std::stoull(argv[1]));
        } catch (...) {
            std::cerr << "Usage: " << argv[0] << " [operations] [seed]\n";
            return 2;
        }
    }
    if (argc >= 3) {
        try {
            seed = static_cast<std::uint32_t>(std::stoul(argv[2]));
        } catch (...) {
            std::cerr << "Usage: " << argv[0] << " [operations] [seed]\n";
            return 2;
        }
    }
    if (operations == 0 || operations > 1000000) {
        std::cerr << "operations must be between 1 and 1000000\n";
        return 2;
    }

    std::vector<BenchmarkResult> results;
    results.push_back(benchmarkInsert(operations, seed));
    results.push_back(benchmarkSuccessfulMatch(operations, seed));
    results.push_back(benchmarkPartialFills(operations, seed));
    results.push_back(benchmarkCancellation(operations, seed));
    results.push_back(benchmarkMultiLevel(operations, seed));
    results.push_back(benchmarkMixed(operations, seed));

    worklet3::printBenchmarkReport(results);

    const bool allPassed = std::all_of(results.begin(), results.end(),
        [](const BenchmarkResult& result) {
            return result.correctnessPassed && result.failed == 0;
        });
    return allPassed ? 0 : 1;
}