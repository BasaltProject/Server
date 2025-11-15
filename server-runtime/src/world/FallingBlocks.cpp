#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/wire/common/Mth.hpp>

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

bool falls(std::int32_t stateId) { return registry::blockConnections::isFalling(stateId); }

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
    world.scheduleBlockTick(x, y, z, entityType(), kFallDelayTicks);
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
    const registry::EntityTypeEntry* type = registry::entityTypes::of(entityType());
    if (type == nullptr) {
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

std::optional<BlockChange> tick(WorldState& world, entity::ServerEntity& entity) {
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
        if (world.setBlockState(blockX, resting, blockZ, entity.objectData)) {
            return BlockChange{.x = blockX, .y = resting, .z = blockZ, .stateId = entity.objectData};
        }
        return std::nullopt;
    }
    const server::world::Chunk* chunk = world.chunkIfLoaded(blockX >> 4, blockZ >> 4);
    if (chunk != nullptr && landingY < chunk->verticalBounds().minY()) {
        entity.removed = true;
        return std::nullopt;
    }
    entity.y = nextY;
    return std::nullopt;
}

} // namespace basalt::server::runtime::world::fallingBlocks
