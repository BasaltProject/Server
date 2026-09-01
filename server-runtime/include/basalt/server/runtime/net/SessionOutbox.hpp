#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace basalt::server::runtime::net {

/// The frames waiting to go out on one connection.
///
/// Sockets here block, so whoever writes to a client that has stopped draining is parked until it
/// drains. That thread must never be the tick thread: a single slow client would hold up the world.
/// The tick thread hands its frames here instead, and the connection's own outbound thread is the
/// only one that touches the socket.
///
/// Order is the queue's order, which is why every frame goes through it once it is open: a writer
/// that bypassed it would interleave with what is waiting and the client would read them swapped.
class SessionOutbox {
public:
    using Frame = std::vector<std::uint8_t>;

    /// Eight megabytes of unsent frames is a client that is never coming back.
    static constexpr std::int64_t kDefaultLimitBytes = 8LL * 1024 * 1024;

    /// `limitBytes` is how much may wait before the client counts as not draining.
    explicit SessionOutbox(std::int64_t limitBytes = kDefaultLimitBytes) : limitBytes_(limitBytes) {}

    SessionOutbox(const SessionOutbox&) = delete;
    SessionOutbox& operator=(const SessionOutbox&) = delete;
    SessionOutbox(SessionOutbox&&) = delete;
    SessionOutbox& operator=(SessionOutbox&&) = delete;
    ~SessionOutbox() = default;

    /// Whether frames are being queued rather than written straight to the socket.
    [[nodiscard]] bool isOpen() const;

    /// How many bytes are waiting, for the timings and for the overflow check.
    [[nodiscard]] std::int64_t pendingBytes() const;

    /// Whether the client stopped draining and the connection has to go.
    [[nodiscard]] bool hasOverflowed() const;

    /// From here on frames are queued. Called when the outbound thread starts, and only then.
    void open();

    /// Queues one frame. Answers false when the outbox is not open yet, so the caller writes it
    /// straight to the socket as it did before there was an outbox (login and configuration).
    bool offer(Frame frame);

    /// Takes everything waiting, in order. The caller writes it and is the only one that does, so
    /// the cipher stays in send order even though the frames came from several threads.
    [[nodiscard]] std::deque<Frame> drain();

    /// Nothing else goes out: the connection is gone.
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
