#include "Benchmark.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

void runBenchmarkTests() {
    using worklet3::calculateLatencyStats;
    using worklet3::percentileNearestRank;

    assert(percentileNearestRank({}, 50.0) == 0.0);
    assert(percentileNearestRank({9.0, 1.0, 5.0, 3.0, 7.0}, 50.0) == 5.0);
    assert(percentileNearestRank({9.0, 1.0, 5.0, 3.0, 7.0}, 95.0) == 9.0);
    assert(percentileNearestRank({9.0, 1.0, 5.0, 3.0, 7.0}, 100.0) == 9.0);
    assert(percentileNearestRank({9.0, 1.0, 5.0, 3.0, 7.0}, 0.0) == 1.0);

    bool threw = false;
    try {
        (void)percentileNearestRank({1.0}, 101.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    const auto empty = calculateLatencyStats({});
    assert(empty.sampleCount == 0);
    assert(empty.averageNs == 0.0);

    const auto stats = calculateLatencyStats({1.0, 2.0, 3.0, 4.0, 100.0});
    assert(stats.sampleCount == 5);
    assert(std::fabs(stats.averageNs - 22.0) < 0.0001);
    assert(stats.p50Ns == 3.0);
    assert(stats.p95Ns == 100.0);
    assert(stats.p99Ns == 100.0);
    assert(stats.maxNs == 100.0);

    worklet3::BenchmarkConfig config;
    config.operations = 5;
    config.warmupOperations = 0;
    const auto result = worklet3::runBenchmark(
        "test",
        config,
        [](std::size_t i) { return i % 2 == 0; },
        []() { return true; }
    );
    assert(result.attempted == 5);
    assert(result.succeeded == 3);
    assert(result.failed == 2);
    assert(result.latency.sampleCount == 5);
    assert(result.correctnessPassed);

    std::cout << "All Worklet 3 benchmark utility tests passed.\n";
}
