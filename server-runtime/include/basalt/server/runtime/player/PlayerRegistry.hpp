#pragma once

#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace basalt::server::runtime::player {

/// The set of currently connected players, shared across every connection's own thread.
///
/// An instance passed in by the server, not a singleton: callers that need it get it through their
/// constructor rather than a static accessor.
class PlayerRegistry {
public:
    [[nodiscard]] std::int32_t allocateEntityId();

    void add(PlayerSession* session);

    void remove(const wire::common::Uuid& uuid);

    /// Takes a session that has left and holds it until `collect` lets it go. `snapshot` hands out
    /// plain pointers and the thread that took one is not the thread that ends a connection, so
    /// freeing the session there is a read of freed memory milliseconds later: measured with the
    /// sanitiser, a Bedrock client leaving while the world ticked took the whole server down.
    void retire(std::unique_ptr<PlayerSession> session);

    /// Lets go of what has been retired. Call it from the tick thread between turns: nothing that
    /// thread handed out can still be in use there, which is what makes it safe rather than likely.
    /// The other threads that take a snapshot are covered by the floor below, since they share no
    /// sequence point with it.
    void collect();

    [[nodiscard]] PlayerSession* get(const wire::common::Uuid& uuid) const;

    /// A point-in-time copy: safe to iterate without holding the lock, and while it may itself
    /// change under the caller.
    [[nodiscard]] std::vector<PlayerSession*> snapshot() const;

private:
    /// The least a retired session is kept, for the threads that do not share a sequence point
    /// with the collector: two seconds against a loop that goes round forty times in one.
    static constexpr std::chrono::seconds kRetirement{2};

    mutable std::mutex lock_;
    /// Kept in join order, which is the order vanilla lists players in.
    std::vector<PlayerSession*> order_;
    std::unordered_map<wire::common::Uuid, PlayerSession*> sessions_;
    std::int32_t nextEntityId_ = 1;
    /// Sessions that have left, with the moment they did. Swept whenever the list is read.
    std::vector<std::pair<std::chrono::steady_clock::time_point, std::unique_ptr<PlayerSession>>> retired_;
};

} // namespace basalt::server::runtime::player
