#pragma once

#include <basalt/server/runtime/world/DroppedItem.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::player {

class PlayerSession;

/// One stack that went into somebody's bag, and what stayed on the floor of it.
struct PickedItem {
    world::DroppedItem item;
    /// How many of them the bag took. Never zero: a pickup that fits nowhere is not one.
    std::int32_t picked = 0;
    /// How many are still lying there, as the same entity.
    std::int32_t leftover = 0;
};

/// Takes what this player is standing on, as far as the world and the bag are concerned. What the
/// two editions do about it after that, the animation and the slots, is their own.
///
/// parity: `Player.take` over `ItemEntity.playerTouch`, pickup delay included.
[[nodiscard]] std::vector<PickedItem> pickupNearbyItems(
    PlayerSession& session, world::WorldState& world,
    const std::function<bool(const PlayerSession&, const std::string&, std::int32_t, std::int32_t)>& allowed);

} // namespace basalt::server::runtime::player
