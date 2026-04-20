#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `Entity.applyGravity`.
namespace entityPhysics {

/// parity: `Entity.getAirDrag`, the 0.98 that nearly everything keeps.
inline constexpr double kAirDrag = 0.98;

void move(world::WorldState& world, ServerEntity& entity, double gravity, double drag = kAirDrag,
          double horizontalGroundDrag = 1.0, double verticalGroundDrag = 1.0);

} // namespace entityPhysics

} // namespace basalt::server::runtime::entity
