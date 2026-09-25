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

/// A player's crossbow: charging it, what it loads, and shooting what it loaded.
///
/// parity: `CrossbowItem` over `ProjectileWeaponItem`.
namespace crossbow {

/// The sounds already played for the charge in progress. parity: `startSoundPlayed` and
/// `midLoadSoundPlayed`.
struct Charging {
    bool startPlayed = false;
    bool midPlayed = false;
};

[[nodiscard]] bool isCrossbow(const storage::StoredItemStack& stack);

/// parity: `CrossbowItem.isCharged`.
[[nodiscard]] bool isCharged(const storage::StoredItemStack& stack);

/// parity: `Player.getProjectile` for a crossbow: an arrow or a rocket held in either hand, the
/// off hand first, then the first arrow in the bag, and a plain arrow for infinite materials.
[[nodiscard]] std::optional<std::int32_t> ammoSlot(const PlayerSession& player);

/// parity: `CrossbowItem.onUseTick`: the start and middle sounds, and the load once fully drawn.
void tickCharging(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t slot,
                  std::int32_t timeHeld, Charging& charging);

/// parity: `CrossbowItem.performShooting` from a player's hand, at 3.15 for arrows and 1.6 for
/// rockets.
void shoot(const BreakContext& context, FireworkSystem* fireworks, PlayerSession& player, world::LoadedWorld& world,
           std::int32_t slot);

} // namespace crossbow

} // namespace basalt::server::runtime::player
