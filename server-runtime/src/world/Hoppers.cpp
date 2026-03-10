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
constexpr std::int32_t kCooldown = 8;

/// parity: `Hopper.SUCK_AABB`, `Block.column(16, 11, 32)`: from 11/16 up to two blocks.
constexpr double kSuckBottom = 11.0 / 16.0;
constexpr double kSuckTop = 2.0;

constexpr std::string_view kCooldownKey = "TransferCooldown";

/// parity: `MinecartHopper.readAdditionalSaveData`, whose default is on.
constexpr std::string_view kEnabledKey = "Enabled";

/// parity: `MinecartHopper.getLevelY`, half a block over the cart's own middle.
constexpr double kCartLevelRise = 0.5;

/// parity: `MinecartHopper.suckInItems`, whose second box is its own widened a quarter sideways.
constexpr double kCartReach = 0.25;

/// parity: `Container.getMaxStackSize()`, whose default nothing that holds items overrides.
constexpr std::int32_t kContainerMaxStack = 99;

/// parity: `Container.getMaxStackSize(stack)`: how high this item stacks here, and in practice the
/// item's own maximum. A bucket goes sixteen to a slot and a sword one, not sixty-four.
[[nodiscard]] std::int32_t capOf(const wire::common::Identifier& item) {
    return std::min(kContainerMaxStack, registry::itemStacking::maxStackSize(item));
}

/// The same, for a container that stacks lower than the item does.
[[nodiscard]] std::int32_t capIn(std::string_view path, const wire::common::Identifier& item) {
    return std::min(kContainerMaxStack, containers::maxStackFor(path, item));
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

/// A container this build keeps contents for, with the tag that holds them. It is a block most of
/// the time and a minecart the rest, and the two are the same thing from here: a name, a number of
/// slots and somewhere to write.
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

/// parity: `HopperBlockEntity.getEntityContainer`, a cube of one block centred on the point the
/// hopper asks about, against the cart's box. Only a block hopper's point is a square's own middle:
/// a cart asks half a block over itself, which is why the point is what travels and not the square.
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

/// parity: `getContainerAt`: the block first, and only where there is none, whatever cart is there.
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

/// parity: `WorldlyContainer.getSlotsForFace`: a furnace takes fuel from the side, ingredients
/// from above and gives its result downwards. Everything else opens every slot to every side.
[[nodiscard]] std::vector<std::int32_t> slotsFor(std::string_view path, std::int32_t size, registry::BlockFace from) {
    return containers::slotsForFace(path, size, from);
}

/// parity: Kotlin's `getOrNull`: a face may name a slot the container does not have.
[[nodiscard]] std::optional<ItemStack>* slotAt(Slots& slots, std::int32_t slot) {
    return slot >= 0 && slot < static_cast<std::int32_t>(slots.size()) ? &slots[static_cast<std::size_t>(slot)]
                                                                       : nullptr;
}

/// One item out of the stack, with everything it carries. parity: `ItemStack.shrink`, which takes a
/// number off and leaves the components where they are.
[[nodiscard]] std::optional<ItemStack> minusOne(const ItemStack& stack) {
    if (stack.count <= 1) {
        return std::nullopt;
    }
    ItemStack left = stack;
    left.count = stack.count - 1;
    return left;
}

/// parity: `ItemStack.isSameItemSameComponents`, which is what decides whether two stacks are the
/// same thing. Two swords of the same name with different enchantments never share a slot.
[[nodiscard]] bool same(const ItemStack& one, const ItemStack& other) {
    return one.item == other.item && one.components == other.components;
}

[[nodiscard]] bool accepts(const Slots& slots, const ItemStack& stack) {
    return std::ranges::any_of(slots, [&stack](const std::optional<ItemStack>& there) {
        return !there.has_value() || (same(*there, stack) && there->count < capOf(stack.item));
    });
}

void add(Slots& slots, const ItemStack& stack) {
    for (std::optional<ItemStack>& there : slots) {
        if (there.has_value() && (!same(*there, stack) || there->count >= capOf(stack.item))) {
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

/// Puts one of `stack` into the container, answering whether it went in.
[[nodiscard]] bool insert(WorldState& world, const Container& target, const ItemStack& stack, registry::BlockFace from,
                          const Around& around) {
    Slots slots = blockInventory::readFor(target.path, *target.data, target.size);
    const std::vector<std::int32_t> allowed = slotsFor(target.path, target.size, from);
    for (const std::int32_t slot : allowed) {
        std::optional<ItemStack>* there = slotAt(slots, slot);
        if (there == nullptr) {
            continue;
        }
        if (there->has_value() && (!same(**there, stack) || (*there)->count >= capIn(target.path, stack.item))) {
            continue;
        }
        // parity: `canPlaceItemThroughFace`. A furnace's result slot takes nothing, its fuel slot
        // takes only fuel, a brewing stand's three slots each take their own thing, and a shulker
        // box takes anything but another shulker box.
        if (!containers::acceptsThroughFace(target.path, slot, stack.item, !there->has_value())) {
            continue;
        }
        // parity: `CrafterBlockEntity.canPlaceItem` is more than a face rule: it refuses a slot that
        // is switched off and spreads what it is fed across the grid rather than piling it up.
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
        // parity: `tryMoveInItem` only calls `setItem` when the slot it lands in is empty; a merge
        // goes through `grow` and leaves the counters alone. And `setItem` only cares about the
        // slot a furnace cooks from.
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

/// parity: `Container.isEmpty` and `HopperBlockEntity.inventoryFull`, the two questions
/// `tryMoveItems` asks before deciding whether to give and whether to take.
[[nodiscard]] bool isEmpty(const Slots& slots) {
    return std::ranges::none_of(slots, [](const std::optional<ItemStack>& slot) { return slot.has_value(); });
}

[[nodiscard]] bool isFull(const Slots& slots) {
    return std::ranges::all_of(slots, [](const std::optional<ItemStack>& slot) {
        return slot.has_value() && slot->count >= capOf(slot->item);
    });
}

/// parity: `ejectItems`: the first slot with something in it gives one item to what it faces.
/// parity: `ComposterBlock.InputContainer`, which is a container with one slot that only takes
/// something compostable, only through the **top** face and only while there is room. Putting the
/// item in composts it there and then: `setChanged` is what calls `addItem`.
[[nodiscard]] bool compost(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, registry::BlockFace face,
                           Slots& slots, random::RandomSource& random, const std::int64_t gameTime) {
    // parity: `ComposterBlock.getContainer` hands out an `InputContainer` **only** while the level is
    // under seven: at seven it is an `EmptyContainer` and at eight an `OutputContainer`. A hopper on
    // top of a composter that is full or ready puts nothing in, and this one was eating the item.
    const std::optional<std::int32_t> level = composters::levelAt(world, x, y, z);
    if (face != registry::BlockFace::Up || !level.has_value() || *level >= composters::kFull) {
        return false;
    }
    for (std::optional<ItemStack>& slot : slots) {
        if (!slot.has_value() || !composters::chanceOf(slot->item).has_value()) {
            continue;
        }
        // The roll can fail and the item is eaten anyway: `InputContainer.setChanged` shrinks the
        // stack whatever `addItem` answered.
        // parity: `ComposterBlock.addItem` raises `BLOCK_CHANGE`, and only when the level really
        // rose: a roll that fails is silent.
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

/// parity: `ComposterBlock.OutputContainer`, the bone meal of a composter that is ready, given out
/// through the **bottom** face only, and taking it empties the composter.
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

/// Notes a block whose state a move changed, for whoever has to tell the clients.
void noteChange(const Around& around, const BlockChange& change) {
    if (around.changes != nullptr) {
        around.changes->push_back(change);
    }
}

/// parity: `JukeboxBlockEntity` is a container of one, so a hopper loads a jukebox and another one
/// under it takes the disc back out. What it holds lives under `RecordItem` rather than in an item
/// list, so it goes through the jukebox's own door rather than through the generic one.
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

/// The other half: the disc out of the jukebox above, which stops the song.
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
    const std::optional<std::pair<BlockChange, wire::common::Identifier>> ejected = jukeboxes::eject(world, x, y, z);
    if (!ejected.has_value()) {
        return false;
    }
    noteChange(around, ejected->first);
    add(slots, disc);
    return true;
}

/// parity: `ChiseledBookShelfBlockEntity` is a container of six that holds one book each and whose
/// `acceptsItemType` asks the `bookshelf_books` tag, so a hopper full of cobblestone gets nowhere.
/// Putting one in goes through `setItem`, which is what lights the slot on the block and moves the
/// number a comparator reads.
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

/// The other half: a hopper under a shelf takes the books back out, one a turn.
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
        // parity: `canTakeItemThroughFace`, which is what stops a hopper under a furnace draining
        // its coal or one under a brewing stand stealing the ingredient.
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

/// parity: `getItemsAtAndAbove`: whatever lies in the box goes in, whole stack at a time. The box
/// travels with the hopper rather than with a square, which is what lets a cart use the same one.
/// Whether a dropped stack's own box crosses the one given.
///
/// parity: `HopperBlockEntity.getItemsAtAndAbove` asks the level with
/// `getEntitiesOfClass(ItemEntity.class, aabb, ...)`, so what decides is the **box** of the item and
/// not where its middle is. A stack measures a quarter of a block, so it sticks out an eighth each
/// way and one lying just outside the column still goes in. Crossed with `AABB.intersects`, strict at
/// both ends, the same way `arrowButtons::reaches` and `pressurePlates::touches` do it.
[[nodiscard]] bool itemCrosses(const DroppedItem& item, double minX, double minY, double minZ, double maxX,
                               double maxY, double maxZ) {
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
            world.spawnItem(*std::move(taken));
        }
    }
    return took;
}

/// parity: `Hopper.SUCK_AABB` over a block hopper's own square, from 11/16 up to two blocks.
[[nodiscard]] bool suckIn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, Slots& slots) {
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
    // parity: `pushItemsTick` counts down first and only then asks whether the cooldown is over, so
    // a hopper that just moved waits eight ticks and not nine.
    // parity: only a move calls `setChanged`. Counting down is not a change worth saving, and
    // marking the chunk for it would rewrite every hopper's region file every single tick.
    // parity: `getIntOr("TransferCooldown", -1)`, and a hopper with no cooldown written down is one
    // that has never moved anything.
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
    // parity: `tryMoveItems` gives one away when it has something and takes one when it has room,
    // and both happen on the same turn: the two are `moved |=`, not one or the other.
    bool moved = false;
    if (!isEmpty(slots)) {
        moved = push(world, x, y, z, *out, slots, random, around);
    }
    if (!isFull(slots)) {
        // parity: `suckInItems` looks at what is lying above only when there is no container there,
        // and a composter that is ready **is** one: its bone meal comes out downwards.
        const std::optional<Container> above = containerAt(world, x, y + 1, z);
        const bool taken =
            takeCompost(world, x, y + 1, z, slots, around.gameTime) || takeDisc(world, x, y + 1, z, slots, around) ||
            unshelveBook(world, x, y + 1, z, slots, around) ||
            (above.has_value() ? pullFrom(world, *above, slots, around.gameTime) : suckIn(world, x, y, z, slots));
        moved = taken || moved;
    }
    if (!moved) {
        return false;
    }
    blockInventory::write(entity->data(), slots);
    entity->data().putInt(kCooldownKey, kCooldown);
    redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
    return true;
}

bool tickCart(WorldState& world, entity::ServerEntity& cart, const std::int64_t gameTime) {
    if (cart.removed || !cart.sourceNbt.has_value() || cart.type().path() != "hopper_minecart") {
        return false;
    }
    // parity: `MinecartHopper.isEnabled`, which an activator rail turns off. Missing means on.
    if (cart.sourceNbt->getByte(kEnabledKey, 1) == 0) {
        return false;
    }
    Slots slots = blockInventory::read(*cart.sourceNbt, kSize);
    if (isFull(slots)) {
        return false;
    }
    // parity: `getLevelY` is half a block over the cart, and `suckInItems` asks one block over that.
    const double levelY = cart.y + kCartLevelRise;
    const std::optional<Container> above = containerAtPoint(world, cart.x, levelY + 1.0, cart.z);
    // parity: a cart's `isGridAligned` is false, so a full block over it never blocks the suck.
    bool took = above.has_value() ? pullFrom(world, *above, slots, gameTime)
                                  : suckInBox(world, cart.x - 0.5, levelY - 0.5 + kSuckBottom, cart.z - 0.5,
                                              cart.x + 0.5, levelY - 0.5 + kSuckTop, cart.z + 0.5, slots);
    if (!took) {
        // parity: `MinecartHopper.suckInItems`' own half, which a block hopper has no equivalent of.
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
