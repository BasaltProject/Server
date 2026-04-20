#pragma once

#include <basalt/server/entity/component/Components.hpp>
#include <basalt/server/entity/ecs/ComponentStore.hpp>

namespace basalt::server::entity::ecs {
void applyVelocity(ComponentStore<component::Position>& positions, ComponentStore<component::Velocity>& velocities);
} // namespace basalt::server::entity::ecs
