
#include "EventLogger.hpp"
#include <utility>

void EventLogger::writerLoop() {
    std::size_t count = 0;

    while (true) {
        std::string line;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] {
                return stopping_ || !writeQueue_.empty();
            });

            if (writeQueue_.empty() && stopping_) break;

            line = std::move(writeQueue_.front());
            writeQueue_.pop_front();
            writerActive_ = true;
        }

        file_ << line << '\n';

        if (!file_) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                writeFailed_ = true;
                writerActive_ = false;
                writeQueue_.clear();
            }
            condition_.notify_all();
            break;
        }

        if (++count >= 256) {
            file_.flush();
            count = 0;

            if (!file_) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    writeFailed_ = true;
                    writerActive_ = false;
                    writeQueue_.clear();
                }
                condition_.notify_all();
                break;
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            writerActive_ = false;
        }
        condition_.notify_all();
    }

    file_.flush();

    if (!file_) {
        std::lock_guard<std::mutex> lock(mutex_);
        writeFailed_ = true;
    }
    condition_.notify_all();
}

bool EventLogger::readEvents(
    const std::string& name,
    std::vector<Event>& events,
    std::string& error
) {
    events.clear();
    error.clear();

    std::ifstream file(name, std::ios::in | std::ios::binary);
    if (!file.is_open()) return true;

    std::string line;
    std::uint64_t expected = 1;

    while (std::getline(file, line)) {
        Event e;

        if (!deserialize(line, e) || e.sequenceID != expected) {
            error = "Invalid event record or sequence.";
            return false;
        }

        events.push_back(e);
        ++expected;
    }

    if (file.bad()) {
        error = "Error reading event log.";
        return false;
    }

    return true;
}
