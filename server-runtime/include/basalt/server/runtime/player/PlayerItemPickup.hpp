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

struct PickedItem {
    world::DroppedItem item;
    /// How many of them the bag took. Never zero: a pickup that fits nowhere is not one.
    std::int32_t picked = 0;
    std::int32_t leftover = 0;
};

[[nodiscard]] std::vector<PickedItem> pickupNearbyItems(
    PlayerSession& session, world::WorldState& world,
    const std::function<bool(const PlayerSession&, const std::string&, std::int32_t, std::int32_t)>& allowed);

} // namespace basalt::server::runtime::player
