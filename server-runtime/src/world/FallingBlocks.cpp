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

/// parity: `FluidTags.WATER` at a square, which is the water itself and anything waterlogged.
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

/// parity: the `clip` of `FallingBlockEntity.tick`, which only runs for concrete powder moving faster
/// than one block a tick: it looks back along the way it came for the water it went through, so a
/// long fall into a pool sets at the surface instead of under it. The fall is straight down here, so
/// walking the column is that ray exactly.
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
        // `ClipContext.Fluid.SOURCE_ONLY` stops the ray at a **source**, so water that is only
        // flowing is passed straight through; `Block.COLLIDER` stops it at anything in the way.
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

/// parity: `BrushableBlock.tick` makes its falling entity and then calls `disableDrop()`, which sets
/// `cancelDrop`, and `cancelDrop` skips the whole place-or-drop branch of `FallingBlockEntity.tick`:
/// a suspicious sand that falls is simply gone, it neither lands nor leaves an item.
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
    // parity: `ConcretePowderBlock.updateShape` devuelve el hormigon **sin llamar a super**, y es
    // super quien reserva el turno de caida. O sea que un polvo al que le llega agua no reserva nada:
    // se endurece y ya. Reservarlo dejaba un turno de mas que el jar no tiene, medido en `hormigon`.
    if (reshapes::concreteFor(*block).has_value() && reshapes::touchesWater(world, x, y, z)) {
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
    // parity: `Entity.move`, which is real collision and not a question about the block below. A
    // falling block goes **straight through** anything with no collision box, a torch, a line of
    // dust or a nest of frogspawn included, and stops on the first thing that is really in the way.
    // Reading the square underneath instead landed the sand on top of the eggs. Measured in
    // `huevarana`.
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
    // parity: `FallingBlockEntity.tick`. Concrete powder standing in water lands **there and then**,
    // in mid air if need be: the landing branch is `if (!onGround && !isStuckInWater)` inverted, not
    // `onGround` alone. Without it the powder sank to the floor of the pool and set seven ticks late.
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
    // parity: the three questions `FallingBlockEntity.tick` asks once it is on the ground, and the
    // square it asks them about is `blockPosition()`, where its own feet are.
    const std::optional<std::int32_t> at = world.blockState(blockX, blockY, blockZ);
    const std::optional<std::int32_t> below = world.blockState(blockX, blockY - 1, blockZ);
    const bool mayReplace = at.has_value() && registry::blockShapes::isReplaceable(*at);
    // parity: `wouldContinueFalling`, which is `FallingBlock.isFree` on the square underneath: a
    // block that came to rest on something it could still fall through drops as an item instead.
    const bool wouldContinueFalling = below.has_value() && isFree(*below);
    const bool wouldSurvive =
        (!canSurvive || canSurvive(entity.objectData, blockX, blockY, blockZ)) && !wouldContinueFalling;
    if (mayReplace && wouldSurvive && world.setBlockState(blockX, blockY, blockZ, entity.objectData)) {
        std::int32_t landed = entity.objectData;
        if (const std::optional<std::int32_t> set =
                reshapes::landedConcrete(world, blockX, blockY, blockZ, entity.objectData, *at);
            set.has_value() && world.setBlockState(blockX, blockY, blockZ, *set)) {
            landed = *set;
        }
        return Landing{.placed = BlockChange{.x = blockX, .y = blockY, .z = blockZ, .stateId = landed}};
    }
    return Landing{.shattered = entity.objectData};
}

} // namespace basalt::server::runtime::world::fallingBlocks
