#include "Benchmark.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace worklet3 {

double percentileNearestRank(std::vector<double> samples, double percentile) {
    if (percentile < 0.0 || percentile > 100.0) {
        throw std::invalid_argument("percentile must be between 0 and 100");
    }
    if (samples.empty()) {
        return 0.0;
    }

    std::sort(samples.begin(), samples.end());

    if (percentile == 0.0) {
        return samples.front();
    }

    // Nearest-rank definition: rank = ceil(p * N), using one-based ranks.
    const double rankValue = (percentile / 100.0) * samples.size();
    std::size_t rank = static_cast<std::size_t>(rankValue);
    if (static_cast<double>(rank) < rankValue) {
        ++rank;
    }
    if (rank == 0) {
        rank = 1;
    }
    if (rank > samples.size()) {
        rank = samples.size();
    }
    return samples[rank - 1];
}

LatencyStats calculateLatencyStats(const std::vector<double>& samples) {
    LatencyStats stats;
    stats.sampleCount = samples.size();
    if (samples.empty()) {
        return stats;
    }

    stats.averageNs = std::accumulate(samples.begin(), samples.end(), 0.0)
                    / static_cast<double>(samples.size());
    stats.p50Ns = percentileNearestRank(samples, 50.0);
    stats.p95Ns = percentileNearestRank(samples, 95.0);
    stats.p99Ns = percentileNearestRank(samples, 99.0);
    stats.maxNs = *std::max_element(samples.begin(), samples.end());
    return stats;
}

BenchmarkResult runBenchmark(
    const std::string& name,
    const BenchmarkConfig& config,
    const Operation& operation,
    const std::function<bool()>& correctnessCheck,
    const std::string& notes
) {
    if (!operation) {
        throw std::invalid_argument("benchmark operation must be callable");
    }
    if (config.operations == 0) {
        throw std::invalid_argument("benchmark operations must be greater than zero");
    }

    BenchmarkResult result;
    result.name = name;
    result.attempted = config.operations;
    result.notes = notes;

    // Warm-up operations are deliberately not timed or included in counters.
    for (std::size_t i = 0; i < config.warmupOperations; ++i) {
        (void)operation(i);
    }

    std::vector<double> latencySamples;
    latencySamples.reserve(config.operations);

    const auto wallStart = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < config.operations; ++i) {
        const auto start = std::chrono::steady_clock::now();
        const bool success = operation(i);
        const auto stop = std::chrono::steady_clock::now();

        latencySamples.push_back(
            std::chrono::duration<double, std::nano>(stop - start).count()
        );

        if (success) {
            ++result.succeeded;
        } else {
            ++result.failed;
        }
    }
    const auto wallStop = std::chrono::steady_clock::now();

    result.elapsedSeconds =
        std::chrono::duration<double>(wallStop - wallStart).count();
    result.throughputOpsPerSecond =
        result.elapsedSeconds > 0.0
            ? static_cast<double>(result.succeeded) / result.elapsedSeconds
            : 0.0;
    result.latency = calculateLatencyStats(latencySamples);
    result.correctnessPassed = correctnessCheck ? correctnessCheck() : true;
    return result;
}

void printBenchmarkResult(const BenchmarkResult& result) {
    const auto us = [](double ns) { return ns / 1000.0; };

    std::cout << "\n[" << result.name << "]\n"
              << "  attempted: " << result.attempted
              << ", succeeded: " << result.succeeded
              << ", failed: " << result.failed << '\n'
              << std::fixed << std::setprecision(3)
              << "  elapsed: " << result.elapsedSeconds << " s\n"
              << "  throughput: " << result.throughputOpsPerSecond
              << " operations/s\n"
              << "  latency (microseconds): avg=" << us(result.latency.averageNs)
              << ", p50=" << us(result.latency.p50Ns)
              << ", p95=" << us(result.latency.p95Ns)
              << ", p99=" << us(result.latency.p99Ns)
              << ", max=" << us(result.latency.maxNs) << '\n'
              << "  correctness: " << (result.correctnessPassed ? "PASS" : "FAIL")
              << '\n';

    if (!result.notes.empty()) {
        std::cout << "  notes: " << result.notes << '\n';
    }
}

void printBenchmarkReport(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n========== Worklet 3 Benchmark Report ==========\n";
    for (const auto& result : results) {
        printBenchmarkResult(result);
    }
    std::cout << "\nTargets (evaluate on your own machine/build):\n"
              << "  p99 latency <= 20 microseconds\n"
              << "  throughput >= 200000 limit orders/second\n"
              << "Do not treat these targets as measured results.\n";
}

} // namespace worklet3