#include "PerformanceMetrics.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>

namespace {
std::uint64_t percentile(const std::vector<std::uint64_t>& sorted, double p) {
    if (sorted.empty()) {
        return 0;
    }
    const std::size_t index = static_cast<std::size_t>(
        std::ceil(p * static_cast<double>(sorted.size())) - 1.0);
    return sorted[std::min(index, sorted.size() - 1)];
}

std::string csvField(const std::string& value) {
    std::string escaped = "\"";
    for (char ch : value) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped += ch;
        }
    }
    escaped += '"';
    return escaped;
}
}

void PerformanceMetrics::recordOrder(const OrderLatency& latency) {
    orderLatencies_.push_back(latency);
}

const std::vector<OrderLatency>& PerformanceMetrics::getOrderLatencies() const {
    return orderLatencies_;
}

PerformanceSummary PerformanceMetrics::summarize(
    std::uint64_t totalDurationNanoseconds,
    std::size_t tradeCount
) const {
    PerformanceSummary result;
    result.orderCount = orderLatencies_.size();
    result.totalDurationNanoseconds = totalDurationNanoseconds;

    std::vector<std::uint64_t> durations;
    durations.reserve(orderLatencies_.size());
    std::uint64_t totalLatency = 0;
    for (const OrderLatency& latency : orderLatencies_) {
        durations.push_back(latency.latencyNanoseconds);
        totalLatency += latency.latencyNanoseconds;
        if (latency.accepted) {
            ++result.acceptedCount;
        } else {
            ++result.rejectedCount;
        }
    }

    std::sort(durations.begin(), durations.end());
    if (!durations.empty()) {
        result.meanLatencyNanoseconds =
            static_cast<double>(totalLatency) / durations.size();
        result.p50LatencyNanoseconds = percentile(durations, 0.50);
        result.p95LatencyNanoseconds = percentile(durations, 0.95);
        result.p99LatencyNanoseconds = percentile(durations, 0.99);
        result.maxLatencyNanoseconds = durations.back();
    }

    if (totalDurationNanoseconds > 0) {
        const double seconds =
            static_cast<double>(totalDurationNanoseconds) / 1'000'000'000.0;
        result.ordersPerSecond = result.orderCount / seconds;
        result.tradesPerSecond = tradeCount / seconds;
    }
    return result;
}

bool PerformanceMetrics::writeCsv(
    const std::string& fileName,
    std::string& error
) const {
    std::ofstream file(fileName.c_str(), std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        error = "Could not open latency output file: " + fileName;
        return false;
    }

    file << "request_index,order_id,accepted,reason,latency_ns,latency_us\n";
    file << std::fixed << std::setprecision(3);
    for (const OrderLatency& latency : orderLatencies_) {
        file << latency.requestIndex << ','
             << latency.orderID << ','
             << (latency.accepted ? "true" : "false") << ','
             << csvField(latency.reason) << ','
             << latency.latencyNanoseconds << ','
             << static_cast<double>(latency.latencyNanoseconds) / 1000.0
             << '\n';
    }
    file.flush();
    if (!file.good()) {
        error = "Failed while writing latency output file: " + fileName;
        return false;
    }
    error.clear();
    return true;
}
