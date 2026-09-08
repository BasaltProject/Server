#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Buckets.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Dispensers.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>

#include <cstdint>
#include <map>
#include <string>

namespace basalt::server::runtime::world::dispensers {
namespace {

using registry::BlockFace;
using ItemStack = server::world::loot::ItemStack;

/// parity: `DispenserBlock`'s four ticks of delay.
constexpr std::int32_t kDelay = 4;

/// parity: `DefaultDispenseItemBehavior.spawnItem`, the shove it gives what it throws.
constexpr double kSpeedLow = 0.2;
constexpr double kSpeedSpread = 0.1;
constexpr double kThrowLift = 0.2;

/// parity: `DefaultDispenseItemBehavior.DEFAULT_ACCURACY` and the spread one point of it is worth.
constexpr double kSpreadPerAccuracy = 0.0172275;
constexpr double kDefaultAccuracy = 6.0;

/// parity: `DispenserBlock.getDispensePosition`'s own 0.7 out of the face, and the drop below the
/// middle that is smaller out of the top and the bottom.
constexpr double kDispenseReach = 0.7;
constexpr double kVerticalDrop = 0.125;
constexpr double kSidewaysDrop = 0.15625;

/// parity: the `(0, 0.1, 0)` offset a projectile leaves with, so it clears the block below.
constexpr double kProjectileLift = 0.1;

constexpr std::int32_t kMaxStack = 64;

[[nodiscard]] const wire::common::Identifier& flintAndSteel() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("flint_and_steel");
    return instance;
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] bool isDispenser(std::string_view path) { return path == "dispenser" || path == "dropper"; }

[[nodiscard]] std::optional<BlockFace> faceOf(std::string_view facing) {
    if (facing == "down") {
        return BlockFace::Down;
    }
    if (facing == "up") {
        return BlockFace::Up;
    }
    if (facing == "north") {
        return BlockFace::North;
    }
    if (facing == "south") {
        return BlockFace::South;
    }
    if (facing == "west") {
        return BlockFace::West;
    }
    return facing == "east" ? std::optional{BlockFace::East} : std::nullopt;
}

/// The two enums are both in vanilla's `Direction` order, which is what the Kotlin matched by name.
[[nodiscard]] wire::protocol::play::Direction directionOf(BlockFace face) {
    return static_cast<wire::protocol::play::Direction>(static_cast<std::uint8_t>(face));
}

[[nodiscard]] std::optional<std::int32_t> containerSizeOf(const Hooks& hooks, std::string_view path) {
    return hooks.containerSize ? hooks.containerSize(path) : containers::sizeOf(path);
}

[[nodiscard]] std::optional<std::int32_t> protocolTypeOf(const Hooks& hooks, const wire::common::Identifier& type) {
    if (hooks.protocolTypeOf) {
        return hooks.protocolTypeOf(type);
    }
    const registry::EntityTypeEntry* entry = registry::entityTypes::of(type);
    return entry == nullptr ? std::nullopt : std::optional{entry->protocolId};
}

[[nodiscard]] std::int32_t nextEntityId(const Hooks& hooks) {
    return hooks.allocateEntityId ? hooks.allocateEntityId() : 0;
}

/// The stack a slot is left with once one item leaves it.
[[nodiscard]] std::optional<ItemStack> minusOne(const ItemStack& stack) {
    if (stack.count <= 1) {
        return std::nullopt;
    }
    ItemStack left = stack;
    left.count = stack.count - 1;
    return left;
}

/// parity: `DispenserBlockEntity.getRandomSlot`, which picks evenly among the slots with something
/// in by keeping the last of `seen` that a one-in-`seen` roll chose.
[[nodiscard]] std::optional<std::size_t> randomFullSlot(const blockInventory::Slots& slots,
                                                        random::RandomSource& random) {
    std::optional<std::size_t> chosen;
    std::int32_t seen = 0;
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        if (!slots[slot].has_value()) {
            continue;
        }
        ++seen;
        if (random.nextInt(seen) == 0) {
            chosen = slot;
        }
    }
    return chosen;
}

/// What a block-changing behaviour left in the slot. `handled` false means the item has no
/// behaviour of this kind and falls through to being thrown.
struct Special {
    bool handled = false;
    std::optional<ItemStack> left;
};

/// The block-changing behaviours: a full bucket pours, an empty one fills, and flint and steel
/// lights a fire.
///
/// parity: `DispenseItemBehavior.bootStrap`'s filled-bucket, empty-bucket and flint-and-steel
/// entries. The rest of that table makes entities.
[[nodiscard]] Special dispenseOnBlock(const fluidFlow::Context& context, std::int32_t x, std::int32_t y, std::int32_t z,
                                      BlockFace out, const ItemStack& stack) {
    const std::int32_t tx = x + registry::stepX(out);
    const std::int32_t ty = y + registry::stepY(out);
    const std::int32_t tz = z + registry::stepZ(out);
    if (buckets::isBucket(stack.item)) {
        const std::optional<buckets::Result> result =
            buckets::useOn(context, stack.item, tx, ty, tz, directionOf(out), false);
        if (!result.has_value()) {
            return {};
        }
        // parity: `consumeWithRemainder`, which puts what the bucket became back in the slot.
        const std::optional<ItemStack> left = minusOne(stack);
        if (left.has_value()) {
            return Special{.handled = true, .left = left};
        }
        if (!result->replacement.has_value()) {
            return {};
        }
        return Special{.handled = true, .left = ItemStack{.item = *result->replacement, .count = 1}};
    }
    if (stack.item == flintAndSteel()) {
        if (!fires::ignite(context.world, tx, ty, tz, context.random)) {
            return {};
        }
        return Special{.handled = true, .left = stack};
    }
    return {};
}

/// The container in front: where it is and how many slots it has.
struct Target {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t size = 0;
};

/// The container in front, or nothing when the block in front is not one.
[[nodiscard]] std::optional<Target> containerInFront(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                     BlockFace out, const Hooks& hooks) {
    const Target target{.x = x + registry::stepX(out), .y = y + registry::stepY(out), .z = z + registry::stepZ(out)};
    const std::optional<std::int32_t> stateId = world.blockState(target.x, target.y, target.z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> size = containerSizeOf(hooks, block->path());
    if (!size.has_value()) {
        return std::nullopt;
    }
    return Target{.x = target.x, .y = target.y, .z = target.z, .size = *size};
}

/// Puts one item of the stack into the container, answering whether a slot took it.
[[nodiscard]] bool insertOne(WorldState& world, const Target& into, const ItemStack& stack) {
    server::world::Chunk* chunk = world.chunkIfLoaded(into.x >> 4, into.z >> 4);
    server::world::ChunkBlockEntity* blockEntity =
        chunk == nullptr ? nullptr : chunk->blockEntityAt(into.x & 15, into.y, into.z & 15);
    if (blockEntity == nullptr) {
        return false;
    }
    blockInventory::Slots slots = blockInventory::read(blockEntity->data(), into.size);
    for (std::optional<ItemStack>& there : slots) {
        if (there.has_value() && (there->item != stack.item || there->count >= kMaxStack)) {
            continue;
        }
        if (there.has_value()) {
            there->count += 1;
        } else {
            there = ItemStack{.item = stack.item, .count = 1, .components = stack.components};
        }
        blockInventory::write(blockEntity->data(), slots);
        chunk->markDirty();
        redstoneComponents::scheduleComparatorsAround(world, into.x, into.y, into.z);
        return true;
    }
    return false;
}

/// parity: `DefaultDispenseItemBehavior.spawnItem`: out of the face, a little below the middle.
void throwOut(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, BlockFace out, const ItemStack& stack,
              random::RandomSource& random, const Hooks& hooks) {
    const double centreX = x + 0.5 + (kDispenseReach * registry::stepX(out));
    const double centreZ = z + 0.5 + (kDispenseReach * registry::stepZ(out));
    double centreY = y + 0.5 + (kDispenseReach * registry::stepY(out));
    centreY -= out == BlockFace::Up || out == BlockFace::Down ? kVerticalDrop : kSidewaysDrop;
    const double push = (random.nextDouble() * kSpeedSpread) + kSpeedLow;
    const double spread = kSpreadPerAccuracy * kDefaultAccuracy;
    // The three rolls are in this order because vanilla's are, and a braced list is sequenced.
    world.spawnItem(DroppedItem{.entityId = nextEntityId(hooks),
                                .uuid = wire::common::Uuid::random(),
                                .item = stack.item,
                                .count = 1,
                                .x = centreX,
                                .y = centreY,
                                .z = centreZ,
                                .velocityX = random.triangle(registry::stepX(out) * push, spread),
                                .velocityY = random.triangle(kThrowLift, spread),
                                .velocityZ = random.triangle(registry::stepZ(out) * push, spread),
                                .components = stack.components});
}

/// parity: `ProjectileDispenseBehavior`, which shoots along the face at 1.1 with six degrees of
/// spread, from `getDispensePosition(source, 0.7, (0, 0.1, 0))`.
void shoot(const fluidFlow::Context& context, std::int32_t x, std::int32_t y, std::int32_t z, BlockFace out,
           const entity::projectiles::Kind& kind, const Hooks& hooks) {
    const std::optional<std::int32_t> protocolType = protocolTypeOf(hooks, kind.type);
    if (!protocolType.has_value()) {
        return;
    }
    entity::projectiles::spawn(context.world, kind, nextEntityId(hooks), *protocolType,
                               x + 0.5 + (kDispenseReach * registry::stepX(out)),
                               y + 0.5 + (kDispenseReach * registry::stepY(out)) + kProjectileLift,
                               z + 0.5 + (kDispenseReach * registry::stepZ(out)), registry::stepX(out),
                               registry::stepY(out), registry::stepZ(out), entity::projectiles::kDispenserPower,
                               entity::projectiles::kDispenserUncertainty, context.random);
}

} // namespace

const wire::common::Identifier& tickType() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("dispenser");
    return instance;
}

std::optional<BlockChange> onNeighbourChange(WorldState& world, std::string_view worldName, std::int32_t x,
                                             std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || !isDispenser(block->path())) {
        return std::nullopt;
    }
    const bool triggered = propertyOf(*stateId, "triggered") == "true";
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0 ||
                        redstone::bestNeighbourSignal(world, worldName, x, y + 1, z) > 0;
    if (signal == triggered) {
        return std::nullopt;
    }
    if (signal) {
        world.scheduleBlockTick(x, y, z, tickType(), kDelay);
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, {{"triggered", signal ? "true" : "false"}}, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

Result fire(const fluidFlow::Context& context, std::int32_t x, std::int32_t y, std::int32_t z, const Hooks& hooks) {
    Result result;
    WorldState& world = context.world;
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return result;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || !isDispenser(block->path())) {
        return result;
    }
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    server::world::ChunkBlockEntity* blockEntity = chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
    if (blockEntity == nullptr) {
        return result;
    }
    blockInventory::Slots slots = blockInventory::read(blockEntity->data(), kSize);
    const std::optional<std::size_t> slot = randomFullSlot(slots, context.random);
    if (!slot.has_value()) {
        // parity: `DispenserBlock.dispenseFrom`, which clicks at nothing when it is empty.
        result.effects.emplace_back(blockEffect::Effect{.type = blockEffect::Effect::kDispenserFail, .data = 0});
        return result;
    }
    const std::optional<BlockFace> out = faceOf(propertyOf(*stateId, "facing"));
    if (!out.has_value()) {
        return result;
    }
    const ItemStack stack = *slots[*slot];

    // parity: `DropperBlock.getDispenseMethod`, which answers the default behaviour whatever the
    // item is: a dropper never shoots and never pours, it only throws or hands over.
    const bool dropper = block->path() == "dropper";

    // parity: `DispenserBlock.registerProjectileBehavior`, which shoots rather than throws.
    const entity::projectiles::Kind* projectile = dropper ? nullptr : entity::projectiles::dispensed(stack.item);
    if (projectile != nullptr) {
        shoot(context, x, y, z, *out, *projectile, hooks);
        slots[*slot] = minusOne(stack);
        blockInventory::write(blockEntity->data(), slots);
        chunk->markDirty();
        redstoneComponents::scheduleComparatorsAround(world, x, y, z);
        result.fired = true;
        return result;
    }

    // parity: the `DispenseItemBehavior`s that change blocks. The ones that make an entity fall
    // through to being thrown.
    const Special special = dropper ? Special{} : dispenseOnBlock(context, x, y, z, *out, stack);
    if (special.handled) {
        slots[*slot] = special.left;
        blockInventory::write(blockEntity->data(), slots);
        chunk->markDirty();
        redstoneComponents::scheduleComparatorsAround(world, x, y, z);
        result.fired = true;
        return result;
    }

    const std::optional<Target> into = containerInFront(world, x, y, z, *out, hooks);
    if (into.has_value()) {
        if (!insertOne(world, *into, stack)) {
            return result;
        }
    } else {
        throwOut(world, x, y, z, *out, stack, context.random, hooks);
    }
    slots[*slot] = minusOne(stack);
    blockInventory::write(blockEntity->data(), slots);
    chunk->markDirty();
    redstoneComponents::scheduleComparatorsAround(world, x, y, z);
    result.fired = true;
    return result;
}

} // namespace basalt::server::runtime::world::dispensers
