#include "EngineWorker.hpp"

#include <QFile>
#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <unordered_map>
#include <utility>

#include "OrderParser.hpp"
#include "CsvReader.hpp"

namespace {

std::vector<OrderOutcomeSnapshot> buildOutcomes(
    const std::vector<OrderCommand>& commands,
    const std::vector<std::pair<bool, QString>>& results,
    std::size_t firstTrade,
    const BookSnapshot& snapshot) {
    std::unordered_map<int, int> executed;
    for (std::size_t index = firstTrade; index < snapshot.trades.size(); ++index) {
        const Trade& trade = snapshot.trades[index];
        executed[trade.buyOrderID] += trade.quantity;
        executed[trade.sellOrderID] += trade.quantity;
    }

    std::vector<OrderOutcomeSnapshot> outcomes;
    outcomes.reserve(results.size());
    for (std::size_t index = 0; index < results.size(); ++index) {
        const OrderCommand& command = commands[index];
        const bool accepted = results[index].first;
        int remaining = 0;
        if (accepted) {
            for (const OrderSnapshot& order : snapshot.openOrders) {
                if (order.id == command.id) {
                    remaining = order.quantity;
                    break;
                }
            }
        }
        const int filled = accepted ? executed[command.id] : 0;
        QString reason = results[index].second;
        if (accepted) {
            if (filled >= command.quantity) {
                reason = QStringLiteral("Filled");
            } else if (command.type == OrderType::Market) {
                reason = filled == 0
                    ? QStringLiteral("Accepted; unfilled market remainder discarded")
                    : QStringLiteral("Partially filled; market remainder discarded");
            } else if (remaining > 0 && filled > 0) {
                reason = QStringLiteral("Partially filled; remainder resting");
            } else if (remaining > 0) {
                reason = QStringLiteral("Resting in book");
            } else {
                reason = QStringLiteral("Filled");
            }
        }
        outcomes.push_back({command.id, command.side, command.type,
                            command.quantity, filled, remaining, accepted, reason,
                            command.price, command.originalType, command.originalSide,
                            command.hasOriginalInput});
    }
    return outcomes;
}

}

EngineWorker::EngineWorker(QObject* parent) : QObject(parent) {
    qRegisterMetaType<BookSnapshot>();
    qRegisterMetaType<EngineReply>();
    qRegisterMetaType<std::vector<OrderCommand>>();
}

void EngineWorker::post(std::function<void()> operation) {
    if (!QMetaObject::invokeMethod(this, std::move(operation), Qt::QueuedConnection)) {
        emit ready(false, QStringLiteral("Could not queue an engine-worker operation."), {});
    }
}

void EngineWorker::initialize(bool fullSystemMode) {
    post([this, fullSystemMode] {
        try {
            service_ = std::make_unique<EngineService>();
            std::string error;
            const bool success = service_->reset(fullSystemMode, error);
            emit ready(success, QString::fromStdString(error),
                       success ? service_->snapshot() : BookSnapshot{});
        } catch (const std::exception& error) {
            emit ready(false, QString::fromUtf8(error.what()), {});
        }
    });
}

void EngineWorker::reset(bool fullSystemMode, std::uint64_t requestID) {
    post([this, fullSystemMode, requestID] {
        EngineReply reply;
        reply.requestID = requestID;
        try {
            std::string error;
            reply.success = service_ && service_->reset(fullSystemMode, error);
            reply.message = QString::fromStdString(error);
            if (reply.success) {
                replayEngine_.reset();
                replayEvents_.clear();
                replayCursor_ = 0;
                reply.snapshot = service_->snapshot();
            }
            if (!reply.success && reply.message.isEmpty()) {
                reply.message = QStringLiteral("Engine service is not initialized.");
            }
        } catch (const std::exception& error) {
            reply.message = QString::fromUtf8(error.what());
        }
        emit operationCompleted(reply);
    });
}

void EngineWorker::submit(OrderCommand command, std::uint64_t requestID) {
    post([this, command, requestID] {
        const auto started = std::chrono::steady_clock::now();
        EngineReply reply;
        reply.requestID = requestID;
        try {
            const Order order(command.id, command.price, command.quantity,
                              command.timestamp, command.side);
            const std::size_t firstTrade = service_->tradeCount();
            const SubmissionResult result = service_->submitOrder(order, command.type);
            reply.success = result.success;
            reply.message = result.success
                ? QStringLiteral("Accepted order %1").arg(command.id)
                : QString::fromStdString(result.reason);
            reply.submittedCount = 1;
            reply.acceptedCount = result.success ? 1 : 0;
            if (service_->fullSystemMode()) {
                const FeedDrainStatus drainStatus = service_->drainFeed();
                if (drainStatus == FeedDrainStatus::RecoveredOverflow) {
                    reply.message = QStringLiteral(
                        "Feed overflow recovered; event log remains authoritative.");
                } else if (drainStatus == FeedDrainStatus::InvalidConsumer ||
                           drainStatus == FeedDrainStatus::RecoveryFailed) {
                    reply.success = false;
                    reply.message = QStringLiteral("Could not drain the Worklet 3 feed.");
                }
            }
            reply.latencyMs.push_back(std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count());
            reply.snapshot = service_->snapshot();
            reply.outcomes = buildOutcomes({command},
                {{result.success, QString::fromStdString(result.reason)}},
                firstTrade, reply.snapshot);
        } catch (const std::exception& error) {
            reply.success = false;
            reply.message = QString::fromUtf8(error.what());
            reply.fatalError = true;
        }
        emit operationCompleted(reply);
    });
}

void EngineWorker::submitBatch(std::vector<OrderCommand> commands,
                               std::uint64_t requestID) {
    post([this, commands = std::move(commands), requestID]() mutable {
        EngineReply reply;
        reply.requestID = requestID;
        reply.batch = true;
        reply.success = true;
        reply.latencyMs.reserve(commands.size());
        std::vector<std::pair<bool, QString>> results;
        results.reserve(commands.size());
        const std::size_t firstTrade = service_->tradeCount();
        for (const OrderCommand& command : commands) {
            const auto started = std::chrono::steady_clock::now();
            bool stopBatch = false;
            try {
                const Order order(command.id, command.price, command.quantity,
                                  command.timestamp, command.side);
                const SubmissionResult result = service_->submitOrder(order, command.type);
                results.emplace_back(result.success,
                                     QString::fromStdString(result.reason));
                reply.success = reply.success && result.success;
                if (!result.success && reply.message.isEmpty()) {
                    reply.message = QStringLiteral("Order %1 rejected: %2")
                        .arg(command.id).arg(QString::fromStdString(result.reason));
                }
                ++reply.submittedCount;
                if (result.success) ++reply.acceptedCount;
                if (service_->fullSystemMode()) {
                    const FeedDrainStatus status = service_->drainFeed();
                    if (status == FeedDrainStatus::RecoveredOverflow &&
                        reply.message.isEmpty()) {
                        reply.message = QStringLiteral(
                            "Feed overflow recovered; event log remains authoritative.");
                    } else if ((status == FeedDrainStatus::InvalidConsumer ||
                                status == FeedDrainStatus::RecoveryFailed)) {
                        reply.success = false;
                        reply.fatalError = true;
                        reply.message = QStringLiteral("Could not drain the Worklet 3 feed.");
                        stopBatch = true;
                    }
                }
            } catch (const std::exception& error) {
                reply.success = false;
                reply.fatalError = true;
                reply.message = QString::fromUtf8(error.what());
                if (results.size() > reply.latencyMs.size()) {
                    reply.latencyMs.push_back(std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - started).count());
                }
                break;
            }
            reply.latencyMs.push_back(std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count());
            if (stopBatch) break;
        }
        try {
            reply.snapshot = service_->snapshot();
            reply.outcomes = buildOutcomes(commands, results, firstTrade, reply.snapshot);
        } catch (const std::exception& error) {
            reply.success = false;
            reply.fatalError = true;
            reply.message = QString::fromUtf8(error.what());
        }
        emit operationCompleted(reply);
    });
}

void EngineWorker::cancel(int orderID, std::uint64_t requestID) {
    post([this, orderID, requestID] {
        EngineReply reply;
        reply.requestID = requestID;
        try {
            reply.success = service_->cancelOrder(orderID);
            reply.message = reply.success
                ? QStringLiteral("Canceled order %1").arg(orderID)
                : QStringLiteral("Order %1 is not resting in the book.").arg(orderID);
            if (reply.success && service_->fullSystemMode()) {
                const FeedDrainStatus status = service_->drainFeed();
                if (status == FeedDrainStatus::InvalidConsumer ||
                    status == FeedDrainStatus::RecoveryFailed) {
                    reply.success = false;
                    reply.message = QStringLiteral("Could not drain the Worklet 3 feed.");
                }
            }
            reply.snapshot = service_->snapshot();
        } catch (const std::exception& error) {
            reply.message = QString::fromUtf8(error.what());
            reply.fatalError = true;
        }
        emit operationCompleted(reply);
    });
}

void EngineWorker::modify(int orderID, int price, int quantity,
                          std::uint64_t requestID) {
    post([this, orderID, price, quantity, requestID] {
        EngineReply reply;
        reply.requestID = requestID;
        try {
            reply.success = service_->modifyOrder(orderID, price, quantity);
            reply.message = reply.success
                ? QStringLiteral("Modified order %1").arg(orderID)
                : QStringLiteral("Order %1 could not be modified.").arg(orderID);
            if (reply.success && service_->fullSystemMode()) {
                const FeedDrainStatus status = service_->drainFeed();
                if (status == FeedDrainStatus::InvalidConsumer ||
                    status == FeedDrainStatus::RecoveryFailed) {
                    reply.success = false;
                    reply.message = QStringLiteral("Could not drain the Worklet 3 feed.");
                }
            }
            reply.snapshot = service_->snapshot();
        } catch (const std::exception& error) {
            reply.message = QString::fromUtf8(error.what());
            reply.fatalError = true;
        }
        emit operationCompleted(reply);
    });
}

void EngineWorker::parseOrders(QString path, std::uint64_t requestID) {
    post([this, path = std::move(path), requestID] {
        EngineReply reply;
        reply.requestID = requestID;
        try {
            const QString suffix = QFileInfo(path).suffix().toLower();
            if (suffix == QStringLiteral("json")) {
                std::vector<OrderRequest> requests;
                std::vector<ParserRejection> rejections;
                std::string error;
                if (!OrderParser::parseFile(path.toStdString(), requests, rejections, error)) {
                    reply.message = QString::fromStdString(error);
                    emit operationCompleted(reply);
                    return;
                }
                reply.parsedOrders.reserve(requests.size());
                for (const OrderRequest& request : requests) {
                    OrderCommand command{request.id, request.price,
                        request.quantity, 0, request.side, request.type, {}, {}, false};
                    command.originalType = QString::fromStdString(request.originalType);
                    command.originalSide = QString::fromStdString(request.originalSide);
                    command.hasOriginalInput = true;
                    reply.parsedOrders.push_back(std::move(command));
                }
                for (const ParserRejection& rejection : rejections) {
                    reply.parseWarnings.push_back(QStringLiteral("Record %1: %2")
                        .arg(rejection.requestIndex + 1)
                        .arg(QString::fromStdString(rejection.reason)));
                }
            } else if (suffix == QStringLiteral("csv")) {
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    reply.message = QStringLiteral("Could not open CSV: %1")
                        .arg(file.errorString());
                    emit operationCompleted(reply);
                    return;
                }
                if (file.atEnd()) {
                    reply.message = QStringLiteral("CSV file is empty.");
                    emit operationCompleted(reply);
                    return;
                }
                const QStringList required = {QStringLiteral("id"), QStringLiteral("type"),
                    QStringLiteral("side"), QStringLiteral("price"), QStringLiteral("quantity")};
                QStringList header;
                if (!parseCsvRow(csvLineText(file.readLine()), header) || header != required) {
                    reply.message = QStringLiteral(
                        "Order CSV header must be exactly: id,type,side,price,quantity");
                    emit operationCompleted(reply);
                    return;
                }
                std::size_t lineNumber = 1;
                while (!file.atEnd()) {
                    const QString line = csvLineText(file.readLine());
                    ++lineNumber;
                    if (line.trimmed().isEmpty()) continue;
                    QStringList fields;
                    if (!parseCsvRow(line, fields) || fields.size() != 5) {
                        reply.message = QStringLiteral(
                            "Malformed CSV record at line %1.").arg(lineNumber);
                        reply.parsedOrders.clear();
                        emit operationCompleted(reply);
                        return;
                    }
                    bool valid[3] = {false, false, false};
                    const int id = fields[0].toInt(&valid[0]);
                    const int price = fields[3].toInt(&valid[1]);
                    const int quantity = fields[4].toInt(&valid[2]);
                    if (!valid[0] || !valid[1] || !valid[2]) {
                        reply.message = QStringLiteral(
                            "Invalid integer field at CSV line %1.").arg(lineNumber);
                        reply.parsedOrders.clear();
                        emit operationCompleted(reply);
                        return;
                    }
                    const QString originalType = fields[1];
                    const QString originalSide = fields[2];
                    const QString typeText = originalType.trimmed().toUpper();
                    const QString sideText = originalSide.trimmed().toUpper();
                    const OrderType type = typeText == QStringLiteral("LIMIT")
                        ? OrderType::Limit : typeText == QStringLiteral("MARKET")
                        ? OrderType::Market : static_cast<OrderType>(-1);
                    const Side side = sideText == QStringLiteral("BUY")
                        ? Side::Buy : sideText == QStringLiteral("SELL")
                        ? Side::Sell : static_cast<Side>(-1);
                    OrderCommand command{id, price, quantity, 0, side, type, {}, {}, false};
                    command.originalType = originalType;
                    command.originalSide = originalSide;
                    command.hasOriginalInput = true;
                    reply.parsedOrders.push_back(std::move(command));
                }
            } else {
                reply.message = QStringLiteral("Unsupported order file type.");
                emit operationCompleted(reply);
                return;
            }
            reply.success = true;
            emit operationCompleted(reply);
        } catch (const std::exception& error) {
            reply.message = QString::fromUtf8(error.what());
            emit operationCompleted(reply);
        }
    });
}

void EngineWorker::flushEventLog(std::uint64_t requestID) {
    post([this, requestID] {
        EngineReply reply;
        reply.requestID = requestID;
        try {
            reply.success = service_ != nullptr;
            if (service_) {
                service_->flushEventLog();
                reply.snapshot = service_->snapshot();
            } else {
                reply.message = QStringLiteral("Engine service is not initialized.");
            }
        } catch (const std::exception& error) {
            reply.success = false;
            reply.message = QString::fromUtf8(error.what());
        }
        emit operationCompleted(reply);
    });
}

void EngineWorker::requestSnapshot() {
    post([this] {
        if (service_) emit snapshotReady(service_->snapshot());
    });
}

void EngineWorker::loadReplay(QString path, std::uint64_t requestID) {
    post([this, path = std::move(path), requestID] {
        EngineReply reply;
        reply.requestID = requestID;
        try {
            std::string error;
            if (!EventLogger::readEvents(path.toStdString(), replayEvents_, error)) {
                replayEngine_.reset();
                replayEvents_.clear();
                replayCursor_ = 0;
                reply.message = QString::fromStdString(error);
                emit operationCompleted(reply);
                return;
            }
            replayEngine_ = std::make_unique<ReplayEngine>(replayEvents_);
            replayCursor_ = 0;
            reply.success = true;
            constexpr std::size_t eventPageSize = 500;
            const std::size_t visibleCount = std::min(replayEvents_.size(), eventPageSize);
            reply.events.reserve(visibleCount);
            for (std::size_t index = 0; index < visibleCount; ++index) {
                const Event& event = replayEvents_[index];
                reply.events.push_back({event.sequenceID, event.type, event.side,
                    event.orderID, event.buyOrderID, event.sellOrderID,
                    event.price, event.quantity});
            }
            reply.replayEventPageStart = 0;
            reply.replayEventCount = replayEvents_.size();
            reply.message = QStringLiteral("Event log loaded.");
            emit operationCompleted(reply);
        } catch (const std::exception& error) {
            replayEngine_.reset();
            replayEvents_.clear();
            replayCursor_ = 0;
            reply.message = QString::fromUtf8(error.what());
            emit operationCompleted(reply);
        }
    });
}

void EngineWorker::loadReplayEventPage(std::size_t offset, std::size_t pageSize,
                                       std::uint64_t requestID) {
    post([this, offset, pageSize, requestID] {
        EngineReply reply;
        reply.requestID = requestID;
        if (!replayEngine_) {
            reply.message = QStringLiteral("Load an event log before browsing events.");
        } else if (offset > replayEvents_.size()) {
            reply.message = QStringLiteral("Requested event page is outside the log.");
        } else {
            const std::size_t count = std::min(
                replayEvents_.size() - offset, std::max<std::size_t>(1, pageSize));
            const std::size_t end = offset + count;
            reply.events.reserve(end - offset);
            for (std::size_t index = offset; index < end; ++index) {
                const Event& event = replayEvents_[index];
                reply.events.push_back({event.sequenceID, event.type, event.side,
                    event.orderID, event.buyOrderID, event.sellOrderID,
                    event.price, event.quantity});
            }
            reply.success = true;
            reply.replayEventPageStart = offset;
            reply.replayEventCount = replayEvents_.size();
            reply.message = QStringLiteral("Event page loaded.");
        }
        emit operationCompleted(reply);
    });
}

void EngineWorker::stepReplay(std::uint64_t requestID) {
    post([this, requestID] {
        EngineReply reply;
        reply.requestID = requestID;
        try {
            if (!replayEngine_) {
                reply.message = QStringLiteral("Load an event log before replaying.");
                emit operationCompleted(reply);
                return;
            }
            if (replayCursor_ >= replayEvents_.size()) {
                reply.success = true;
                reply.message = QStringLiteral("Replay is already at the end.");
                reply.replayCursor = replayCursor_;
                reply.replayEventCount = replayEvents_.size();
                emit operationCompleted(reply);
                return;
            }
            std::size_t end = replayCursor_ + 1;
            while (end < replayEvents_.size() &&
                   replayEvents_[end].type != EventType::NEW_ORDER) {
                ++end;
            }
            const ReplayResult result = replayEngine_->replayNextOrderBoundary();
            reply.success = result.success;
            reply.replaySequence = result.sequence;
            if (!result.success) {
                reply.message = QStringLiteral("Replay failed at sequence %1: %2")
                    .arg(result.sequence).arg(QString::fromStdString(result.message));
            } else {
                replayCursor_ = end;
                reply.replayCursor = replayCursor_;
                reply.replayEventCount = replayEvents_.size();
                reply.replaySnapshot = replayEngine_->snapshot();
                reply.hasReplaySnapshot = true;
                reply.message = QStringLiteral("Replayed through sequence %1.")
                    .arg(result.sequence);
            }
            emit operationCompleted(reply);
        } catch (const std::exception& error) {
            reply.message = QString::fromUtf8(error.what());
            reply.fatalError = true;
            emit operationCompleted(reply);
        }
    });
}

void EngineWorker::resetReplay(std::uint64_t requestID) {
    post([this, requestID] {
        EngineReply reply;
        reply.requestID = requestID;
        if (!replayEngine_) {
            reply.message = QStringLiteral("Load an event log before resetting replay.");
        } else {
            replayEngine_->reset();
            replayCursor_ = 0;
            reply.success = true;
            reply.replayCursor = 0;
            reply.replayEventCount = replayEvents_.size();
            reply.replaySnapshot = replayEngine_->snapshot();
            reply.hasReplaySnapshot = true;
            reply.message = QStringLiteral("Replay reset to the beginning.");
        }
        emit operationCompleted(reply);
    });
}

void EngineWorker::fastForwardReplay(std::uint64_t requestID) {
    post([this, requestID] {
        EngineReply reply;
        reply.requestID = requestID;
        if (!replayEngine_) {
            reply.message = QStringLiteral("Load an event log before replaying.");
        } else {
            const ReplayResult result = replayEngine_->replayTo();
            reply.success = result.success;
            reply.replaySequence = result.sequence;
            reply.replayEventCount = replayEvents_.size();
            if (result.success) {
                replayCursor_ = replayEvents_.size();
                reply.replayCursor = replayCursor_;
                reply.replaySnapshot = replayEngine_->snapshot();
                reply.hasReplaySnapshot = true;
                reply.message = QStringLiteral("Fast-forwarded through sequence %1.")
                    .arg(result.sequence);
            } else {
                reply.message = QStringLiteral("Replay failed at sequence %1: %2")
                    .arg(result.sequence).arg(QString::fromStdString(result.message));
            }
        }
        emit operationCompleted(reply);
    });
}

void EngineWorker::validateReplay(std::uint64_t requestID) {
    post([this, requestID] {
        EngineReply reply;
        reply.requestID = requestID;
        if (!replayEngine_) {
            reply.message = QStringLiteral("Open an event log before validating.");
            emit operationCompleted(reply);
            return;
        }
        try {
            const ReplayResult result = replayEngine_->replayTo();
            reply.success = result.success;
            reply.replaySequence = result.sequence;
            if (result.success) {
                reply.replaySnapshot = replayEngine_->snapshot();
                reply.hasReplaySnapshot = true;
                replayCursor_ = replayEvents_.size();
                reply.replayCursor = replayCursor_;
                reply.replayEventCount = replayEvents_.size();
            } else {
                reply.fatalError = true;
            }
            if (!result.success) {
                reply.message = QStringLiteral("Replay failed at sequence %1: %2")
                    .arg(result.sequence).arg(QString::fromStdString(result.message));
            } else {
                for (const std::string& difference :
                     service_->compareWithLiveBook(*replayEngine_)) {
                    reply.differences.push_back(QString::fromStdString(difference));
                }
                reply.success = reply.differences.empty();
                reply.message = reply.success
                    ? QStringLiteral("Reconstructed state matches the live book.")
                    : QStringLiteral("Reconstructed state differs from the live book.");
            }
            reply.snapshot = service_->snapshot();
        } catch (const std::exception& error) {
            reply.message = QString::fromUtf8(error.what());
            reply.fatalError = true;
        }
        emit operationCompleted(reply);
    });
}
