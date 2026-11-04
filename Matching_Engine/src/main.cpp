#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Book.hpp"
#include "MatchingEngine.hpp"
#include "MarketDataFeed.hpp"
#include "Order.hpp"
#include "OrderParser.hpp"

namespace EngineOnly {
namespace {
using Clock = std::chrono::steady_clock;

struct OrderResult {
    std::size_t requestIndex;
    int orderID;
    bool accepted;
    std::string reason;
    std::uint64_t latencyNanoseconds;
    std::size_t firstTrade;
    std::size_t lastTrade;
    bool restingInBook;
};

struct LatencySummary {
    double mean = 0.0;
    std::uint64_t p50 = 0;
    std::uint64_t p95 = 0;
    std::uint64_t p99 = 0;
    std::uint64_t maximum = 0;
};

std::string getBaseName(const std::string& path) {
    const std::size_t separator = path.find_last_of("/\\");
    std::string filename = separator == std::string::npos
        ? path
        : path.substr(separator + 1);
    const std::size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos && dot != 0) {
        filename = filename.substr(0, dot);
    }
    return filename;
}

std::string currentDateTime() {
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

const char* sideName(Side side) {
    return side == Side::Buy ? "BUY" : side == Side::Sell ? "SELL" : "INVALID";
}

const char* typeName(OrderType type) {
    return type == OrderType::Limit ? "LIMIT" : type == OrderType::Market ? "MARKET" : "INVALID";
}

std::string summaryField(const std::string& label, const std::string& value) {
    std::ostringstream line;
    line << "  " << std::left << std::setw(35) << label << " : " << value;
    return line.str();
}

std::string formatRate(double value) {
    std::ostringstream formatted;
    formatted << std::fixed << std::setprecision(2) << value;
    return formatted.str();
}

std::string formatSeconds(double value) {
    std::ostringstream formatted;
    formatted << std::fixed << std::setprecision(9) << value;
    return formatted.str();
}

void printOrder(std::ostream& output, const OrderRequest& request,
                const std::vector<Trade>& trades, std::size_t firstTrade,
                std::size_t lastTrade,
                const SubmissionResult& result, bool restingInBook) {
    int executedQuantity = 0;
    output << "\n+--------------------------------------------------+\n"
           << "| ORDER #" << request.id << '\n'
           << "+--------------------------------------------------+\n"
           << "  REQUEST\n"
           << "    Type / side       : " << typeName(request.type)
           << " / " << sideName(request.side) << '\n';
    if (request.type == OrderType::Limit) {
        output << "    Limit price       : " << request.price << '\n';
    }
    output << "    Requested quantity: " << request.quantity
           << "\n\n  TRADES (" << (lastTrade - firstTrade) << ")\n";
    if (firstTrade == lastTrade) {
        output << "    No trades executed.\n";
    }
    for (std::size_t i = firstTrade; i < lastTrade; ++i) {
        const Trade& trade = trades[i];
        executedQuantity += trade.quantity;
        output << "    #" << trade.tradeID
               << " | BUY #" << trade.buyOrderID
               << " / SELL #" << trade.sellOrderID
               << " | price " << trade.price
               << " | qty " << trade.quantity << '\n';
    }
    const int remainingQuantity = request.quantity - executedQuantity;
    output << "\n  OUTCOME\n";
    if (!result.success && executedQuantity == 0 && !restingInBook) {
        output << "    Status             : REJECTED\n"
               << "    Reason             : " << result.reason << '\n';
    } else if (executedQuantity >= request.quantity) {
        output << "    Status             : FULLY FILLED\n";
    } else if (request.type == OrderType::Market) {
        output << "    Status             : "
               << (executedQuantity > 0 ? "PARTIALLY EXECUTED\n" : "NOT EXECUTED\n")
               << "    Executed quantity  : " << executedQuantity << '\n'
               << "    Unfilled discarded : " << remainingQuantity << '\n'
               << "    Resting in book    : No\n";
    } else if (restingInBook) {
        output << "    Status             : "
               << (executedQuantity > 0 ? "PARTIALLY FILLED\n" : "OPEN (NOT YET FILLED)\n")
               << "    Executed quantity  : " << executedQuantity << '\n'
               << "    Remaining quantity: " << remainingQuantity << '\n'
               << "    Resting in book    : Yes\n";
    } else {
        output << "    Status             : NOT FULLY EXECUTED\n"
               << "    Executed quantity  : " << executedQuantity << '\n'
               << "    Remaining quantity: " << remainingQuantity << '\n'
               << "    Resting in book    : No\n";
    }
    output << "+--------------------------------------------------+\n";
}

std::uint64_t percentile(const std::vector<std::uint64_t>& sorted, double p) {
    if (sorted.empty()) {
        return 0;
    }
    const std::size_t index = static_cast<std::size_t>(
        std::ceil(p * static_cast<double>(sorted.size())) - 1.0);
    return sorted[std::min(index, sorted.size() - 1)];
}

LatencySummary summarizeLatencies(const std::vector<OrderResult>& results) {
    LatencySummary summary;
    if (results.empty()) {
        return summary;
    }
    std::vector<std::uint64_t> durations;
    durations.reserve(results.size());
    std::uint64_t total = 0;
    for (const OrderResult& result : results) {
        durations.push_back(result.latencyNanoseconds);
        total += result.latencyNanoseconds;
    }
    std::sort(durations.begin(), durations.end());
    summary.mean = static_cast<double>(total) / static_cast<double>(durations.size());
    summary.p50 = percentile(durations, 0.50);
    summary.p95 = percentile(durations, 0.95);
    summary.p99 = percentile(durations, 0.99);
    summary.maximum = durations.back();
    return summary;
}

bool writeLatencyCsv(const std::string& path,
                     const std::vector<OrderResult>& results,
                     std::string& error) {
    std::ofstream output(path.c_str(), std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        error = "Could not open latency output file: " + path;
        return false;
    }
    output << "request_index,order_id,accepted,reason,latency_ns,latency_us\n"
           << std::fixed << std::setprecision(3);
    for (const OrderResult& result : results) {
        std::string escaped = "\"";
        for (char ch : result.reason) {
            if (ch == '"') escaped += "\"\"";
            else escaped += ch;
        }
        escaped += '"';
        output << result.requestIndex << ',' << result.orderID << ','
               << (result.accepted ? "true" : "false") << ','
               << escaped << ',' << result.latencyNanoseconds << ','
               << static_cast<double>(result.latencyNanoseconds) / 1000.0 << '\n';
    }
    output.flush();
    if (!output.good()) {
        error = "Failed while writing latency output file: " + path;
        return false;
    }
    error.clear();
    return true;
}

int runEngineOnly(const std::string& inputPath) {
    std::error_code directoryError;
    std::filesystem::create_directories("log", directoryError);
    if (directoryError) {
        std::cerr << "Error creating log directory: " << directoryError.message() << '\n';
        return 1;
    }

    const std::string base = getBaseName(inputPath);
    const std::string logPath = "log/" + base + "_logs.log";
    const std::string rejectPath = "log/" + base + "_reject.log";
    const std::string latencyPath = "log/" + base + "_latency.csv";
    std::ofstream log(logPath.c_str(), std::ios::out | std::ios::trunc);
    std::ofstream rejects(rejectPath.c_str(), std::ios::out | std::ios::trunc);
    if (!log.is_open() || !rejects.is_open()) {
        std::cerr << "Could not open run log files.\n";
        return 1;
    }
    auto report = [&log](const std::string& line) {
        std::cout << line << '\n';
        log << line << '\n';
    };

    std::vector<OrderRequest> requests;
    std::vector<ParserRejection> parserRejections;
    std::string error;
    if (!OrderParser::parseFile(inputPath, requests, parserRejections, error)) {
        std::cerr << "ERROR: " << error << '\n';
        return 1;
    }
    for (const ParserRejection& rejection : parserRejections) {
        rejects << "Request Index: " << rejection.requestIndex << '\n'
                << "Order ID: "
                << (rejection.hasOrderId ? std::to_string(rejection.orderId) : "Unknown") << '\n'
                << "Rejection Source: PARSER\nReason: " << rejection.reason
                << "\nOriginal Record: " << rejection.originalRecord << "\n\n";
    }

    Book book;
    MatchingEngine engine(book);
    std::vector<OrderResult> results;
    results.reserve(requests.size());
    std::size_t accepted = 0;
    std::size_t rejected = 0;
    report("+============================================================+");
    report("|             LIMIT ORDER BOOK MATCHING ENGINE               |");
    report("+============================================================+");
    report(summaryField("Started at", currentDateTime()));
    report(summaryField("Input file", inputPath));
    report(summaryField("Requests loaded",
                        std::to_string(requests.size() + parserRejections.size())));
    report("+------------------------------------------------------------+");

    const Clock::time_point processingStart = Clock::now();
    for (const OrderRequest& request : requests) {
        const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        Order order(request.id, request.price, request.quantity, timestamp, request.side);
        const std::size_t firstTrade = engine.getTradeCount();
        const Clock::time_point start = Clock::now();
        const SubmissionResult result = engine.submitOrderDetailed(order, request.type);
        const std::uint64_t latency = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                Clock::now() - start).count());
        results.push_back({request.requestIndex, request.id, result.success,
                           result.reason, latency, firstTrade,
                           engine.getTradeCount(), book.getOrder(request.id) != nullptr});
        if (result.success) {
            ++accepted;
        } else {
            ++rejected;
        }
    }

    const std::uint64_t processingDuration = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - processingStart).count());
    for (std::size_t i = 0; i < results.size(); ++i) {
        const OrderResult& result = results[i];
        printOrder(log, requests[i], engine.getTrades(), result.firstTrade,
                   result.lastTrade, {result.accepted, result.reason},
                   result.restingInBook);
        if (!result.accepted) {
            rejects << "Request Index: " << result.requestIndex
                    << "\nOrder ID: " << result.orderID
                    << "\nRejection Source: ENGINE\nReason: " << result.reason << "\n\n";
        }
    }

    if (!writeLatencyCsv(latencyPath, results, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    const LatencySummary latency = summarizeLatencies(results);
    const double seconds = static_cast<double>(processingDuration) / 1'000'000'000.0;
    const double submissionsPerSecond = seconds > 0
        ? static_cast<double>(requests.size()) / seconds : 0.0;
    const double acceptedPerSecond = seconds > 0
        ? static_cast<double>(accepted) / seconds : 0.0;
    const double tradesPerSecond = seconds > 0
        ? static_cast<double>(engine.getTradeCount()) / seconds : 0.0;

    report("");
    report("+============================================================+");
    report("|                    EXECUTION SUMMARY                       |");
    report("+============================================================+");
    report(summaryField("Finished at", currentDateTime()));
    report(summaryField("Test mode", "ENGINE ONLY (Worklets 1-2)"));
    report(summaryField("Valid order submissions", std::to_string(requests.size())));
    report(summaryField("Parser rejections", std::to_string(parserRejections.size())));
    report(summaryField("Engine rejections", std::to_string(rejected)));
    report(summaryField("Accepted orders", std::to_string(accepted)));
    report(summaryField("Recorded trade executions", std::to_string(engine.getTradeCount())));
    report(summaryField("Orders resting after run", std::to_string(book.orderMap.getSize())));
    report(summaryField("Replay validation", "Not run (Worklet 3 excluded)"));
    report("");
    report("  PERFORMANCE");
    report("  ----------------------------------------------------------");
    report(summaryField("Timed processing duration",
                        formatRate(processingDuration / 1'000'000.0) + " ms"));
    report(summaryField("Throughput (same timed duration)",
                        formatRate(submissionsPerSecond) + " submissions/s (TPS)"));
    report(summaryField("", formatRate(acceptedPerSecond) + " accepted orders/s"));
    report(summaryField("", formatRate(tradesPerSecond) + " trade executions/s"));
    report(summaryField("Transactions per second (TPS)",
                        formatRate(submissionsPerSecond) + " transactions/s"));
    report(summaryField("TPS definition", "1 valid order submission = 1 transaction"));
    report(summaryField("TPS calculation",
                        std::to_string(requests.size()) + " / " +
                        formatSeconds(seconds) + " seconds"));
    report(summaryField("Submission latency (mean)",
                        formatRate(latency.mean / 1000.0) + " us"));
    report(summaryField("Latency p50 / p95 / p99 / max",
                        formatRate(latency.p50 / 1000.0) + " / " +
                        formatRate(latency.p95 / 1000.0) + " / " +
                        formatRate(latency.p99 / 1000.0) + " / " +
                        formatRate(latency.maximum / 1000.0) + " us"));
    report(summaryField("Timed work",
                        "Matching, book updates and metric collection"));
    report(summaryField("Excluded from timing", "Parsing, CLI/file output"));
    report("");
    report("  OUTPUT");
    report(summaryField("Per-order latency CSV", latencyPath));
    report(summaryField("Rejection log", rejectPath));
    report("+============================================================+");
    return 0;
}
}

int run(const std::string& inputPath) {
    return runEngineOnly(inputPath);
}

}

#include <cerrno>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "Book.hpp"
#include "EventLogger.hpp"
#include "EventLoggerIntegration.hpp"
#include "MatchingEngine.hpp"
#include "Order.hpp"
#include "OrderParser.hpp"
#include "PerformanceMetrics.hpp"
#include "replayEngine.hpp"

namespace FullSystem {
namespace {
using Clock = std::chrono::steady_clock;

std::string getBaseName(const std::string& path) {
    const std::size_t separator = path.find_last_of("/\\");
    std::string filename = separator == std::string::npos
        ? path
        : path.substr(separator + 1);
    const std::size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos && dot != 0) {
        filename = filename.substr(0, dot);
    }
    return filename;
}

std::string currentDateTime() {
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

const char* sideName(Side side) {
    if (side == Side::Buy) return "BUY";
    if (side == Side::Sell) return "SELL";
    return "INVALID";
}

const char* typeName(OrderType type) {
    if (type == OrderType::Limit) return "LIMIT";
    if (type == OrderType::Market) return "MARKET";
    return "INVALID";
}

std::string summaryField(const std::string& label, const std::string& value) {
    std::ostringstream line;
    line << "  " << std::left << std::setw(35) << label << " : " << value;
    return line.str();
}

std::string formatRate(double value) {
    std::ostringstream formatted;
    formatted << std::fixed << std::setprecision(2) << value;
    return formatted.str();
}

std::string formatSeconds(double value) {
    std::ostringstream formatted;
    formatted << std::fixed << std::setprecision(9) << value;
    return formatted.str();
}

void printer(
    std::ostream& output,
    const OrderRequest& request,
    const std::vector<Trade>& trades,
    std::size_t firstTrade,
    std::size_t lastTrade,
    const SubmissionResult& result,
    bool restingInBook
) {
    int executedQuantity = 0;
    output << "\n+--------------------------------------------------+\n"
           << "| ORDER #" << request.id << '\n'
           << "+--------------------------------------------------+\n"
           << "  REQUEST\n"
           << "    Type / side       : " << typeName(request.type)
           << " / " << sideName(request.side) << '\n';
    if (request.type == OrderType::Limit) {
        output << "    Limit price       : " << request.price << '\n';
    }
    output << "    Requested quantity: " << request.quantity
           << "\n\n  TRADES (" << (lastTrade - firstTrade) << ")\n";

    if (firstTrade == lastTrade) {
        output << "    No trades executed.\n";
    }
    for (std::size_t i = firstTrade; i < lastTrade; ++i) {
        const Trade& trade = trades[i];
        executedQuantity += trade.quantity;
        output << "    #" << trade.tradeID
               << " | BUY #" << trade.buyOrderID
               << " / SELL #" << trade.sellOrderID
               << " | price " << trade.price
               << " | qty " << trade.quantity << '\n';
    }

    const int remainingQuantity = request.quantity - executedQuantity;
    output << "\n  OUTCOME\n";
    if (!result.success && executedQuantity == 0 && !restingInBook) {
        output << "    Status             : REJECTED\n"
               << "    Reason             : " << result.reason << '\n';
    } else if (executedQuantity >= request.quantity) {
        output << "    Status             : FULLY FILLED\n";
    } else if (request.type == OrderType::Market) {
        output << "    Status             : "
               << (executedQuantity > 0 ? "PARTIALLY EXECUTED\n" : "NOT EXECUTED\n")
               << "    Executed quantity  : " << executedQuantity << '\n'
               << "    Unfilled discarded : " << remainingQuantity << '\n'
               << "    Resting in book    : No\n";
    } else if (restingInBook) {
        output << "    Status             : "
               << (executedQuantity > 0 ? "PARTIALLY FILLED\n" : "OPEN (NOT YET FILLED)\n")
               << "    Executed quantity  : " << executedQuantity << '\n'
               << "    Remaining quantity: " << remainingQuantity << '\n'
               << "    Resting in book    : Yes\n";
    } else {
        output << "    Status             : NOT FULLY EXECUTED\n"
               << "    Executed quantity  : " << executedQuantity << '\n'
               << "    Remaining quantity: " << remainingQuantity << '\n'
               << "    Resting in book    : No\n";
    }
    output << "+--------------------------------------------------+\n";
}

void drainFeed(MarketDataFeed& feed, std::uint64_t consumerID,
               std::uint64_t& eventsRead) {
    for (;;) {
        Event event;
        const FeedStatus status = feed.getNextEvent(consumerID, event);
        if (status == FeedStatus::EVENT_AVAILABLE) {
            ++eventsRead;
        } else if (status == FeedStatus::BUFFER_OVERFLOW) {
            if (!feed.recoverConsumer(consumerID)) {
                throw std::runtime_error("Unable to recover market-data consumer.");
            }
        } else if (status == FeedStatus::NO_EVENT) {
            return;
        } else {
            throw std::runtime_error("Market-data consumer is invalid.");
        }
    }
}

struct BenchmarkRun {
    std::uint64_t durationNanoseconds = 0;
    std::size_t acceptedOrders = 0;
    std::size_t trades = 0;
    std::size_t restingOrders = 0;
};

BenchmarkRun runEngineOnly(const std::vector<OrderRequest>& requests) {
    Book book;
    MatchingEngine engine(book);
    std::size_t accepted = 0;
    const Clock::time_point start = Clock::now();
    for (const OrderRequest& request : requests) {
        Order order(request.id, request.price, request.quantity,
                    static_cast<long long>(request.requestIndex), request.side);
        if (engine.submitOrderDetailed(order, request.type).success) {
            ++accepted;
        }
    }
    const std::uint64_t duration = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - start).count());
    return {duration, accepted, engine.getTradeCount(),
            static_cast<std::size_t>(book.orderMap.getSize())};
}

[[maybe_unused]] bool parsePositiveCount(const char* text, std::size_t& count) {
    if (!text || *text == '\0' || *text == '-') {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || parsed == 0 ||
        parsed > static_cast<unsigned long long>(
            std::numeric_limits<std::size_t>::max())) {
        return false;
    }
    count = static_cast<std::size_t>(parsed);
    return true;
}

double submissionsPerSecond(const BenchmarkRun& run, std::size_t submissionCount) {
    return run.durationNanoseconds == 0 ? 0.0 :
        static_cast<double>(submissionCount) * 1'000'000'000.0 /
            static_cast<double>(run.durationNanoseconds);
}

double acceptedOrdersPerSecond(const BenchmarkRun& run) {
    return run.durationNanoseconds == 0 ? 0.0 :
        static_cast<double>(run.acceptedOrders) * 1'000'000'000.0 /
            static_cast<double>(run.durationNanoseconds);
}

double tradesPerSecond(const BenchmarkRun& run) {
    return run.durationNanoseconds == 0 ? 0.0 :
        static_cast<double>(run.trades) * 1'000'000'000.0 /
            static_cast<double>(run.durationNanoseconds);
}

double median(std::vector<double> sortedValues) {
    std::sort(sortedValues.begin(), sortedValues.end());
    const std::size_t middle = sortedValues.size() / 2;
    if (sortedValues.size() % 2 != 0) {
        return sortedValues[middle];
    }
    return (sortedValues[middle - 1] + sortedValues[middle]) / 2.0;
}

std::uint64_t latencyPercentile(
    const std::vector<std::uint64_t>& sortedLatencies,
    double percentile
) {
    if (sortedLatencies.empty()) {
        return 0;
    }
    const std::size_t index = static_cast<std::size_t>(
        std::ceil(percentile * sortedLatencies.size()) - 1.0);
    return sortedLatencies[std::min(index, sortedLatencies.size() - 1)];
}

[[maybe_unused]] int runBenchmark(const std::string& inputFile, std::size_t repetitions) {
    std::vector<OrderRequest> requests;
    std::vector<ParserRejection> parserRejections;
    std::string error;
    if (!OrderParser::parseFile(inputFile, requests, parserRejections, error)) {
        std::cerr << "ERROR: " << error << '\n';
        return 1;
    }
    if (requests.empty()) {
        std::cerr << "Benchmark requires at least one structurally valid order.\n";
        return 1;
    }

    runEngineOnly(requests);
    std::vector<BenchmarkRun> runs;
    std::vector<double> submissionRates;
    std::vector<double> acceptedOrderRates;
    std::vector<double> tradeRates;
    runs.reserve(repetitions);
    submissionRates.reserve(repetitions);
    acceptedOrderRates.reserve(repetitions);
    tradeRates.reserve(repetitions);
    for (std::size_t runNumber = 0; runNumber < repetitions; ++runNumber) {
        const BenchmarkRun run = runEngineOnly(requests);
        if (!runs.empty() &&
            (run.acceptedOrders != runs.front().acceptedOrders ||
             run.trades != runs.front().trades ||
             run.restingOrders != runs.front().restingOrders)) {
            std::cerr << "Benchmark repetitions produced inconsistent matching results.\n";
            return 2;
        }
        runs.push_back(run);
        submissionRates.push_back(submissionsPerSecond(run, requests.size()));
        acceptedOrderRates.push_back(acceptedOrdersPerSecond(run));
        tradeRates.push_back(tradesPerSecond(run));
    }

    const double medianSubmissionRate = median(submissionRates);
    const double medianAcceptedOrderRate = median(acceptedOrderRates);
    const double medianTradeRate = median(tradeRates);

    std::vector<std::uint64_t> latencies;
    latencies.reserve(requests.size());
    std::size_t accepted = 0;
    std::size_t trades = 0;
    Book latencyBook;
    MatchingEngine latencyEngine(latencyBook);
    for (const OrderRequest& request : requests) {
        Order order(request.id, request.price, request.quantity,
                    static_cast<long long>(request.requestIndex), request.side);
        const Clock::time_point start = Clock::now();
        const SubmissionResult result =
            latencyEngine.submitOrderDetailed(order, request.type);
        latencies.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                Clock::now() - start).count()));
        if (result.success) {
            ++accepted;
        }
    }
    trades = latencyEngine.getTradeCount();
    if (accepted != runs.front().acceptedOrders ||
        trades != runs.front().trades) {
        std::cerr << "Latency pass produced inconsistent matching results.\n";
        return 2;
    }
    std::sort(latencies.begin(), latencies.end());
    const std::uint64_t totalLatency = std::accumulate(
        latencies.begin(), latencies.end(), std::uint64_t{0});
    const double meanLatency = static_cast<double>(totalLatency) / latencies.size();

    std::cout << "\n"
              << "============================================================\n"
              << " MATCHING ENGINE BENCHMARK (engine-only)\n"
              << "============================================================\n"
              << "Input:                       " << inputFile << '\n'
              << "Structurally valid orders:   " << requests.size() << '\n'
              << "Parser-rejected records:     " << parserRejections.size() << '\n'
              << "Warm-up runs:                1 (excluded)\n"
              << "Measured throughput runs:    " << repetitions << '\n'
              << "Measured accepted orders:    " << accepted << '\n'
              << "Trade executions:            " << trades << '\n'
              << "Orders resting after run:     " << runs.front().restingOrders << '\n'
              << "------------------------------------------------------------\n"
              << "Run   Elapsed (ms)   Submit/s      Accepted/s    Trades/s\n";
    for (std::size_t i = 0; i < runs.size(); ++i) {
        std::cout << std::setw(3) << (i + 1) << "   "
                  << std::fixed << std::setprecision(3)
                  << std::setw(11)
                  << runs[i].durationNanoseconds / 1'000'000.0 << "   "
                  << std::setprecision(0) << std::setw(12)
                  << submissionsPerSecond(runs[i], requests.size()) << "   "
                  << std::setw(12) << acceptedOrdersPerSecond(runs[i]) << "   "
                  << std::setw(12) << tradesPerSecond(runs[i]) << '\n';
    }
    std::cout << "------------------------------------------------------------\n"
              << std::fixed << std::setprecision(0)
              << "Median throughput:           " << medianSubmissionRate
              << " submissions/s; " << medianAcceptedOrderRate
              << " accepted orders/s; " << medianTradeRate << " trades/s\n"
              << "Latency pass (one run):       mean "
              << std::setprecision(3) << meanLatency / 1000.0
              << " us; p50 " << latencyPercentile(latencies, 0.50) / 1000.0
              << " us; p95 " << latencyPercentile(latencies, 0.95) / 1000.0
              << " us; p99 " << latencyPercentile(latencies, 0.99) / 1000.0
              << " us; max " << latencies.back() / 1000.0 << " us\n"
              << "Timing scope: MatchingEngine::submitOrderDetailed only.\n"
              << "Included: matching, fills, and insertion of unmatched limit remainders into the order book.\n"
              << "Excluded: JSON parsing, event log/feed, CLI output, replay and validation.\n"
              << "============================================================\n";
    return 0;
}

int runReplay(const std::string& eventFile) {
    std::vector<Event> events;
    std::string error;
    if (!EventLogger::readEvents(eventFile, events, error)) {
        std::cerr << "Event log is invalid: " << error << '\n';
        return 1;
    }

    ReplayEngine replay(events);
    const ReplayResult result = replay.replayTo();
    if (!result.success) {
        std::cerr << "Replay failed at sequence " << result.sequence
                  << ": " << result.message << '\n';
        return 1;
    }
    const ReplaySnapshot snapshot = replay.snapshot();
    std::cout << "Replay successful\n"
              << "Events: " << events.size() << '\n'
              << "Active orders: " << snapshot.orders.size() << '\n'
              << "Price levels: " << snapshot.levels.size() << '\n'
              << "Best bid: "
              << (snapshot.hasBestBid ? std::to_string(snapshot.bestBid) : "none") << '\n'
              << "Best ask: "
              << (snapshot.hasBestAsk ? std::to_string(snapshot.bestAsk) : "none") << '\n';
    if (!result.message.empty()) {
        std::cout << "Note: " << result.message << '\n';
    }
    return 0;
}

int runOrders(const std::string& inputFile) {
    std::error_code directoryError;
    std::filesystem::create_directories("log", directoryError);
    if (directoryError) {
        std::cerr << "Error creating log directory: " << directoryError.message() << '\n';
        return 1;
    }

    const std::string baseName = getBaseName(inputFile);
    const std::string logFile = "log/" + baseName + "_logs.log";
    const std::string rejectFile = "log/" + baseName + "_reject.log";
    const std::string eventFile = "log/" + baseName + "_events.log";
    const std::string latencyFile = "log/" + baseName + "_latency.csv";
    std::ofstream log(logFile.c_str(), std::ios::out | std::ios::trunc);
    std::ofstream rejectLog(rejectFile.c_str(), std::ios::out | std::ios::trunc);
    if (!log.is_open() || !rejectLog.is_open()) {
        std::cerr << "Could not open run log files.\n";
        return 1;
    }

    auto report = [&log](const std::string& message) {
        std::cout << message << '\n';
        log << message << '\n';
    };

    std::vector<OrderRequest> requests;
    std::vector<ParserRejection> parserRejections;
    std::string parseError;
    if (!OrderParser::parseFile(inputFile, requests, parserRejections, parseError)) {
        std::cerr << "ERROR: " << parseError << '\n';
        return 1;
    }

    for (const ParserRejection& rejection : parserRejections) {
        rejectLog << "Request Index: " << rejection.requestIndex << '\n'
                  << "Order ID: "
                  << (rejection.hasOrderId ? std::to_string(rejection.orderId) : "Unknown") << '\n'
                  << "Rejection Source: PARSER\n"
                  << "Reason: " << rejection.reason << '\n'
                  << "Original Record: " << rejection.originalRecord << "\n\n";
    }

    report("+============================================================+");
    report("|             LIMIT ORDER BOOK MATCHING ENGINE               |");
    report("+============================================================+");
    report(summaryField("Started at", currentDateTime()));
    report(summaryField("Input file", inputFile));
    report(summaryField("Requests loaded",
                        std::to_string(requests.size() + parserRejections.size())));
    report(summaryField("Event log", eventFile));
    report("+------------------------------------------------------------+");

    Book book;
    MatchingEngine engine(book);
    EventLogger eventLogger(eventFile, false);
    MarketDataFeed feed(65536);
    const std::uint64_t consumerID = feed.registerConsumer();
    EventLoggerIntegration integration(book, engine, eventLogger, feed);
    PerformanceMetrics performance;
    std::uint64_t feedEventsRead = 0;
    std::size_t accepted = 0;
    std::size_t engineRejected = 0;
    std::vector<std::size_t> firstTradeIndices;
    std::vector<std::size_t> lastTradeIndices;
    std::vector<SubmissionResult> submissionResults;
    std::vector<bool> restingStatuses;
    firstTradeIndices.reserve(requests.size());
    lastTradeIndices.reserve(requests.size());
    submissionResults.reserve(requests.size());
    restingStatuses.reserve(requests.size());
    const Clock::time_point processingStart = Clock::now();

    for (const OrderRequest& request : requests) {
        const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        Order order(request.id, request.price, request.quantity, timestamp, request.side);
        const std::size_t firstTrade = engine.getTradeCount();
        const Clock::time_point orderStart = Clock::now();
        const SubmissionResult result = integration.submitOrderDetailed(order, request.type);
        const std::uint64_t latency = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                Clock::now() - orderStart).count());
        performance.recordOrder({request.requestIndex, request.id, result.success,
                                 result.reason, latency});
        drainFeed(feed, consumerID, feedEventsRead);

        firstTradeIndices.push_back(firstTrade);
        lastTradeIndices.push_back(engine.getTradeCount());
        submissionResults.push_back(result);
        restingStatuses.push_back(book.getOrder(request.id) != nullptr);

        if (result.success) {
            ++accepted;
        } else {
            ++engineRejected;
        }
    }

    const std::uint64_t totalDuration = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - processingStart).count());
    for (std::size_t i = 0; i < requests.size(); ++i) {
        printer(log, requests[i], engine.getTrades(), firstTradeIndices[i],
                lastTradeIndices[i], submissionResults[i], restingStatuses[i]);
        if (!submissionResults[i].success) {
            rejectLog << "Request Index: " << requests[i].requestIndex << '\n'
                      << "Order ID: " << requests[i].id << '\n'
                      << "Rejection Source: ENGINE\n"
                      << "Reason: " << submissionResults[i].reason << "\n\n";
        }
    }
    eventLogger.flush();

    const std::vector<Event> events = eventLogger.getEvents();
    const std::size_t eventCount = events.size();
    ReplayEngine replay(events);
    const ReplayResult replayResult = replay.replayTo();
    if (!replayResult.success) {
        std::cerr << "Recorded events failed replay at sequence "
                  << replayResult.sequence << ": " << replayResult.message << '\n';
        return 2;
    }
    const std::vector<std::string> differences = replay.compareWith(book);
    if (!differences.empty()) {
        std::cerr << "Replayed book differs from live book:\n";
        for (const std::string& difference : differences) {
            std::cerr << " - " << difference << '\n';
        }
        return 2;
    }

    std::string outputError;
    if (!performance.writeCsv(latencyFile, outputError)) {
        std::cerr << outputError << '\n';
        return 1;
    }

    const PerformanceSummary metrics =
        performance.summarize(totalDuration, engine.getTradeCount());
    const FeedStats feedStats = feed.getFeedStats();
    ConsumerStats consumerStats{};
    if (!feed.getConsumerStats(consumerID, consumerStats)) {
        throw std::runtime_error("Unable to read market-data consumer metrics.");
    }

    const double seconds =
        static_cast<double>(metrics.totalDurationNanoseconds) / 1'000'000'000.0;
    const double acceptedPerSecond = seconds > 0
        ? static_cast<double>(accepted) / seconds : 0.0;
    report("");
    report("+============================================================+");
    report("|                    EXECUTION SUMMARY                       |");
    report("+============================================================+");
    report(summaryField("Finished at", currentDateTime()));
    report(summaryField("Test mode", "FULL SYSTEM (Worklets 1-3)"));
    report(summaryField("Valid order submissions", std::to_string(requests.size())));
    report(summaryField("Parser rejections", std::to_string(parserRejections.size())));
    report(summaryField("Engine rejections", std::to_string(engineRejected)));
    report(summaryField("Accepted orders", std::to_string(accepted)));
    report(summaryField("Recorded trade executions", std::to_string(engine.getTradeCount())));
    report(summaryField("Orders resting after run", std::to_string(book.orderMap.getSize())));
    report(summaryField("Replay validation",
                        "Passed; " + std::to_string(eventCount) +
                        " events; live book matches"));
    report("");
    report("  PERFORMANCE");
    report("  ----------------------------------------------------------");
    report(summaryField("Timed processing duration",
                        formatRate(metrics.totalDurationNanoseconds / 1'000'000.0) + " ms"));
    report(summaryField("Throughput (same timed duration)",
                        formatRate(metrics.ordersPerSecond) + " submissions/s (TPS)"));
    report(summaryField("", formatRate(acceptedPerSecond) + " accepted orders/s"));
    report(summaryField("", formatRate(metrics.tradesPerSecond) +
                        " trade executions/s"));
    report(summaryField("Transactions per second (TPS)",
                        formatRate(metrics.ordersPerSecond) + " transactions/s"));
    report(summaryField("TPS definition", "1 valid order submission = 1 transaction"));
    report(summaryField("TPS calculation",
                        std::to_string(requests.size()) + " / " +
                        formatSeconds(seconds) + " seconds"));
    report(summaryField("Submission latency (mean)",
                        formatRate(metrics.meanLatencyNanoseconds / 1000.0) + " us"));
    report(summaryField("Latency p50 / p95 / p99 / max",
                        formatRate(metrics.p50LatencyNanoseconds / 1000.0) + " / " +
                        formatRate(metrics.p95LatencyNanoseconds / 1000.0) + " / " +
                        formatRate(metrics.p99LatencyNanoseconds / 1000.0) + " / " +
                        formatRate(metrics.maxLatencyNanoseconds / 1000.0) + " us"));
    report(summaryField("Timed work",
                        "Matching, book updates and metric collection"));
    report(summaryField("Included in full-system mode",
                        "Event publication and feed draining"));
    report(summaryField("Excluded from timing",
                        "Parsing, CLI output, event flush and replay"));
    report(summaryField("Replay validation",
                        "Completed after timing; excluded from TPS"));
    report("");
    report("  OUTPUT");
    report(summaryField("Per-order latency CSV", latencyFile));
    report(summaryField("Event log", eventFile));
    report(summaryField("Feed events (published / read / missed)",
                        std::to_string(feedStats.eventsPublished) + " / " +
                        std::to_string(feedEventsRead) + " / " +
                        std::to_string(consumerStats.missedEvents)));
    report("+============================================================+");
    return 0;
}
}

int runOrdersCli(const std::string& inputFile) {
    return runOrders(inputFile);
}

int runReplayCli(const std::string& eventFile) {
    return runReplay(eventFile);
}

}

int main(int argc, char* argv[]) {
    try {
        if (argc == 3 && std::string(argv[1]) == "--engine-only") {
            return EngineOnly::run(argv[2]);
        }
        if (argc == 3 && std::string(argv[1]) == "--test-system") {
            return FullSystem::runOrdersCli(argv[2]);
        }
        if (argc == 3 && std::string(argv[1]) == "--replay") {
            return FullSystem::runReplayCli(argv[2]);
        }
        std::cerr << "Usage:\n"
                  << "  matchingengine.exe --engine-only <input.json>\n"
                  << "  matchingengine.exe --test-system <input.json>\n"
                  << "  matchingengine.exe --replay <events.log>\n";
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
