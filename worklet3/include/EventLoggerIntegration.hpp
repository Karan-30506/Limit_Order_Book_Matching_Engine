#ifndef EVENT_LOGGER_INTEGRATION_HPP
#define EVENT_LOGGER_INTEGRATION_HPP

#include "Book.hpp"
#include "MatchingEngine.hpp"
#include "EventLogger.hpp"

class EventLoggerIntegration {
public:
    EventLoggerIntegration(
        Book& book,
        MatchingEngine& engine,
        EventLogger& logger
    );

    EventLoggerIntegration(
        Book& book,
        MatchingEngine& engine,
        EventLogger& logger,
        MarketDataFeed& feed
    );

    bool submitOrder(const Order& order, OrderType type);
    bool cancelOrder(int orderID);
    bool modifyOrder(int orderID, int newPrice, int newQuantity);

private:
    void publishLastEvent();

    Book& book_;
    MatchingEngine& engine_;
    EventLogger& logger_;
    MarketDataFeed* feed_;
};

#endif