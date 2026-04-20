#include <basalt/server/registry/BlockConnections.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/FallingBlocks.hpp>
#include <basalt/server/runtime/world/Reshapes.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world::fallingBlocks {
namespace {

/// parity: `Entity.getGravity` for a falling block, and `Entity.getAirDrag`.
constexpr double kGravity = 0.04;
constexpr double kAirDrag = 0.98;

/// parity: `FluidTags.WATER` at a square.
[[nodiscard]] bool waterAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> at = world.blockState(x, y, z);
    if (!at.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*at);
    if (block != nullptr && block->path() == "water") {
        return true;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*at);
    const auto logged = properties.find("waterlogged");
    return logged != properties.end() && logged->second == "true";
}

[[nodiscard]] bool isConcretePowder(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path().ends_with("_concrete_powder");
}

/// parity: the `clip` of `FallingBlockEntity.tick`.
[[nodiscard]] bool waterSource(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> at = world.blockState(x, y, z);
    if (!at.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*at);
    if (block == nullptr || block->path() != "water") {
        return false;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*at);
    const auto level = properties.find("level");
    return level == properties.end() || level->second == "0";
}

[[nodiscard]] std::optional<std::int32_t> waterCrossed(WorldState& world, std::int32_t x, std::int32_t z,
                                                       std::int32_t fromY, std::int32_t toY) {
    for (std::int32_t y = fromY; y >= toY; --y) {
        const std::optional<std::int32_t> at = world.blockState(x, y, z);
        const bool stops = waterSource(world, x, y, z) || (at.has_value() && !isFree(*at));
        if (!stops) {
            continue;
        }
        // And then the jar asks whether **that** square holds water, which a waterlogged block does.
        return waterAt(world, x, y, z) ? std::optional{y} : std::nullopt;
    }
    return std::nullopt;
}

} // namespace

const wire::common::Identifier& entityType() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("falling_block");
    return instance;
}

bool falls(std::int32_t stateId) {
    // parity: `SpeleothemBlock implements Fallable`.
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block != nullptr && (block->path() == "pointed_dripstone" || block->path() == "sulfur_spike")) {
        return false;
    }
    return registry::blockConnections::isFalling(stateId);
}

/// parity: `BrushableBlock.tick` makes its falling entity and then calls `disableDrop()`.
[[nodiscard]] bool cancelsDrop(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return false;
    }
    const std::string_view path = block->path();
    return path == "suspicious_sand" || path == "suspicious_gravel";
}

bool isFree(std::int32_t stateId) {
    if (stateId == registry::blocks::air().stateId || registry::blockShapes::isReplaceable(stateId)) {
        return true;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && block->path() == "fire";
}

void scheduleIfFalling(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const Asked asked) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !falls(*stateId)) {
        return;
    }
    // parity: a scheduled tick is stored under the **block** that booked it, so a world of ours opens.
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return;
    }
    // parity: only `ConcretePowderBlock.updateShape` short-circuits.
    if (asked == Asked::updateShape && reshapes::concreteFor(*block).has_value() &&
        reshapes::touchesWater(world, x, y, z)) {
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

Landing tick(WorldState& world, entity::ServerEntity& entity, const CanSurvive& canSurvive,
             const OnArrival& onArrival) {
    entity.velocityY -= kGravity;
    entity.velocityY *= kAirDrag;
    // parity: `Entity.move`.
    const entity::entityCollision::BlockAt blockAt = [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
        return world.blockState(x, y, z);
    };
    const entity::Aabb box = entity::Aabb::of(entity.x, entity.y, entity.z, entity.width(), entity.height());
    const std::array<double, 3> moved = entity::entityCollision::collide(blockAt, box, 0.0, entity.velocityY, 0.0);
    const bool onGround = moved[1] != entity.velocityY && entity.velocityY < 0.0;
    entity.y += moved[1];

    std::int32_t blockX = wire::common::mth::floor(entity.x);
    std::int32_t blockY = wire::common::mth::floor(entity.y);
    const std::int32_t blockZ = wire::common::mth::floor(entity.z);
    // parity: `FallingBlockEntity.tick`.
    const bool concrete = isConcretePowder(entity.objectData);
    bool stuckInWater = concrete && waterAt(world, blockX, blockY, blockZ);
    if (concrete && (entity.velocityY * entity.velocityY) > 1.0 && !stuckInWater) {
        const std::int32_t fromY = wire::common::mth::floor(entity.y - moved[1]);
        if (const std::optional<std::int32_t> crossed = waterCrossed(world, blockX, blockZ, fromY, blockY);
            crossed.has_value()) {
            blockY = *crossed;
            stuckInWater = true;
        }
    }
    if (!onGround && !stuckInWater) {
        const server::world::Chunk* chunk = world.chunkIfLoaded(blockX >> 4, blockZ >> 4);
        if (chunk != nullptr && blockY < chunk->verticalBounds().minY()) {
            entity.removed = true;
        }
        return Landing{};
    }
    entity.removed = true;
    if (cancelsDrop(entity.objectData)) {
        return Landing{};
    }
    // parity: the three questions `FallingBlockEntity.tick` asks once it is on the ground.
    const std::optional<std::int32_t> at = world.blockState(blockX, blockY, blockZ);
    const std::optional<std::int32_t> below = world.blockState(blockX, blockY - 1, blockZ);
    const bool mayReplace = at.has_value() && registry::blockShapes::isReplaceable(*at);
    // parity: `wouldContinueFalling`.
    const bool wouldContinueFalling = below.has_value() && isFree(*below);
    const bool wouldSurvive =
        (!canSurvive || canSurvive(entity.objectData, blockX, blockY, blockZ)) && !wouldContinueFalling;
    if (mayReplace && wouldSurvive && world.setBlockState(blockX, blockY, blockZ, entity.objectData)) {
        std::int32_t landed = entity.objectData;
        if (const std::optional<std::int32_t> set =
                reshapes::landedConcrete(world, blockX, blockY, blockZ, entity.objectData, *at);
            set.has_value()) {
            // parity: `onLand` runs **after** the entity's own `setBlock`, so the powder is really.
            if (onArrival) {
                onArrival(entity.objectData, blockX, blockY, blockZ);
            }
            if (world.setBlockState(blockX, blockY, blockZ, *set)) {
                landed = *set;
            }
        }
        return Landing{.placed = BlockChange{.x = blockX, .y = blockY, .z = blockZ, .stateId = landed}};
    }
    return Landing{.shattered = entity.objectData};
}

} // namespace basalt::server::runtime::world::fallingBlocks
