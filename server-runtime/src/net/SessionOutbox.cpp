#include <basalt/server/runtime/net/SessionOutbox.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::runtime::net {

bool SessionOutbox::isOpen() const { return open_.load(std::memory_order_acquire); }

std::int64_t SessionOutbox::pendingBytes() const {
    const std::lock_guard<std::mutex> lock{mutex_};
    return pending_;
}

bool SessionOutbox::hasOverflowed() const {
    const std::lock_guard<std::mutex> lock{mutex_};
    return overflowed_;
}

void SessionOutbox::open() {
    const std::lock_guard<std::mutex> lock{mutex_};
    open_.store(true, std::memory_order_release);
}

bool SessionOutbox::offer(Frame frame) {
    const std::lock_guard<std::mutex> lock{mutex_};
    if (!open_.load(std::memory_order_relaxed)) {
        return false;
    }
    if (pending_ + static_cast<std::int64_t>(frame.size()) > limitBytes_) {
        // Dropping frames would desynchronize the client, so stop queueing and let the connection close.
        overflowed_ = true;
        return true;
    }
    pending_ += static_cast<std::int64_t>(frame.size());
    queue_.push_back(Entry{.own = std::move(frame), .shared = nullptr});
    return true;
}

bool SessionOutbox::offerShared(SharedFrame frame) {
    const std::lock_guard<std::mutex> lock{mutex_};
    if (!open_.load(std::memory_order_relaxed)) {
        return false;
    }
    if (pending_ + static_cast<std::int64_t>(frame->size()) > limitBytes_) {
        overflowed_ = true;
        return true;
    }
    pending_ += static_cast<std::int64_t>(frame->size());
    queue_.push_back(Entry{.own = {}, .shared = std::move(frame)});
    return true;
}

std::deque<SessionOutbox::Entry> SessionOutbox::drain() {
    const std::lock_guard<std::mutex> lock{mutex_};
    std::deque<Entry> taken;
    taken.swap(queue_);
    pending_ = 0;
    return taken;
}

void SessionOutbox::close() {
    const std::lock_guard<std::mutex> lock{mutex_};
    open_.store(false, std::memory_order_release);
    queue_.clear();
    pending_ = 0;
}

} // namespace basalt::server::runtime::net
