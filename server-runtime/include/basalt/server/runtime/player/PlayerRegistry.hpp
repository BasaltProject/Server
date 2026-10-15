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
class PlayerRegistry {
public:
    [[nodiscard]] std::int32_t allocateEntityId();

    [[nodiscard]] std::int32_t allocateEntityIds(std::int32_t count);

    void add(PlayerSession* session);

    void remove(const wire::common::Uuid& uuid);

    void retire(std::unique_ptr<PlayerSession> session);

    void keepAlive(std::shared_ptr<void> owner);

    void collect();

    [[nodiscard]] PlayerSession* get(const wire::common::Uuid& uuid) const;

    [[nodiscard]] std::vector<PlayerSession*> snapshot() const;

private:
    static constexpr std::chrono::seconds kRetirement{2};

    mutable std::mutex lock_;
    /// Kept in join order, which is the order vanilla lists players in.
    std::vector<PlayerSession*> order_;
    std::unordered_map<wire::common::Uuid, PlayerSession*> sessions_;
    std::int32_t nextEntityId_ = 1;
    std::vector<std::pair<std::chrono::steady_clock::time_point, std::unique_ptr<PlayerSession>>> retired_;
    std::vector<std::pair<std::chrono::steady_clock::time_point, std::shared_ptr<void>>> kept_;
};

} // namespace basalt::server::runtime::player
