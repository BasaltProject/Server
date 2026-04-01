#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::entity::ai {

/// Opening the doors on a mob's path and closing them behind it.
///
/// parity: `InteractWithDoor`, the core behaviour of every brain that walks through doors.
namespace doorInteraction {

using Block = std::array<std::int32_t, 3>;

/// The behaviour's own captures and the `DOORS_TO_CLOSE` memory.
struct State {
    std::optional<Block> lastCheckedNode;
    std::int32_t remainingCooldown = 0;
    std::vector<Block> doorsToClose;
};

/// Opens or closes the door at a block, with its sound, as `DoorBlock.setOpen` does.
using SetDoor = std::function<void(const Block& pos, bool open)>;

/// One turn. `nearest` is the mob's `NEAREST_LIVING_ENTITIES`, which may keep a door open for a
/// neighbour of the same kind coming through it.
void tick(world::WorldState& world, ServerEntity& mob, State& state, const std::vector<ServerEntity*>& nearest,
          const SetDoor& setDoor);

} // namespace doorInteraction

} // namespace basalt::server::runtime::entity::ai
