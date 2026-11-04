#include "EngineService.hpp"

#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}

int main() {
    try {
        EngineService service;
        const SubmissionResult first = service.submitOrder(
            Order(1, 100, 5, 1, Side::Buy), OrderType::Limit);
        const SubmissionResult second = service.submitOrder(
            Order(2, 100, 3, 2, Side::Buy), OrderType::Limit);
        require(first.success && second.success, "engine service rejected resting orders");

        BookSnapshot snapshot = service.snapshot();
        require(snapshot.buyRoot >= 0, "snapshot omitted the buy-tree root");
        const PriceLevelSnapshot& level =
            snapshot.buyLevels[static_cast<std::size_t>(snapshot.buyRoot)];
        require(level.price == 100 && level.totalQuantity == 8,
                "snapshot price-level totals are incorrect");
        require(level.orders.size() == 2 && level.orders[0].id == 1 &&
                level.orders[1].id == 2 && level.orders[0].fifoPosition == 1 &&
                level.orders[1].fifoPosition == 2,
                "snapshot did not preserve the FIFO queue");
        require(snapshot.openOrders.size() == 2,
                "snapshot omitted active orders");
        require(service.modifyOrder(2, 101, 3),
                "engine service could not modify a resting order");
        snapshot = service.snapshot();
        require(snapshot.buyLevels.size() == 2 && snapshot.openOrders[0].id == 2 &&
                snapshot.openOrders[0].price == 101,
                "modified order did not appear at the new price in the snapshot");
        require(service.cancelOrder(1), "engine service could not cancel an order");

        std::string error;
        require(service.reset(true, error), "service failed to enter full-system mode");
        require(service.submitOrder(
                    Order(3, 100, 4, 3, Side::Buy), OrderType::Limit).success,
                "full-system service rejected the resting order");
        require(service.submitOrder(
                    Order(4, 99, 2, 4, Side::Sell), OrderType::Limit).success,
                "full-system service rejected the matching order");
        require(service.modifyOrder(3, 101, 1),
                "full-system service could not publish an order modification");
        require(service.cancelOrder(3),
                "full-system service could not publish an order cancellation");
        require(service.drainFeed() == FeedDrainStatus::Drained,
                "service did not drain its feed");

        snapshot = service.snapshot();
        require(snapshot.trades.size() == 1 && snapshot.trades[0].quantity == 2,
                "snapshot did not capture the generated trade");
        FeedStats feedStats;
        ConsumerStats consumerStats;
        require(service.getFeedStats(feedStats, consumerStats),
                "service did not expose Worklet 3 feed statistics");
        require(feedStats.eventsPublished == 5 && consumerStats.eventsRead == 5,
                "feed snapshot counters are inconsistent");
    } catch (const std::exception& error) {
        std::cerr << "EngineService test failed: " << error.what() << '\n';
        return 1;
    }

    std::remove("lob_gui_events.log");
    std::cout << "EngineService snapshot and integration tests passed.\n";
    return 0;
}
