#pragma once

#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime {
class FireworkSystem;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

class PlayerSession;

namespace crossbow {

struct Charging {
    bool startPlayed = false;
    bool midPlayed = false;
};

[[nodiscard]] bool isCrossbow(const storage::StoredItemStack& stack);

[[nodiscard]] bool isCharged(const storage::StoredItemStack& stack);

[[nodiscard]] std::optional<std::int32_t> ammoSlot(const PlayerSession& player);

void tickCharging(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t slot,
                  std::int32_t timeHeld, Charging& charging);

void shoot(const BreakContext& context, FireworkSystem* fireworks, PlayerSession& player, world::LoadedWorld& world,
           std::int32_t slot);

} // namespace crossbow

} // namespace basalt::server::runtime::player
