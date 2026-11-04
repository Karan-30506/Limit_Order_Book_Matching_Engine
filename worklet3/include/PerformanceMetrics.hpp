#ifndef PERFORMANCE_METRICS_HPP
#define PERFORMANCE_METRICS_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct OrderLatency {
    std::size_t requestIndex;
    int orderID;
    bool accepted;
    std::string reason;
    std::uint64_t latencyNanoseconds;
};

struct PerformanceSummary {
    std::size_t orderCount = 0;
    std::size_t acceptedCount = 0;
    std::size_t rejectedCount = 0;
    std::uint64_t totalDurationNanoseconds = 0;
    double meanLatencyNanoseconds = 0.0;
    std::uint64_t p50LatencyNanoseconds = 0;
    std::uint64_t p95LatencyNanoseconds = 0;
    std::uint64_t p99LatencyNanoseconds = 0;
    std::uint64_t maxLatencyNanoseconds = 0;
    double ordersPerSecond = 0.0;
    double tradesPerSecond = 0.0;
};

class PerformanceMetrics {
public:
    void recordOrder(const OrderLatency& latency);
    const std::vector<OrderLatency>& getOrderLatencies() const;

    PerformanceSummary summarize(
        std::uint64_t totalDurationNanoseconds,
        std::size_t tradeCount
    ) const;

    bool writeCsv(const std::string& fileName, std::string& error) const;

private:
    std::vector<OrderLatency> orderLatencies_;
};

#endif
