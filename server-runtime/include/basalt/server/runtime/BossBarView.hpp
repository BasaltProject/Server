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

    [[nodiscard]] const wire::protocol::play::BossEventPacket::Add& style() const noexcept { return style_; }

    [[nodiscard]] const std::set<std::int32_t>& players() const noexcept { return players_; }

    [[nodiscard]] bool visible() const noexcept { return visible_; }

    /// The players in range; a pinned one is shown too, and one sent away stays away until it leaves the range.
    void track(player::PlayerRegistry& registry, const std::set<std::int32_t>& players);

    void setName(player::PlayerRegistry& registry, wire::text::Component name);

    void setLook(player::PlayerRegistry& registry, wire::protocol::play::BossBarColor color,
                 wire::protocol::play::BossBarOverlay overlay);

    void setFlags(player::PlayerRegistry& registry, bool darkenScreen, bool playMusic, bool createWorldFog);

    void setVisible(player::PlayerRegistry& registry, bool visible);

    /// parity: `ServerBossEvent.addPlayer` and `removePlayer`.
    void pin(player::PlayerRegistry& registry, std::int32_t playerId, bool shown);

    void setProgress(player::PlayerRegistry& registry, float progress);

    /// parity: `ServerBossEvent.removeAllPlayers`.
    void clear(player::PlayerRegistry& registry);

private:
    void send(player::PlayerRegistry& registry, std::int32_t playerId,
              const wire::protocol::play::BossEventPacket::Operation& operation) const;

    void apply(player::PlayerRegistry& registry);

    wire::common::Uuid id_ = wire::common::Uuid::random();
    wire::protocol::play::BossEventPacket::Add style_;
    std::set<std::int32_t> players_;
    std::set<std::int32_t> inRange_;
    std::set<std::int32_t> pinned_;
    std::set<std::int32_t> sentAway_;
    bool visible_ = true;
};

} // namespace basalt::server::runtime
