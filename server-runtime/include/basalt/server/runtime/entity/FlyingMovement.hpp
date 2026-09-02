#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// How something that flies moves.
///
/// parity: `FlyingMoveControl` with `Mob.travel`'s flying branch: nothing falls, the mob is nudged
/// towards where it wants to be a tenth of the distance at a time, and the air keeps 0.91 of what is
/// left every tick. A ghast drifts because of that, not because anything steers it.
namespace flyingMovement {

/// Leans towards a point. Answers false once it is there.
///
/// parity: every flier's move control adds to the speed rather than setting a position, and each has
/// its own number for how much: the ghast's 0.1 impulse, the vex's 0.05, and the 0.02 of the air
/// input everything else flies on. That number is `step`, and with the drag it is what decides how
/// fast the mob ends up going: about eleven times the step, and no faster.
bool approach(ServerEntity& entity, double x, double y, double z, double step);

/// The one move a flying mob makes each tick, whatever its goals decided.
void tick(world::WorldState& world, ServerEntity& entity);

} // namespace flyingMovement

/// The mobs that fly rather than walk, which is what decides how they are moved every tick.
namespace flyers {

[[nodiscard]] bool flies(const wire::common::Identifier& type);

} // namespace flyers

} // namespace basalt::server::runtime::entity
