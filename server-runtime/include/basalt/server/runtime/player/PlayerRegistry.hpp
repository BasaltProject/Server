#pragma once

#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <mutex>
#include <unordered_map>
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

    [[nodiscard]] PlayerSession* get(const wire::common::Uuid& uuid) const;

    /// A point-in-time copy: safe to iterate without holding the lock, and while it may itself
    /// change under the caller.
    [[nodiscard]] std::vector<PlayerSession*> snapshot() const;

private:
    mutable std::mutex lock_;
    /// Kept in join order, which is the order vanilla lists players in.
    std::vector<PlayerSession*> order_;
    std::unordered_map<wire::common::Uuid, PlayerSession*> sessions_;
    std::int32_t nextEntityId_ = 1;
};

} // namespace basalt::server::runtime::player
