#pragma once

#include <basalt/server/runtime/entity/PathTypes.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `Creaking.CreakingPathNavigation`.
namespace creakingNavigation {

/// parity: `Creaking.setTransient`'s maluses for one a heart keeps, else the defaults.
[[nodiscard]] pathTypes::Maluses maluses(bool heartBound);

/// parity: `createPath(target, reachRange)` then `moveTo(path, speed)`.
bool moveTo(world::WorldState& world, ServerEntity& creaking, double x, double y, double z, std::int32_t reachRange,
            double speedModifier);

/// parity: `CreakingPathNavigation.tick` over `PathNavigation.tick`.
void tick(world::WorldState& world, ServerEntity& creaking);

} // namespace creakingNavigation

} // namespace basalt::server::runtime::entity
