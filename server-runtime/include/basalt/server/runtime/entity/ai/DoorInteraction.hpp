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

/// parity: `InteractWithDoor`.
namespace doorInteraction {

using Block = std::array<std::int32_t, 3>;

struct State {
    std::optional<Block> lastCheckedNode;
    std::int32_t remainingCooldown = 0;
    std::vector<Block> doorsToClose;
};

using SetDoor = std::function<void(const Block& pos, bool open)>;

void tick(world::WorldState& world, ServerEntity& mob, State& state, const std::vector<ServerEntity*>& nearest,
          const SetDoor& setDoor);

} // namespace doorInteraction

} // namespace basalt::server::runtime::entity::ai
