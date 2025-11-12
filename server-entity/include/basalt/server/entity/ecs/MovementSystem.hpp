#pragma once

#include <basalt/server/entity/component/Components.hpp>
#include <basalt/server/entity/ecs/ComponentStore.hpp>

namespace basalt::server::entity::ecs {

/// Advances every entity that has both a position and a velocity by one tick. Iterates the velocity
/// store, since every entity here has at most as much velocity data as position data, and looks the
/// other up by entity index: the standard sparse-set join, with no per-entity allocation.
void applyVelocity(ComponentStore<component::Position>& positions, ComponentStore<component::Velocity>& velocities);

} // namespace basalt::server::entity::ecs
