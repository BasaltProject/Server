#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>

#include <cstdint>
#include <set>

namespace basalt::server::runtime::player {
class PlayerRegistry;
}

namespace basalt::server::runtime {

/// parity: `ServerBossEvent`.
class BossBarView {
public:
    explicit BossBarView(wire::protocol::play::BossEventPacket::Add style);

    [[nodiscard]] float progress() const noexcept { return style_.progress; }

    void track(player::PlayerRegistry& registry, const std::set<std::int32_t>& players);

    void setProgress(player::PlayerRegistry& registry, float progress);

    /// parity: `ServerBossEvent.removeAllPlayers`.
    void clear(player::PlayerRegistry& registry);

private:
    void send(player::PlayerRegistry& registry, std::int32_t playerId,
              const wire::protocol::play::BossEventPacket::Operation& operation) const;

    wire::common::Uuid id_ = wire::common::Uuid::random();
    wire::protocol::play::BossEventPacket::Add style_;
    std::set<std::int32_t> players_;
};

} // namespace basalt::server::runtime
