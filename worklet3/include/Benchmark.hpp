#ifndef WORKLET3_BENCHMARK_HPP
#define WORKLET3_BENCHMARK_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace worklet3 {

struct LatencyStats {
    std::size_t sampleCount = 0;
    double averageNs = 0.0;
    double p50Ns = 0.0;
    double p95Ns = 0.0;
    double p99Ns = 0.0;
    double maxNs = 0.0;
};

struct BenchmarkResult {
    std::string name;
    std::size_t attempted = 0;
    std::size_t succeeded = 0;
    std::size_t failed = 0;
    double elapsedSeconds = 0.0;
    double throughputOpsPerSecond = 0.0;
    LatencyStats latency;
    bool correctnessPassed = true;
    std::string notes;
};

struct BenchmarkConfig {
    std::size_t operations = 100000;
    std::size_t warmupOperations = 1000;
    std::uint32_t seed = 20261017;
};

double percentileNearestRank(std::vector<double> samples, double percentile);
LatencyStats calculateLatencyStats(const std::vector<double>& samples);

using Operation = std::function<bool(std::size_t)>;

BenchmarkResult runBenchmark(
    const std::string& name,
    const BenchmarkConfig& config,
    const Operation& operation,
    const std::function<bool()>& correctnessCheck = {},
    const std::string& notes = ""
);

void printBenchmarkResult(const BenchmarkResult& result);
void printBenchmarkReport(const std::vector<BenchmarkResult>& results);

} // namespace worklet3

#endif