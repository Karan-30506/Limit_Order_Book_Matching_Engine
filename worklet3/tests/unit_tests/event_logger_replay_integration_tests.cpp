#include "EventLoggerIntegration.hpp"
#include "replayEngine.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>

void runEventLoggerReplayIntegrationTests() {
    const std::string logFile = "worklet3_event_log_test.tsv";
    std::remove(logFile.c_str());

    Book liveBook;
    MatchingEngine matchingEngine(liveBook);
    MarketDataFeed feed(16);
    const std::uint64_t consumer = feed.registerConsumer();
    std::vector<Event> memoryEvents;
    {
        EventLogger logger(logFile);
        EventLoggerIntegration integration(liveBook, matchingEngine, logger, feed);

        Order restingSell(300, 100, 5, 1, Side::Sell);
        Order incomingBuy(301, 105, 2, 2, Side::Buy);
        assert(integration.submitOrder(restingSell, OrderType::Limit));
        assert(integration.submitOrder(incomingBuy, OrderType::Limit));
        assert(integration.modifyOrder(300, 101, 4));
        assert(integration.cancelOrder(300));

        memoryEvents = logger.getEvents();
        assert(memoryEvents.size() == 5);
        assert(memoryEvents[3].type == EventType::MODIFY_ORDER);
        assert(memoryEvents[3].orderID == 300);
        assert(memoryEvents[3].price == 101);
        assert(memoryEvents[3].quantity == 4);
        assert(memoryEvents[4].type == EventType::CANCEL_ORDER);
        assert(memoryEvents[4].orderID == 300);
        assert(memoryEvents[4].side == Side::Sell);
        assert(memoryEvents[4].price == 101);
        assert(memoryEvents[4].quantity == 4);

        std::uint64_t nextLoggedSequence = 1;
        for (const Event& expected : memoryEvents) {
            Event received{};
            assert(logger.getNextEvent(nextLoggedSequence, received) ==
                   FeedStatus::EVENT_AVAILABLE);
            assert(received.sequenceID == expected.sequenceID);
            assert(received.type == expected.type);
        }
        Event noLoggedEvent{};
        assert(logger.getNextEvent(nextLoggedSequence, noLoggedEvent) ==
               FeedStatus::NO_EVENT);

        std::uint64_t zeroBasedCursor = 0;
        Event firstLoggedEvent{};
        assert(logger.getNextEvent(zeroBasedCursor, firstLoggedEvent) ==
               FeedStatus::EVENT_AVAILABLE);
        assert(firstLoggedEvent.sequenceID == 1);
        assert(zeroBasedCursor == 2);

        for (const Event& expected : memoryEvents) {
            Event received{};
            assert(feed.getNextEvent(consumer, received) == FeedStatus::EVENT_AVAILABLE);
            assert(received.sequenceID == expected.sequenceID);
            assert(received.timestamp == expected.timestamp);
            assert(received.type == expected.type);
            assert(received.orderID == expected.orderID);
            assert(received.tradeID == expected.tradeID);
            assert(received.buyOrderID == expected.buyOrderID);
            assert(received.sellOrderID == expected.sellOrderID);
            assert(received.side == expected.side);
            assert(received.orderType == expected.orderType);
            assert(received.price == expected.price);
            assert(received.quantity == expected.quantity);
            assert(received.orderTimestamp == expected.orderTimestamp);
        }
        Event noEvent{};
        assert(feed.getNextEvent(consumer, noEvent) == FeedStatus::NO_EVENT);

        ReplayEngine inMemoryReplay(logger);
        assert(inMemoryReplay.replayTo().success);
        assert(inMemoryReplay.compareWith(liveBook).empty());
    }

    std::vector<Event> recovered;
    std::string error;
    assert(EventLogger::readEvents(logFile, recovered, error));
    assert(error.empty());
    assert(recovered.size() == 5);

    ReplayEngine recoveredReplay(recovered);
    assert(recoveredReplay.replayTo().success);
    assert(recoveredReplay.compareWith(liveBook).empty());

    std::string firstValidRecord;
    {
        std::ifstream persistedLog(logFile, std::ios::in | std::ios::binary);
        assert(std::getline(persistedLog, firstValidRecord));
    }

    {
        std::ofstream malformedLog(logFile, std::ios::out | std::ios::app);
        assert(malformedLog.is_open());
        malformedLog << "incomplete\tpersisted\trecord\n";
    }

    std::vector<Event> eventsBeforeMalformedRecord;
    assert(!EventLogger::readEvents(
        logFile,
        eventsBeforeMalformedRecord,
        error
    ));
    assert(!error.empty());
    assert(eventsBeforeMalformedRecord.size() == memoryEvents.size());
    for (std::size_t i = 0; i < eventsBeforeMalformedRecord.size(); ++i) {
        assert(eventsBeforeMalformedRecord[i].sequenceID == i + 1);
    }
    bool malformedLogRejected = false;
    try {
        EventLogger malformedLogger(logFile);
    } catch (const std::runtime_error&) {
        malformedLogRejected = true;
    }
    assert(malformedLogRejected);

    {
        std::ofstream malformedLog(
            logFile,
            std::ios::out | std::ios::trunc | std::ios::binary
        );
        assert(malformedLog.is_open());
        malformedLog << firstValidRecord << "\textra-field\n";
    }
    assert(!EventLogger::readEvents(
        logFile,
        eventsBeforeMalformedRecord,
        error
    ));
    assert(eventsBeforeMalformedRecord.empty());

    const std::size_t sequenceDelimiter = firstValidRecord.find('\t');
    assert(sequenceDelimiter != std::string::npos);
    firstValidRecord.replace(0, sequenceDelimiter, "1trailing");
    {
        std::ofstream malformedLog(
            logFile,
            std::ios::out | std::ios::trunc | std::ios::binary
        );
        assert(malformedLog.is_open());
        malformedLog << firstValidRecord << '\n';
    }
    assert(!EventLogger::readEvents(
        logFile,
        eventsBeforeMalformedRecord,
        error
    ));
    assert(eventsBeforeMalformedRecord.empty());

    std::remove(logFile.c_str());

    const std::string failedPublicationLog =
        "worklet3_failed_publication_test.tsv";
    std::remove(failedPublicationLog.c_str());

    Book publicationBook;
    MatchingEngine publicationEngine(publicationBook);
    MarketDataFeed outOfSyncFeed(4);
    Event existingEvent{};
    existingEvent.sequenceID = 1;
    assert(outOfSyncFeed.publish(existingEvent) == PublishStatus::PUBLISHED);

    {
        EventLogger publicationLogger(failedPublicationLog);
        EventLoggerIntegration publicationIntegration(
            publicationBook,
            publicationEngine,
            publicationLogger,
            outOfSyncFeed
        );
        Order acceptedOrder(400, 100, 2, 10, Side::Buy);
        bool publicationFailed = false;
        try {
            publicationIntegration.submitOrder(
                acceptedOrder,
                OrderType::Limit
            );
        } catch (const std::runtime_error&) {
            publicationFailed = true;
        }

        assert(publicationFailed);
        assert(publicationBook.getOrder(400) != nullptr);
        const std::vector<Event> loggedAfterFailure =
            publicationLogger.getEvents();
        assert(loggedAfterFailure.size() == 1);
        assert(loggedAfterFailure[0].sequenceID == 1);
        assert(outOfSyncFeed.getLatestSequence() == 1);
    }

    std::remove(failedPublicationLog.c_str());
    std::cout << "Event Logger Replay Integration tests passed.\n";
}
