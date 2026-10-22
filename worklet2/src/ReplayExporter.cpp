
#include "ReplayExporter.hpp"

#include <fstream>
#include <sstream>

#include "LimitNode.hpp"
#include "Order.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

ReplayExporter::ReplayExporter() {}

std::string ReplayExporter::makeSnapshot(Book& book) const {
    json snapshot;
    snapshot["bids"] = json::array();
    snapshot["asks"] = json::array();

    // Bids: highest price to lowest price.
    for (LimitNode* level = book.getBestBid();
         level != nullptr;
         level = book.getNextLevel(level)) {

        json item;
        item["price"] = level->price;
        item["volume"] = level->queue.totalQuantity;
        item["orders"] = json::array();

        for (Order* order = level->queue.head;
             order != nullptr;
             order = order->next) {

            item["orders"].push_back({
                {"id", order->id},
                {"quantity", order->quantity}
            });
        }

        snapshot["bids"].push_back(item);
    }

    // Asks: lowest price to highest price.
    for (LimitNode* level = book.getBestAsk();
         level != nullptr;
         level = book.getNextLevel(level)) {

        json item;
        item["price"] = level->price;
        item["volume"] = level->queue.totalQuantity;
        item["orders"] = json::array();

        for (Order* order = level->queue.head;
             order != nullptr;
             order = order->next) {

            item["orders"].push_back({
                {"id", order->id},
                {"quantity", order->quantity}
            });
        }

        snapshot["asks"].push_back(item);
    }

    return snapshot.dump();
}

void ReplayExporter::recordOrder(
    Book& book,
    const OrderRequest& request,
    const std::vector<Trade>& trades,
    std::size_t firstTrade,
    std::size_t lastTrade,
    const SubmissionResult& result
) {
    json step;

    step["requestIndex"] = request.requestIndex;
    step["order"] = {
        {"id", request.id},
        {"type", request.originalType},
        {"side", request.originalSide},
        {"price", request.price},
        {"quantity", request.quantity}
    };

    step["accepted"] = result.success;
    step["reason"] = result.reason;
    step["event"] = result.success
        ? "ORDER_ACCEPTED"
        : "ORDER_REJECTED";

    step["trades"] = json::array();

    for (std::size_t i = firstTrade; i < lastTrade; ++i) {
        const Trade& trade = trades[i];

        step["trades"].push_back({
            {"tradeID", trade.tradeID},
            {"buyOrderID", trade.buyOrderID},
            {"sellOrderID", trade.sellOrderID},
            {"price", trade.price},
            {"quantity", trade.quantity}
        });
    }

    // State immediately after this order submission.
    step["book"] = json::parse(makeSnapshot(book));

    serializedSteps.push_back(step.dump());
}

bool ReplayExporter::save(
    const std::string& filename,
    std::string& error
) const {
    std::ofstream file(filename.c_str());

    if (!file.is_open()) {
        error = "Could not open replay output file: " + filename;
        return false;
    }

    file << "{\n  \"version\": 1,\n  \"steps\": [\n";

    for (std::size_t i = 0; i < serializedSteps.size(); ++i) {
        file << "    " << serializedSteps[i];

        if (i + 1 < serializedSteps.size()) {
            file << ',';
        }

        file << '\n';
    }

    file << "  ]\n}\n";

    if (!file.good()) {
        error = "Failed while writing replay output: " + filename;
        return false;
    }

    error.clear();
    return true;
}