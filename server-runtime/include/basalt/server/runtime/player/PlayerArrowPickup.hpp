#pragma once

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::player {

class PlayerSession;

/// One arrow that went back into somebody's quiver.
struct PickedArrow {
    std::int32_t entityId = 0;
};

/// Takes the arrows this player is standing on, as far as the world and the bag are concerned. The
/// animation and the slots are each edition's own, the same as for an item on the floor.
///
/// parity: `AbstractArrow.playerTouch`: only a stuck arrow is picked up, it goes whole or not at
/// all, and only as its `pickup` allows.
[[nodiscard]] std::vector<PickedArrow> pickupNearbyArrows(PlayerSession& session, world::WorldState& world);

} // namespace basalt::server::runtime::player
