#include <basalt/server/runtime/entity/EntityPhysics.hpp>
#include <basalt/server/runtime/entity/PrimedTnt.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <memory>

namespace basalt::server::runtime::entity::primedTnt {
namespace {

/// parity: `PrimedTnt.getDefaultGravity`.
constexpr double kGravity = 0.04;

/// parity: `getDeltaMovement().multiply(0.7, -0.5, 0.7)` on landing.
constexpr double kGroundDrag = 0.7;
constexpr double kLandingBounce = -0.5;

} // namespace

const wire::common::Identifier& type() {
    static const wire::common::Identifier tnt = wire::common::Identifier::vanilla("tnt");
    return tnt;
}

ServerEntity& prime(world::WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                    const std::int32_t entityId, const std::int32_t protocolType, const std::int32_t fuse) {
    // parity: `TntBlock.explode` spawns it at the middle of the block it was.
    auto entity = std::make_unique<ServerEntity>(entityId, wire::common::Uuid::random(), type(), protocolType, x + 0.5,
                                                 static_cast<double>(y), z + 0.5);
    entity->fuse = fuse;
    return world.spawnEntity(std::move(entity));
}

bool tick(world::WorldState& world, ServerEntity& entity) {
    // parity: `PrimedTnt`'s own constructor, which starts at eighty: a block of TNT summoned rather
    // than lit still has a fuse, and without this it went off on its first tick.
    if (entity.fuse == ServerEntity::kNoFuse) {
        entity.fuse = kDefaultFuse;
    }
    entityPhysics::move(world, entity, kGravity, entityPhysics::kAirDrag, kGroundDrag, kLandingBounce);
    ++entity.age;
    --entity.fuse;
    return entity.fuse <= 0;
}

} // namespace basalt::server::runtime::entity::primedTnt
