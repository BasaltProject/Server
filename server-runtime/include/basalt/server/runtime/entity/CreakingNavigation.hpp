#pragma once

#include <basalt/server/runtime/entity/PathTypes.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// The creaking's own way of getting about.
///
/// parity: `Creaking.CreakingPathNavigation`: a floating, door-passing `GroundPathNavigation` with
/// the `HomeNodeEvaluator` that keeps it near its heart, idle while the creaking is frozen.
namespace creakingNavigation {

/// parity: `Creaking.setTransient`'s maluses for one a heart keeps, else the defaults.
[[nodiscard]] pathTypes::Maluses maluses(bool heartBound);

/// parity: `createPath(target, reachRange)` then `moveTo(path, speed)`. Answers whether it now walks a path.
bool moveTo(world::WorldState& world, ServerEntity& creaking, double x, double y, double z, std::int32_t reachRange,
            double speedModifier);

/// parity: `CreakingPathNavigation.tick` over `PathNavigation.tick`.
void tick(world::WorldState& world, ServerEntity& creaking);

} // namespace creakingNavigation

} // namespace basalt::server::runtime::entity
