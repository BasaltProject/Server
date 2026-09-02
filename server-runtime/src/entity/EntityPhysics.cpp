#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/EntityPhysics.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>

#include <cmath>

namespace basalt::server::runtime::entity::entityPhysics {
namespace {

/// parity: `Entity.getBlockBounciness`, which gives anything that is not alive four fifths of it.
constexpr float kNonLivingBounceShare = 0.8F;

/// parity: `getBlockPosBelowThatAffectsMyMovement`, which looks half a block under the feet.
constexpr double kFrictionProbeDepth = 0.5;

/// parity: the block whose bounce the entity feels, which is not always the one it stands in.
[[nodiscard]] std::int32_t blockUnder(world::WorldState& world, const ServerEntity& entity) {
    return world
        .blockState(static_cast<std::int32_t>(std::floor(entity.x)),
                    static_cast<std::int32_t>(std::floor(entity.y - kFrictionProbeDepth)),
                    static_cast<std::int32_t>(std::floor(entity.z)))
        .value_or(0);
}

} // namespace

void move(world::WorldState& world, ServerEntity& entity, double gravity, double drag, double horizontalGroundDrag,
          double verticalGroundDrag) {
    entity.velocityY -= gravity;

    const Aabb box = Aabb::of(entity.x, entity.y, entity.z, entity.width(), entity.height());
    const entityCollision::BlockAt blockAt = [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
        return world.blockState(x, y, z);
    };
    const std::array<double, 3> moved =
        entityCollision::collide(blockAt, box, entity.velocityX, entity.velocityY, entity.velocityZ);
    entity.x += moved[0];
    entity.y += moved[1];
    entity.z += moved[2];
    entity.onGround = moved[1] != entity.velocityY && entity.velocityY < 0.0;

    // parity: `Entity.restituteMovementAfterCollisions`: a stopped axis is zeroed unless the block
    // underneath bounces, and only slime and beds do.
    const float bounciness =
        entity.onGround ? registry::blockMovement::bouncinessOf(blockUnder(world, entity)) * kNonLivingBounceShare : 0.0F;
    if (moved[0] != entity.velocityX) {
        entity.velocityX = 0.0;
    }
    if (moved[2] != entity.velocityZ) {
        entity.velocityZ = 0.0;
    }
    if (moved[1] != entity.velocityY) {
        entity.velocityY = bounciness > 0.0F ? -entity.velocityY * static_cast<double>(bounciness) : 0.0;
    }

    entity.velocityX *= drag;
    entity.velocityY *= drag;
    entity.velocityZ *= drag;
    if (entity.onGround) {
        entity.velocityX *= horizontalGroundDrag;
        entity.velocityY *= verticalGroundDrag;
        entity.velocityZ *= horizontalGroundDrag;
    }
}

} // namespace basalt::server::runtime::entity::entityPhysics
