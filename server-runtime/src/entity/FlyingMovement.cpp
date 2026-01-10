#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/FlyingMovement.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::entity {
namespace {

/// parity: the 0.91 the air keeps every tick, in `travelFlying` and in every flier's move control.
constexpr double kDrag = 0.91;

/// parity: how near counts as arrived, which is what stops a flier jittering over its target.
constexpr double kArrived = 0.3;

} // namespace

namespace flyingMovement {

bool approach(ServerEntity& entity, double x, double y, double z, double step) {
    const double dx = x - entity.x;
    const double dy = y - entity.y;
    const double dz = z - entity.z;
    const double distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    if (distance < kArrived) {
        return false;
    }
    entity.velocityX += dx / distance * step;
    entity.velocityY += dy / distance * step;
    entity.velocityZ += dz / distance * step;
    return true;
}

void tick(world::WorldState& world, ServerEntity& entity) {
    const Aabb box = Aabb::of(entity.x, entity.y, entity.z, entity.width(), entity.height());
    const entityCollision::BlockAt blockAt = [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
        return world.blockState(x, y, z);
    };
    const std::array<double, 3> moved =
        entityCollision::collide(blockAt, box, entity.velocityX, entity.velocityY, entity.velocityZ);
    entity.x += moved[0];
    entity.y += moved[1];
    entity.z += moved[2];
    entity.horizontallyBlocked = moved[0] != entity.velocityX || moved[2] != entity.velocityZ;
    if (moved[0] != entity.velocityX) {
        entity.velocityX = 0.0;
    }
    if (moved[1] != entity.velocityY) {
        entity.velocityY = 0.0;
    }
    if (moved[2] != entity.velocityZ) {
        entity.velocityZ = 0.0;
    }
    entity.velocityX *= kDrag;
    entity.velocityY *= kDrag;
    entity.velocityZ *= kDrag;
    // Nothing that flies is ever standing on anything, whatever it just bumped into.
    entity.onGround = false;
}

} // namespace flyingMovement

namespace flyers {
namespace {

constexpr auto kFlying = std::to_array<std::string_view>({"ghast", "happy_ghast", "ghastling", "blaze", "phantom",
                                                          "vex", "bat", "allay", "wither", "ender_dragon", "breeze"});

} // namespace

bool flies(const wire::common::Identifier& type) { return std::ranges::find(kFlying, type.path()) != kFlying.end(); }

} // namespace flyers

} // namespace basalt::server::runtime::entity
