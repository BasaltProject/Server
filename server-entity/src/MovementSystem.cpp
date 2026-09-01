#include <basalt/server/entity/ecs/MovementSystem.hpp>

namespace basalt::server::entity::ecs {

void applyVelocity(ComponentStore<component::Position>& positions, ComponentStore<component::Velocity>& velocities) {
    velocities.forEach([&positions](std::int32_t entityIndex, const component::Velocity& velocity) {
        component::Position* position = positions.get(entityIndex);
        if (position == nullptr) {
            return;
        }
        position->x += velocity.dx;
        position->y += velocity.dy;
        position->z += velocity.dz;
    });
}

} // namespace basalt::server::entity::ecs
