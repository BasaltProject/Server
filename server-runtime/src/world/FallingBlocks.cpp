#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cstdint>
#include <memory>

namespace basalt::server::runtime::world::fallingBlocks {
namespace {

/// parity: `Entity.getGravity` for a falling block, and `Entity.getAirDrag`.
constexpr double kGravity = 0.04;
constexpr double kAirDrag = 0.98;

} // namespace

const wire::common::Identifier& entityType() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("falling_block");
    return instance;
}

bool falls(std::int32_t stateId) {
    // parity: `SpeleothemBlock implements Fallable` so that a stalactite the block itself drops can
    // land, but it is **not** a `FallingBlock`: it books no fall check of its own and a dripstone
    // with air under it stays where it is. The table cannot tell the two apart, so it is named here.
    // Measured in `estalactita`, where ours fell like sand and the jar's hung there.
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block != nullptr && (block->path() == "pointed_dripstone" || block->path() == "sulfur_spike")) {
        return false;
    }
    return registry::blockConnections::isFalling(stateId);
}

bool isFree(std::int32_t stateId) {
    if (stateId == registry::blocks::air().stateId || registry::blockShapes::isReplaceable(stateId)) {
        return true;
    }
    // Fire is replaceable too, so the check above already covers it; vanilla names it separately
    // and so does this.
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "fire";
}

void scheduleIfFalling(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !falls(*stateId)) {
        return;
    }
    // parity: a scheduled tick is stored under the **block** that booked it, so a world of ours opens
    // in vanilla with the sand still about to fall. `falling_block` is an entity type and no block at
    // all, and vanilla drops a tick whose name it cannot find in the block registry.
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return;
    }
    world.scheduleBlockTick(x, y, z, *block, kFallDelayTicks);
}

entity::ServerEntity* fallFrom(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                               const std::function<std::int32_t()>& allocateEntityId) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    const registry::EntityTypeEntry* type = registry::entityTypes::of(entityType());
    if (!stateId.has_value() || type == nullptr) {
        return nullptr;
    }
    if (!world.setBlockState(x, y, z, registry::blocks::air().stateId)) {
        return nullptr;
    }

    // parity: `FallingBlockEntity.fall` spawns it at the middle of the block it left.
    auto entity = std::make_unique<entity::ServerEntity>(allocateEntityId(), wire::common::Uuid::random(), entityType(),
                                                         type->protocolId, x + 0.5, static_cast<double>(y), z + 0.5);
    entity->objectData = *stateId;
    return &world.spawnEntity(std::move(entity));
}

entity::ServerEntity* beginFalling(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                   const std::function<std::int32_t()>& allocateEntityId) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !falls(*stateId)) {
        return nullptr;
    }
    // On the floor of the world there is nothing below to read, so the check below would refuse it
    // anyway; this says why rather than leaving it to a missing block.
    const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr || y <= chunk->verticalBounds().minY()) {
        return nullptr;
    }
    const std::optional<std::int32_t> below = world.blockState(x, y - 1, z);
    if (!below.has_value() || !isFree(*below)) {
        return nullptr;
    }
    return fallFrom(world, x, y, z, allocateEntityId);
}

Landing tick(WorldState& world, entity::ServerEntity& entity, const CanSurvive& canSurvive) {
    entity.velocityY -= kGravity;
    entity.velocityY *= kAirDrag;
    const double nextY = entity.y + entity.velocityY;
    const std::int32_t blockX = wire::common::mth::floor(entity.x);
    const std::int32_t blockZ = wire::common::mth::floor(entity.z);
    const std::int32_t landingY = wire::common::mth::floor(nextY);

    const std::optional<std::int32_t> below = world.blockState(blockX, landingY, blockZ);
    if (below.has_value() && !isFree(*below)) {
        entity.removed = true;
        const std::int32_t resting = landingY + 1;
        // parity: `mayReplace && wouldSurvive`, the two questions `FallingBlockEntity.tick` asks
        // before it writes. `wouldContinueFalling` is already false here: the square under this one
        // is what stopped the fall.
        const std::optional<std::int32_t> at = world.blockState(blockX, resting, blockZ);
        const bool mayReplace = at.has_value() && registry::blockShapes::isReplaceable(*at);
        const bool wouldSurvive = !canSurvive || canSurvive(entity.objectData, blockX, resting, blockZ);
        if (mayReplace && wouldSurvive && world.setBlockState(blockX, resting, blockZ, entity.objectData)) {
            return Landing{.placed = BlockChange{.x = blockX, .y = resting, .z = blockZ, .stateId = entity.objectData}};
        }
        return Landing{.shattered = entity.objectData};
    }
    const server::world::Chunk* chunk = world.chunkIfLoaded(blockX >> 4, blockZ >> 4);
    if (chunk != nullptr && landingY < chunk->verticalBounds().minY()) {
        entity.removed = true;
        return Landing{};
    }
    entity.y = nextY;
    return Landing{};
}

} // namespace basalt::server::runtime::world::fallingBlocks
