#include "replayEngine.hpp"

#include <algorithm>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "Book.hpp"
#include "LimitNode.hpp"

namespace {
using Orders = std::vector<ReplayOrder>;

struct ReplayState {
    std::map<int, Orders> buys;
    std::map<int, Orders> sells;
    std::unordered_map<int, ReplayOrder> active;
    std::unordered_set<int> tradeIDs;

    struct PendingOrder {
        ReplayOrder order;
        int filledQuantity = 0;
    };
    std::unique_ptr<PendingOrder> pending;
};

bool validSide(Side side) {
    return side == Side::Buy || side == Side::Sell;
}

bool validOrderType(OrderType type) {
    return type == OrderType::Limit || type == OrderType::Market;
}

std::map<int, Orders>& sideLevels(ReplayState& state, Side side) {
    return side == Side::Buy ? state.buys : state.sells;
}

void addOrder(ReplayState& state, const ReplayOrder& order) {
    state.active.emplace(order.id, order);
    sideLevels(state, order.side)[order.price].push_back(order);
}

bool removeOrder(ReplayState& state, int id) {
    auto active = state.active.find(id);
    if (active == state.active.end()) return false;

    auto& orders = sideLevels(state, active->second.side)[active->second.price];
    const auto queued = std::find_if(orders.begin(), orders.end(),
        [id](const ReplayOrder& order) { return order.id == id; });
    if (queued == orders.end()) return false;
    orders.erase(queued);
    if (orders.empty()) sideLevels(state, active->second.side).erase(active->second.price);
    state.active.erase(active);
    return true;
}

std::string orderText(int id) {
    return "order " + std::to_string(id);
}
}

struct ReplayBook::State : ReplayState {};

ReplayBook::ReplayBook() : state_(std::make_unique<State>()) {}
ReplayBook::~ReplayBook() = default;

void ReplayBook::reset() {
    state_->buys.clear();
    state_->sells.clear();
    state_->active.clear();
    state_->tradeIDs.clear();
    state_->pending.reset();
}

ReplayResult ReplayBook::fail(const Event& event, const std::string& message) const {
    return {false, event.sequenceID, message};
}

ReplayResult ReplayBook::finishPending() {
    if (!state_->pending) return {true, 0, ""};
    const ReplayState::PendingOrder& pending = *state_->pending;
    const int remaining = pending.order.quantity - pending.filledQuantity;
    if (remaining < 0) return {false, 0, "execution quantity exceeds " + orderText(pending.order.id)};
    if (remaining > 0 && pending.order.orderType == OrderType::Limit) {
        ReplayOrder remainder = pending.order;
        remainder.quantity = remaining;
        addOrder(*state_, remainder);
    }
    state_->pending.reset();
    return {true, 0, ""};
}

ReplayResult ReplayBook::apply(const Event& event) {
    if (event.sequenceID == 0) return fail(event, "event has no sequence number");

    if (event.type == EventType::NEW_ORDER) {
        if (event.orderID <= 0 ||
            (event.orderType == OrderType::Limit && event.price <= 0) ||
            event.quantity <= 0 || !validSide(event.side) || !validOrderType(event.orderType)) {
            return fail(event, "invalid new-order fields");
        }
        if (state_->active.count(event.orderID) ||
            (state_->pending && state_->pending->order.id == event.orderID)) {
            return fail(event, "duplicate active or pending order ID");
        }
        auto finished = finishPending();
        if (!finished.success) return fail(event, finished.message);
        state_->pending = std::make_unique<ReplayState::PendingOrder>(ReplayState::PendingOrder{
            {event.orderID, event.side, event.price, event.quantity, event.orderTimestamp, event.orderType}, 0});
        return {true, event.sequenceID, ""};
    }

    if (event.type == EventType::TRADE) {
        if (event.tradeID <= 0 || event.quantity <= 0 || event.price <= 0 ||
            event.buyOrderID <= 0 || event.sellOrderID <= 0 ||
            event.buyOrderID == event.sellOrderID || state_->tradeIDs.count(event.tradeID)) {
            return fail(event, "invalid or duplicate trade record");
        }
        if (!state_->pending ||
            (event.buyOrderID != state_->pending->order.id &&
             event.sellOrderID != state_->pending->order.id)) {
            return fail(event, "trade is not associated with the current incoming order");
        }
        const int restingID = event.buyOrderID == state_->pending->order.id
            ? event.sellOrderID : event.buyOrderID;
        auto resting = state_->active.find(restingID);
        if (resting == state_->active.end()) return fail(event, "trade references an unknown resting order");
        if (resting->second.side == state_->pending->order.side ||
            resting->second.price != event.price || event.quantity > resting->second.quantity) {
            return fail(event, "trade does not match the resting order state");
        }
        if (event.quantity > state_->pending->order.quantity - state_->pending->filledQuantity) {
            return fail(event, "execution quantity exceeds " + orderText(state_->pending->order.id));
        }
        if (state_->pending->order.orderType == OrderType::Limit &&
            ((state_->pending->order.side == Side::Buy &&
              event.price > state_->pending->order.price) ||
             (state_->pending->order.side == Side::Sell &&
              event.price < state_->pending->order.price))) {
            return fail(event, "trade price violates the incoming limit order");
        }
        state_->tradeIDs.insert(event.tradeID);
        state_->pending->filledQuantity += event.quantity;
        if (event.quantity == resting->second.quantity) {
            removeOrder(*state_, restingID);
        } else {
            auto& order = resting->second;
            auto& level = sideLevels(*state_, order.side)[order.price];
            order.quantity -= event.quantity;
            for (auto& queued : level) if (queued.id == restingID) queued.quantity = order.quantity;
        }
        return {true, event.sequenceID, ""};
    }

    if (event.type == EventType::CANCEL_ORDER) {
        const bool activeOrder = state_->active.find(event.orderID) != state_->active.end();
        const bool pendingOrder = state_->pending &&
            state_->pending->order.id == event.orderID;
        if (event.orderID <= 0 || (!activeOrder && !pendingOrder)) {
            return fail(event, "cancel references an unknown order");
        }
        if (pendingOrder &&
            (state_->pending->order.orderType != OrderType::Limit ||
             state_->pending->filledQuantity >=
                 state_->pending->order.quantity)) {
            return fail(event, "cancel references a non-resting order");
        }
        auto finished = finishPending();
        if (!finished.success) return fail(event, finished.message);
        if (!removeOrder(*state_, event.orderID)) {
            return fail(event, "cancel references an inconsistent order");
        }
        return {true, event.sequenceID, ""};
    }

    if (event.type == EventType::MODIFY_ORDER) {
        if (event.orderID <= 0 || event.price <= 0 || event.quantity <= 0 || !validSide(event.side)) {
            return fail(event, "invalid modify fields");
        }
        const auto existing = state_->active.find(event.orderID);
        const bool pendingOrder = state_->pending &&
            state_->pending->order.id == event.orderID;
        if (existing == state_->active.end() && !pendingOrder) {
            return fail(event, "modify references an unknown order");
        }
        if (pendingOrder &&
            (state_->pending->order.orderType != OrderType::Limit ||
             state_->pending->filledQuantity >=
                 state_->pending->order.quantity)) {
            return fail(event, "modify references a non-resting order");
        }
        const Side existingSide = existing != state_->active.end()
            ? existing->second.side : state_->pending->order.side;
        if (existingSide != event.side) return fail(event, "modify changes an order side");
        auto finished = finishPending();
        if (!finished.success) return fail(event, finished.message);
        if (!removeOrder(*state_, event.orderID)) return fail(event, "modify references an inconsistent order");
        addOrder(*state_, {event.orderID, event.side, event.price, event.quantity, event.orderTimestamp});
        return {true, event.sequenceID, ""};
    }

    return fail(event, "unsupported event type");
}

ReplayResult ReplayBook::finalize() {
    return finishPending();
}

bool ReplayBook::hasPendingOrder() const {
    return state_->pending != nullptr;
}

int ReplayBook::pendingOrderID() const {
    return state_->pending ? state_->pending->order.id : 0;
}

ReplaySnapshot ReplayBook::snapshot() const {
    ReplaySnapshot result;
    for (const auto& item : state_->active) result.orders.push_back(item.second);
    std::sort(result.orders.begin(), result.orders.end(), [](const ReplayOrder& a, const ReplayOrder& b) {
        return a.id < b.id;
    });
    for (Side side : {Side::Buy, Side::Sell}) {
        const auto& levels = side == Side::Buy ? state_->buys : state_->sells;
        if (side == Side::Buy) {
            for (auto levelIt = levels.rbegin(); levelIt != levels.rend(); ++levelIt) {
                ReplayLevel level{side, levelIt->first, 0, {}};
                for (const auto& order : levelIt->second) {
                    level.totalQuantity += order.quantity;
                    level.orderIDs.push_back(order.id);
                }
                result.levels.push_back(level);
            }
            continue;
        }
        for (auto levelIt = levels.begin(); levelIt != levels.end(); ++levelIt) {
            ReplayLevel level{side, levelIt->first, 0, {}};
            for (const auto& order : levelIt->second) {
                level.totalQuantity += order.quantity;
                level.orderIDs.push_back(order.id);
            }
            result.levels.push_back(level);
        }
    }
    if (!state_->buys.empty()) { result.hasBestBid = true; result.bestBid = state_->buys.rbegin()->first; }
    if (!state_->sells.empty()) { result.hasBestAsk = true; result.bestAsk = state_->sells.begin()->first; }
    return result;
}

std::vector<std::string> ReplayBook::compareWith(Book& liveBook) const {
    std::vector<std::string> differences;
    const ReplaySnapshot expected = snapshot();
    std::vector<ReplayOrder> actualOrders;
    std::unordered_set<LimitNode*> visitedLevels;
    std::unordered_set<Order*> visitedOrders;
    for (Side side : {Side::Buy, Side::Sell}) {
        LimitNode* level = side == Side::Buy ? liveBook.getBestBid() : liveBook.getBestAsk();
        while (level) {
            if (!visitedLevels.insert(level).second) {
                differences.push_back("live price-level traversal contains a cycle");
                break;
            }
            for (Order* order = level->queue.head; order; order = order->next) {
                if (!visitedOrders.insert(order).second) {
                    differences.push_back("live order-queue traversal contains a cycle");
                    break;
                }
                actualOrders.push_back({order->id, order->side, order->price, order->quantity, order->timestamp, OrderType::Limit});
            }
            level = liveBook.getNextLevel(level);
        }
    }
    std::sort(actualOrders.begin(), actualOrders.end(), [](const ReplayOrder& a, const ReplayOrder& b) { return a.id < b.id; });
    if (actualOrders.size() != expected.orders.size()) differences.push_back("active order count differs");
    std::unordered_map<int, ReplayOrder> actualByID;
    for (const auto& order : actualOrders) actualByID.emplace(order.id, order);
    for (const auto& expectedOrder : expected.orders) {
        const auto actual = actualByID.find(expectedOrder.id);
        if (actual == actualByID.end()) {
            differences.push_back("missing active order " + std::to_string(expectedOrder.id));
            continue;
        }
        if (actual->second.side != expectedOrder.side ||
            actual->second.price != expectedOrder.price ||
            actual->second.quantity != expectedOrder.quantity) {
            differences.push_back("active order " + std::to_string(expectedOrder.id) + " differs");
        }
        actualByID.erase(actual);
    }
    for (const auto& unexpected : actualByID) {
        differences.push_back("unexpected active order " + std::to_string(unexpected.first));
    }
    std::vector<ReplayLevel> actualLevels;
    visitedLevels.clear();
    visitedOrders.clear();
    for (Side side : {Side::Buy, Side::Sell}) {
        LimitNode* level = side == Side::Buy ? liveBook.getBestBid() : liveBook.getBestAsk();
        while (level) {
            if (!visitedLevels.insert(level).second) {
                differences.push_back("live price-level traversal contains a cycle");
                break;
            }
            ReplayLevel actual{side, level->price, level->queue.totalQuantity, {}};
            for (Order* order = level->queue.head; order; order = order->next) {
                if (!visitedOrders.insert(order).second) {
                    differences.push_back("live order-queue traversal contains a cycle");
                    break;
                }
                actual.orderIDs.push_back(order->id);
            }
            actualLevels.push_back(actual);
            level = liveBook.getNextLevel(level);
        }
    }
    if (actualLevels.size() != expected.levels.size()) differences.push_back("price-level count differs");
    const std::size_t levelCount = std::min(actualLevels.size(), expected.levels.size());
    for (std::size_t i = 0; i < levelCount; ++i) {
        const auto& actual = actualLevels[i];
        const auto& wanted = expected.levels[i];
        if (actual.side != wanted.side || actual.price != wanted.price ||
            actual.totalQuantity != wanted.totalQuantity || actual.orderIDs != wanted.orderIDs) {
            differences.push_back("price level or FIFO order differs at " + std::to_string(wanted.price));
        }
    }
    const bool bidMatches = expected.hasBestBid == (liveBook.getBestBid() != nullptr) &&
        (!expected.hasBestBid || expected.bestBid == liveBook.getBestBid()->price);
    const bool askMatches = expected.hasBestAsk == (liveBook.getBestAsk() != nullptr) &&
        (!expected.hasBestAsk || expected.bestAsk == liveBook.getBestAsk()->price);
    if (!bidMatches) differences.push_back("best bid differs");
    if (!askMatches) differences.push_back("best ask differs");
    return differences;
}

ReplayEngine::ReplayEngine(const std::vector<Event>& events) : events_(events) {}
ReplayEngine::~ReplayEngine() = default;

ReplayResult ReplayEngine::replayTo(std::uint64_t sequence) {
    reset();
    std::uint64_t expected = 1;
    std::uint64_t lastApplied = 0;
    std::size_t nextEvent = 0;
    for (; nextEvent < events_.size(); ++nextEvent) {
        const Event& event = events_[nextEvent];
        if (event.sequenceID > sequence && sequence != UINT64_MAX) {
            if (expected <= sequence) {
                reset();
                return {false, event.sequenceID, "requested sequence is missing from the event log"};
            }
            break;
        }
        if (event.sequenceID != expected) {
            reset();
            return {false, event.sequenceID, "event sequence has a gap, duplicate, or does not start at one"};
        }
        const ReplayResult result = book_.apply(event);
        if (!result.success) {
            reset();
            return result;
        }
        lastApplied = event.sequenceID;
        expected++;
    }
    if (book_.hasPendingOrder() && nextEvent < events_.size() &&
        sequence != UINT64_MAX && events_[nextEvent].type == EventType::TRADE &&
        (events_[nextEvent].buyOrderID == book_.pendingOrderID() ||
         events_[nextEvent].sellOrderID == book_.pendingOrderID())) {
        reset();
        return {false, sequence, "replay boundary falls inside an incoming order execution group"};
    }
    const ReplayResult result = book_.finalize();
    if (!result.success) {
        reset();
        return result;
    }
    const bool beyondLast = sequence != UINT64_MAX && sequence > lastApplied;
    return {true, lastApplied, beyondLast ? "requested sequence is beyond the last event; replayed the last available state" : ""};
}

void ReplayEngine::reset() { book_.reset(); }
const ReplayBook& ReplayEngine::book() const { return book_; }
ReplaySnapshot ReplayEngine::snapshot() const { return book_.snapshot(); }
std::vector<std::string> ReplayEngine::compareWith(Book& liveBook) const { return book_.compareWith(liveBook); }
