#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// One tick of movement for something that falls.
///
/// parity: `Entity.applyGravity`, `Entity.move` and the drag each class puts on afterwards. The
/// numbers are per entity (an item falls at 0.04 a tick, an arrow at 0.05), so they arrive as
/// arguments rather than being guessed here; what is shared is the order: gravity, then the move
/// against the blocks, then the drag on what is left of it.
namespace entityPhysics {

/// parity: `Entity.getAirDrag`, the 0.98 that nearly everything keeps.
inline constexpr double kAirDrag = 0.98;

/// Moves the entity by its own velocity, with `gravity` taken off it first.
///
/// `horizontalGroundDrag` is what a landing costs sideways (a block of TNT keeps 0.7 of it), and
/// `verticalGroundDrag` what a landing does to the fall (vanilla's own -0.5 bounce).
void move(world::WorldState& world, ServerEntity& entity, double gravity, double drag = kAirDrag,
          double horizontalGroundDrag = 1.0, double verticalGroundDrag = 1.0);

} // namespace entityPhysics

} // namespace basalt::server::runtime::entity
