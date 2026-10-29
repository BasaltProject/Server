#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ChiseledBookshelves.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Composters.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/Hoppers.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/Shelves.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world::hoppers {
namespace {

using Slots = blockInventory::Slots;
using ItemStack = server::world::loot::ItemStack;

/// parity: `HopperBlockEntity.MOVE_ITEM_SPEED`, the eight ticks between moves.

/// parity: `Hopper.SUCK_AABB`, `Block.column(16, 11, 32)`: from 11/16 up to two blocks.
constexpr double kSuckBottom = 11.0 / 16.0;
constexpr double kSuckTop = 2.0;

constexpr std::string_view kCooldownKey = "TransferCooldown";

/// parity: `MinecartHopper.readAdditionalSaveData`, whose default is on.
constexpr std::string_view kEnabledKey = "Enabled";

/// parity: `MinecartHopper.getLevelY`, half a block over the cart's own middle.
constexpr double kCartLevelRise = 0.5;

/// parity: `MinecartHopper.suckInItems`.
constexpr double kCartReach = 0.25;

/// parity: `Container.getMaxStackSize(stack)`.
[[nodiscard]] std::int32_t capOf(const server::world::loot::ItemStack& stack) {
    return std::min(containers::kSlotLimit, registry::itemStacking::maxStackSize(stack.item, stack.components));
}

[[nodiscard]] std::int32_t capIn(std::string_view path, const server::world::loot::ItemStack& stack) {
    return std::min(containers::kSlotLimit, containers::maxStackFor(path, stack));
}

/// parity: the sides `AbstractFurnaceBlockEntity.getSlotsForFace` opens to a hopper.
[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::optional<registry::BlockFace> faceOf(std::string_view facing) {
    if (facing == "down") {
        return registry::BlockFace::Down;
    }
    if (facing == "up") {
        return registry::BlockFace::Up;
    }
    if (facing == "north") {
        return registry::BlockFace::North;
    }
    if (facing == "south") {
        return registry::BlockFace::South;
    }
    if (facing == "west") {
        return registry::BlockFace::West;
    }
    if (facing == "east") {
        return registry::BlockFace::East;
    }
    return std::nullopt;
}

[[nodiscard]] server::world::ChunkBlockEntity* entityAt(WorldState& world, std::int32_t x, std::int32_t y,
                                                        std::int32_t z) {
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    return chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
}

struct Container {
    std::string_view path;
    std::int32_t size = 0;
    wire::nbt::NbtCompound* data = nullptr;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    /// parity: only a `BlockEntity` runs `setChanged`, so a minecart tells no comparator.
    bool isBlock = true;
};

/// parity: `HopperBlockEntity.getEntityContainer`.
[[nodiscard]] std::optional<Container> cartAround(WorldState& world, double px, double py, double pz, std::int32_t x,
                                                  std::int32_t y, std::int32_t z) {
    for (entity::ServerEntity* cart : world.entities()) {
        if (cart == nullptr || cart->removed || !cart->sourceNbt.has_value()) {
            continue;
        }
        const std::string_view path = cart->type().path();
        const std::optional<std::int32_t> size = containers::sizeOf(path);
        if (!size.has_value() || !detectorRails::isMinecart(cart->type())) {
            continue;
        }
        if (cart->x + detectorRails::kCartHalfWidth <= px - 0.5 ||
            cart->x - detectorRails::kCartHalfWidth >= px + 0.5 ||
            cart->z + detectorRails::kCartHalfWidth <= pz - 0.5 ||
            cart->z - detectorRails::kCartHalfWidth >= pz + 0.5 || cart->y + detectorRails::kCartHeight <= py - 0.5 ||
            cart->y >= py + 0.5) {
            continue;
        }
        return Container{
            .path = path, .size = *size, .data = &*cart->sourceNbt, .x = x, .y = y, .z = z, .isBlock = false};
    }
    return std::nullopt;
}

/// parity: `getContainerAt`.
[[nodiscard]] std::optional<Container> containerAtPoint(WorldState& world, double px, double py, double pz) {
    const std::int32_t x = wire::common::mth::floor(px);
    const std::int32_t y = wire::common::mth::floor(py);
    const std::int32_t z = wire::common::mth::floor(pz);
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string_view path = block->path();
    const std::optional<std::int32_t> size = path == "hopper" ? std::optional{kSize} : containers::sizeOf(path);
    // parity: every slot read of a `RandomizableContainer` unpacks its table first.
    if (size.has_value()) {
        world.unpackLoot(x, y, z);
    }
    server::world::ChunkBlockEntity* entity = size.has_value() ? entityAt(world, x, y, z) : nullptr;
    if (entity == nullptr) {
        return cartAround(world, px, py, pz, x, y, z);
    }
    return Container{.path = path, .size = *size, .data = &entity->data(), .x = x, .y = y, .z = z};
}

[[nodiscard]] std::optional<Container> containerAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return containerAtPoint(world, static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5,
                            static_cast<double>(z) + 0.5);
}

/// parity: `WorldlyContainer.getSlotsForFace`.
[[nodiscard]] std::vector<std::int32_t> slotsFor(std::string_view path, std::int32_t size, registry::BlockFace from) {
    return containers::slotsForFace(path, size, from);
}

/// parity: Kotlin's `getOrNull`: a face may name a slot the container does not have.
[[nodiscard]] std::optional<ItemStack>* slotAt(Slots& slots, std::int32_t slot) {
    return slot >= 0 && slot < static_cast<std::int32_t>(slots.size()) ? &slots[static_cast<std::size_t>(slot)]
                                                                       : nullptr;
}

/// One item out of the stack, with everything it carries. parity: `ItemStack.shrink`, which takes a
[[nodiscard]] std::optional<ItemStack> minusOne(const ItemStack& stack) {
    if (stack.count <= 1) {
        return std::nullopt;
    }
    ItemStack left = stack;
    left.count = stack.count - 1;
    return left;
}

/// parity: `ItemStack.isSameItemSameComponents`.
[[nodiscard]] bool same(const ItemStack& one, const ItemStack& other) {
    return one.item == other.item && one.components == other.components;
}

[[nodiscard]] bool accepts(const Slots& slots, const ItemStack& stack) {
    return std::ranges::any_of(slots, [&stack](const std::optional<ItemStack>& there) {
        return !there.has_value() || (same(*there, stack) && there->count < capOf(stack));
    });
}

void add(Slots& slots, const ItemStack& stack) {
    for (std::optional<ItemStack>& there : slots) {
        if (there.has_value() && (!same(*there, stack) || there->count >= capOf(stack))) {
            continue;
        }
        if (there.has_value()) {
            there->count += 1;
        } else {
            there = ItemStack{.item = stack.item, .count = 1, .components = stack.components};
        }
        return;
    }
}

[[nodiscard]] bool insert(WorldState& world, const Container& target, const ItemStack& stack, registry::BlockFace from,
                          const Around& around) {
    Slots slots = blockInventory::readFor(target.path, *target.data, target.size);
    const std::vector<std::int32_t> allowed = slotsFor(target.path, target.size, from);
    for (const std::int32_t slot : allowed) {
        std::optional<ItemStack>* there = slotAt(slots, slot);
        if (there == nullptr) {
            continue;
        }
        if (there->has_value() && (!same(**there, stack) || (*there)->count >= capIn(target.path, stack))) {
            continue;
        }
        // parity: `canPlaceItemThroughFace`. A furnace's result slot takes nothing, its fuel slot.
        if (!containers::acceptsThroughFace(target.path, slot, stack.item, !there->has_value())) {
            continue;
        }
        // parity: `CrafterBlockEntity.canPlaceItem` is more than a face rule.
        if (target.path == crafters::block().path() && !crafters::accepts(*target.data, slots, slot, stack)) {
            continue;
        }
        const bool wasEmpty = !there->has_value();
        if (there->has_value()) {
            (*there)->count += 1;
        } else {
            *there = ItemStack{.item = stack.item, .count = 1, .components = stack.components};
        }
        blockInventory::writeFor(target.path, *target.data, slots);
        // parity: `tryMoveInItem` only calls `setItem` when the slot it lands in is empty.
        if (wasEmpty && slot == furnaces::kInputSlot) {
            furnaces::inputChanged(target.path, *target.data, around.recipeFor);
        }
        world.chunkIfLoaded(target.x >> 4, target.z >> 4)->markDirty();
        if (target.isBlock) {
            // parity: `tryMoveInItem` ends in `setChanged`, and a shelf's raises `BLOCK_ACTIVATE`.
            shelves::slotsChanged(world, target.x, target.y, target.z, around.gameTime);
            redstoneComponents::scheduleComparatorsAround(world, {}, target.x, target.y, target.z,
                                                          comparatorReading::of(world));
        }
        return true;
    }
    return false;
}

/// parity: `Container.isEmpty` and `HopperBlockEntity.inventoryFull`, the two questions.
[[nodiscard]] bool isEmpty(const Slots& slots) {
    return std::ranges::none_of(slots, [](const std::optional<ItemStack>& slot) { return slot.has_value(); });
}

[[nodiscard]] bool isFull(const Slots& slots) {
    return std::ranges::all_of(
        slots, [](const std::optional<ItemStack>& slot) { return slot.has_value() && slot->count >= capOf(*slot); });
}

/// parity: `ejectItems`.
[[nodiscard]] bool compost(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, registry::BlockFace face,
                           Slots& slots, random::RandomSource& random, const std::int64_t gameTime) {
    // parity: `ComposterBlock.getContainer` hands out an `InputContainer` **only** while the level is.
    const std::optional<std::int32_t> level = composters::levelAt(world, x, y, z);
    if (face != registry::BlockFace::Up || !level.has_value() || *level >= composters::kFull) {
        return false;
    }
    for (std::optional<ItemStack>& slot : slots) {
        if (!slot.has_value() || !composters::chanceOf(slot->item).has_value()) {
            continue;
        }
        // parity: `ComposterBlock.addItem` raises `BLOCK_CHANGE`, and only when the level really.
        if (composters::addItem(world, x, y, z, slot->item, random).has_value() &&
            world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
            vibrations::emit(world, "block_change", static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5,
                             static_cast<double>(z) + 0.5, gameTime);
        }
        slot = minusOne(*slot);
        redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
        return true;
    }
    return false;
}

/// parity: `ComposterBlock.OutputContainer`.
[[nodiscard]] bool takeCompost(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, Slots& slots,
                               const std::int64_t gameTime) {
    if (composters::levelAt(world, x, y, z) != composters::kReady) {
        return false;
    }
    const wire::common::Identifier boneMeal = wire::common::Identifier::vanilla("bone_meal");
    const ItemStack given{.item = boneMeal, .count = 1};
    if (!accepts(slots, given)) {
        return false;
    }
    if (!composters::empty(world, x, y, z).has_value()) {
        return false;
    }
    // parity: `ComposterBlock.empty` raises `BLOCK_CHANGE` too, so emptying one is heard.
    if (world.hasSculkSensorNear(x, y, z, vibrations::kCalibratedRange)) {
        vibrations::emit(world, "block_change", static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5,
                         static_cast<double>(z) + 0.5, gameTime);
    }
    add(slots, given);
    redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
    return true;
}

void noteChange(const Around& around, const BlockChange& change) {
    if (around.changes != nullptr) {
        around.changes->push_back(change);
    }
}

/// parity: `JukeboxBlockEntity` is a container of one.
[[nodiscard]] bool loadJukebox(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, Slots& slots,
                               const Around& around) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr || *block != jukeboxes::block()) {
        return false;
    }
    for (std::optional<ItemStack>& slot : slots) {
        if (!slot.has_value() || !jukeboxes::isDisc(slot->item)) {
            continue;
        }
        const std::optional<BlockChange> loaded = jukeboxes::insert(world, x, y, z, slot->item);
        if (!loaded.has_value()) {
            return false;
        }
        noteChange(around, *loaded);
        slot = minusOne(*slot);
        return true;
    }
    return false;
}

[[nodiscard]] bool takeDisc(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, Slots& slots,
                            const Around& around) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr || *block != jukeboxes::block()) {
        return false;
    }
    const std::optional<std::string> song = jukeboxes::songAt(world, x, y, z);
    if (!song.has_value()) {
        return false;
    }
    const ItemStack disc{.item = wire::common::Identifier::vanilla("music_disc_" + *song), .count = 1};
    if (!accepts(slots, disc)) {
        return false;
    }
    const std::optional<std::pair<BlockChange, wire::common::Identifier>> ejected =
        jukeboxes::eject(world, x, y, z, around.gameTime);
    if (!ejected.has_value()) {
        return false;
    }
    noteChange(around, ejected->first);
    add(slots, disc);
    return true;
}

/// parity: `ChiseledBookShelfBlockEntity` is a container of six that holds one book each and whose.
[[nodiscard]] bool shelveBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, Slots& slots,
                              const Around& around) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr || !chiseledBookshelves::isBookshelf(block->path())) {
        return false;
    }
    for (std::optional<ItemStack>& slot : slots) {
        if (!slot.has_value() || (around.isBookshelfBook && !around.isBookshelfBook(slot->item))) {
            continue;
        }
        std::optional<chiseledBookshelves::Interaction> shelved;
        for (std::int32_t where = 0; where < chiseledBookshelves::kSize && !shelved.has_value(); ++where) {
            shelved = chiseledBookshelves::putBook(world, x, y, z, where, slot->item);
        }
        if (!shelved.has_value()) {
            return false;
        }
        noteChange(around, shelved->change);
        slot = minusOne(*slot);
        return true;
    }
    return false;
}

[[nodiscard]] bool unshelveBook(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, Slots& slots,
                                const Around& around) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr || !chiseledBookshelves::isBookshelf(block->path())) {
        return false;
    }
    for (std::int32_t where = 0; where < chiseledBookshelves::kSize; ++where) {
        const server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
        const server::world::ChunkBlockEntity* entity =
            chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
        if (entity == nullptr) {
            return false;
        }
        const Slots held = blockInventory::read(entity->data(), chiseledBookshelves::kSize);
        if (!held[static_cast<std::size_t>(where)].has_value()) {
            continue;
        }
        if (!accepts(slots, *held[static_cast<std::size_t>(where)])) {
            return false;
        }
        const std::optional<chiseledBookshelves::Interaction> taken =
            chiseledBookshelves::takeBook(world, x, y, z, where);
        if (!taken.has_value() || !taken->taken.has_value()) {
            return false;
        }
        add(slots, ItemStack{.item = *taken->taken, .count = 1});
        noteChange(around, taken->change);
        return true;
    }
    return false;
}

[[nodiscard]] bool push(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, registry::BlockFace out,
                        Slots& slots, random::RandomSource& random, const Around& around) {
    const std::int32_t tx = x + registry::stepX(out);
    const std::int32_t ty = y + registry::stepY(out);
    const std::int32_t tz = z + registry::stepZ(out);
    if (compost(world, tx, ty, tz, registry::opposite(out), slots, random, around.gameTime) ||
        loadJukebox(world, tx, ty, tz, slots, around) || shelveBook(world, tx, ty, tz, slots, around)) {
        return true;
    }
    const std::optional<Container> target = containerAt(world, tx, ty, tz);
    if (!target.has_value()) {
        return false;
    }
    for (std::optional<ItemStack>& slot : slots) {
        if (!slot.has_value()) {
            continue;
        }
        if (!insert(world, *target, *slot, registry::opposite(out), around)) {
            continue;
        }
        slot = minusOne(*slot);
        return true;
    }
    return false;
}

/// parity: `suckInItems` for the container half: one item out of whatever sits on top.
[[nodiscard]] bool pullFrom(WorldState& world, const Container& above, Slots& slots, const std::int64_t gameTime) {
    Slots source = blockInventory::readFor(above.path, *above.data, above.size);
    const std::vector<std::int32_t> takeable = slotsFor(above.path, above.size, registry::BlockFace::Down);
    for (const std::int32_t slot : takeable) {
        std::optional<ItemStack>* stack = slotAt(source, slot);
        if (stack == nullptr || !stack->has_value()) {
            continue;
        }
        if (!accepts(slots, **stack)) {
            continue;
        }
        // parity: `canTakeItemThroughFace`, which is what stops a hopper under a furnace draining.
        if (!containers::givesThroughFace(above.path, slot, (*stack)->item, registry::BlockFace::Down)) {
            continue;
        }
        add(slots, **stack);
        *stack = minusOne(**stack);
        blockInventory::writeFor(above.path, *above.data, source);
        if (above.isBlock) {
            shelves::slotsChanged(world, above.x, above.y, above.z, gameTime);
            redstoneComponents::scheduleComparatorsAround(world, {}, above.x, above.y, above.z,
                                                          comparatorReading::of(world));
        }
        return true;
    }
    return false;
}

/// parity: `HopperBlockEntity.getItemsAtAndAbove` tests the item's box.
[[nodiscard]] bool itemCrosses(const DroppedItem& item, double minX, double minY, double minZ, double maxX, double maxY,
                               double maxZ) {
    static const double half = registry::entityTypeShapes::widthOf(wire::common::Identifier::vanilla("item")) / 2.0;
    static const double height = registry::entityTypeShapes::heightOf(wire::common::Identifier::vanilla("item"));
    const auto overlaps = [](double lowA, double highA, double lowB, double highB) {
        return lowA < highB && highA > lowB;
    };
    return overlaps(item.x - half, item.x + half, minX, maxX) && overlaps(item.y, item.y + height, minY, maxY) &&
           overlaps(item.z - half, item.z + half, minZ, maxZ);
}

[[nodiscard]] bool suckInBox(WorldState& world, double minX, double minY, double minZ, double maxX, double maxY,
                             double maxZ, Slots& slots) {
    bool took = false;
    for (const DroppedItem& item : world.items()) {
        if (item.count <= 0) {
            continue;
        }
        if (!itemCrosses(item, minX, minY, minZ, maxX, maxY, maxZ)) {
            continue;
        }
        // parity: what a dropped stack carries goes into the hopper with it.
        const ItemStack picked{.item = item.item, .count = 1, .components = item.components};
        std::int32_t left = item.count;
        while (left > 0 && accepts(slots, picked)) {
            add(slots, picked);
            --left;
        }
        if (left == item.count) {
            continue;
        }
        took = true;
        // No way to write one item's count in place, so what is left is put back as it was.
        std::optional<DroppedItem> taken = world.removeItem(item.entityId);
        if (left > 0 && taken.has_value()) {
            taken->count = left;
            taken->countChanged = true;
            world.restoreItem(*std::move(taken));
        }
    }
    return took;
}

/// parity: `Hopper.SUCK_AABB` over a block hopper's own square.
[[nodiscard]] bool suckIn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, Slots& slots,
                          const Around& around) {
    if (const std::optional<std::int32_t> above = world.blockState(x, y + 1, z);
        above.has_value() && registry::blockShapes::isFullCube(*above)) {
        const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*above);
        if (block == nullptr || !around.doesNotBlockHoppers || !around.doesNotBlockHoppers(*block)) {
            return false;
        }
    }
    return suckInBox(world, x, static_cast<double>(y) + kSuckBottom, z, static_cast<double>(x) + 1.0,
                     static_cast<double>(y) + kSuckTop, static_cast<double>(z) + 1.0, slots);
}

} // namespace

bool tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
          const Around& around) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || block->path() != "hopper") {
        return false;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z);
    if (entity == nullptr) {
        return false;
    }
    // parity: `pushItemsTick` counts down before checking.
    const std::int32_t was = entity->data().getInt(kCooldownKey, -1);
    if (was - 1 > 0) {
        entity->data().putInt(kCooldownKey, was - 1);
        return false;
    }
    if (was != 0) {
        entity->data().putInt(kCooldownKey, 0);
    }
    // parity: `HopperBlock.ENABLED`, which a redstone signal turns off.
    if (propertyOf(*stateId, "enabled") == "false") {
        return false;
    }
    const std::optional<registry::BlockFace> out = faceOf(propertyOf(*stateId, "facing"));
    if (!out.has_value()) {
        return false;
    }
    Slots slots = blockInventory::read(entity->data(), kSize);
    // parity: `tryMoveItems` gives one away when it has something and takes one when it has room.
    bool moved = false;
    for (std::int32_t repeat = 0; repeat < around.amount && !isEmpty(slots); repeat++) {
        const bool pushed = push(world, x, y, z, *out, slots, random, around);
        moved = pushed || moved;
        if (!pushed) {
            break;
        }
    }
    if (!isFull(slots)) {
        // parity: `suckInItems` looks at what is lying above only when there is no container there.
        const std::optional<Container> above = containerAt(world, x, y + 1, z);
        const bool taken = takeCompost(world, x, y + 1, z, slots, around.gameTime) ||
                           takeDisc(world, x, y + 1, z, slots, around) ||
                           unshelveBook(world, x, y + 1, z, slots, around) ||
                           (above.has_value() ? pullFrom(world, *above, slots, around.gameTime)
                                              : suckIn(world, x, y, z, slots, around));
        moved = taken || moved;
        // Not vanilla when `amount` is above one: more of the same turn.
        for (std::int32_t repeat = 1; taken && repeat < around.amount && !isFull(slots); repeat++) {
            const bool more = above.has_value() ? pullFrom(world, *above, slots, around.gameTime)
                                                : suckIn(world, x, y, z, slots, around);
            if (!more) {
                break;
            }
        }
    }
    if (!moved) {
        return false;
    }
    blockInventory::write(entity->data(), slots);
    entity->data().putInt(kCooldownKey, around.transferTicks);
    redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
    return true;
}

bool tickCart(WorldState& world, entity::ServerEntity& cart, const std::int64_t gameTime) {
    if (cart.removed || !cart.sourceNbt.has_value() || cart.type().path() != "hopper_minecart") {
        return false;
    }
    // parity: `MinecartHopper.isEnabled`.
    if (cart.sourceNbt->getByte(kEnabledKey, 1) == 0) {
        return false;
    }
    Slots slots = blockInventory::read(*cart.sourceNbt, kSize);
    if (isFull(slots)) {
        return false;
    }
    // parity: `getLevelY` is half a block over the cart.
    const double levelY = cart.y + kCartLevelRise;
    const std::optional<Container> above = containerAtPoint(world, cart.x, levelY + 1.0, cart.z);
    // parity: a cart's `isGridAligned` is false.
    bool took = above.has_value() ? pullFrom(world, *above, slots, gameTime)
                                  : suckInBox(world, cart.x - 0.5, levelY - 0.5 + kSuckBottom, cart.z - 0.5,
                                              cart.x + 0.5, levelY - 0.5 + kSuckTop, cart.z + 0.5, slots);
    if (!took) {
        // parity: `MinecartHopper.suckInItems`' own half.
        constexpr double kSide = detectorRails::kCartHalfWidth + kCartReach;
        took = suckInBox(world, cart.x - kSide, cart.y, cart.z - kSide, cart.x + kSide,
                         cart.y + detectorRails::kCartHeight, cart.z + kSide, slots);
    }
    if (!took) {
        return false;
    }
    blockInventory::write(*cart.sourceNbt, slots);
    if (server::world::Chunk* chunk =
            world.chunkIfLoaded(wire::common::mth::floor(cart.x) >> 4, wire::common::mth::floor(cart.z) >> 4);
        chunk != nullptr) {
        chunk->markDirty();
    }
    return true;
}

} // namespace basalt::server::runtime::world::hoppers
