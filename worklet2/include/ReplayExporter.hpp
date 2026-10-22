
#ifndef REPLAY_EXPORTER_HPP
#define REPLAY_EXPORTER_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "Book.hpp"
#include "MatchingEngine.hpp"
#include "OrderParser.hpp"
#include "Trade.hpp"

class ReplayExporter {
private:
    std::vector<std::string> serializedSteps;

    std::string makeSnapshot(Book& book) const;

public:
    ReplayExporter();

    void recordOrder(
        Book& book,
        const OrderRequest& request,
        const std::vector<Trade>& trades,
        std::size_t firstTrade,
        std::size_t lastTrade,
        const SubmissionResult& result
    );

    bool save(
        const std::string& filename,
        std::string& error
    ) const;
};

#endif
