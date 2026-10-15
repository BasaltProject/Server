#pragma once

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::player {

class PlayerSession;

struct PickedArrow {
    std::int32_t entityId = 0;
};

/// parity: `AbstractArrow.playerTouch`.
[[nodiscard]] std::vector<PickedArrow> pickupNearbyArrows(PlayerSession& session, world::WorldState& world);

} // namespace basalt::server::runtime::player
