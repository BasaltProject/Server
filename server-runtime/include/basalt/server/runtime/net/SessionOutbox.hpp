#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace basalt::server::runtime::net {

class SessionOutbox {
public:
    using Frame = std::vector<std::uint8_t>;
    using SharedFrame = std::shared_ptr<const Frame>;

    /// Either bytes of its own or a frame built once for many connections.
    struct Entry {
        Frame own;
        SharedFrame shared;

        [[nodiscard]] std::span<const std::uint8_t> bytes() const {
            return shared != nullptr ? std::span<const std::uint8_t>{*shared} : std::span<const std::uint8_t>{own};
        }
    };

    /// Eight megabytes of unsent frames is a client that is never coming back.
    static constexpr std::int64_t kDefaultLimitBytes = 8LL * 1024 * 1024;

    explicit SessionOutbox(std::int64_t limitBytes = kDefaultLimitBytes) : limitBytes_(limitBytes) {}

    SessionOutbox(const SessionOutbox&) = delete;
    SessionOutbox& operator=(const SessionOutbox&) = delete;
    SessionOutbox(SessionOutbox&&) = delete;
    SessionOutbox& operator=(SessionOutbox&&) = delete;
    ~SessionOutbox() = default;

    [[nodiscard]] bool isOpen() const;

    [[nodiscard]] std::int64_t pendingBytes() const;

    [[nodiscard]] bool hasOverflowed() const;

    /// From here on frames are queued. Called when the outbound thread starts, and only then.
    void open();

    bool offer(Frame frame);

    bool offerShared(SharedFrame frame);

    [[nodiscard]] std::deque<Entry> drain();

    void close();

private:
    std::int64_t limitBytes_;
    mutable std::mutex mutex_;
    std::deque<Entry> queue_;
    std::int64_t pending_ = 0;
    std::atomic<bool> open_{false};
    bool overflowed_ = false;
};

} // namespace basalt::server::runtime::net
