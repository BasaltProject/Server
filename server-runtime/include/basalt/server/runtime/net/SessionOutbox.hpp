#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace basalt::server::runtime::net {

class SessionOutbox {
public:
    using Frame = std::vector<std::uint8_t>;

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

    [[nodiscard]] std::deque<Frame> drain();

    void close();

private:
    std::int64_t limitBytes_;
    mutable std::mutex mutex_;
    std::deque<Frame> queue_;
    std::int64_t pending_ = 0;
    bool open_ = false;
    bool overflowed_ = false;
};

} // namespace basalt::server::runtime::net
