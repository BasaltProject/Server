#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>

#include <cstdint>
#include <map>
#include <string>

namespace basalt::server::runtime::world::crafters {
namespace {

constexpr std::int32_t kTriggerDelay = 4;
constexpr std::int32_t kCraftingTicks = 6;
constexpr std::int32_t kGrid = 3;

constexpr std::string_view kCraftingTicksKey = "crafting_ticks_remaining";
constexpr std::string_view kDisabledSlotsKey = "disabled_slots";
constexpr std::string_view kTriggeredKey = "triggered";

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] server::world::ChunkBlockEntity* entityAt(WorldState& world, std::int32_t x, std::int32_t y,
                                                        std::int32_t z) {
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    return chunk == nullptr ? nullptr : chunk->blockEntityAt(x & 15, y, z & 15);
}

/// parity: the `orientation` property, whose first word is the way the front faces.
[[nodiscard]] std::array<std::int32_t, 3> facingStep(std::int32_t stateId) {
    const std::string orientation = propertyOf(stateId, "orientation");
    const std::string facing = orientation.substr(0, orientation.find('_'));
    if (facing == "down") {
        return {0, -1, 0};
    }
    if (facing == "up") {
        return {0, 1, 0};
    }
    if (facing == "south") {
        return {0, 0, 1};
    }
    if (facing == "west") {
        return {-1, 0, 0};
    }
    if (facing == "east") {
        return {1, 0, 0};
    }
    return {0, 0, -1};
}

[[nodiscard]] std::int32_t maxStackOf(const server::world::loot::ItemStack& stack) {
    return registry::itemStacking::maxStackSize(stack.item, stack.components);
}

[[nodiscard]] registry::BlockFace oppositeOf(const std::array<std::int32_t, 3>& step) {
    if (step[1] != 0) {
        return step[1] < 0 ? registry::BlockFace::Up : registry::BlockFace::Down;
    }
    if (step[0] != 0) {
        return step[0] < 0 ? registry::BlockFace::East : registry::BlockFace::West;
    }
    return step[2] < 0 ? registry::BlockFace::South : registry::BlockFace::North;
}

/// parity: `HopperBlockEntity.addItem` over `tryMoveInItem`: every slot open to that face in turn,
/// an empty one taking the whole stack and a matching one topping up to the item's own limit.
void addItem(std::string_view path, const wire::nbt::NbtCompound& data, blockInventory::Slots& slots,
             server::world::loot::ItemStack& stack, registry::BlockFace face) {
    const auto size = static_cast<std::int32_t>(slots.size());
    for (const std::int32_t slot : containers::slotsForFace(path, size, face)) {
        if (stack.count <= 0) {
            return;
        }
        std::optional<server::world::loot::ItemStack>& there = slots[static_cast<std::size_t>(slot)];
        const bool fits = path == "crafter"
                              ? accepts(data, slots, slot, stack)
                              : containers::acceptsThroughFace(path, slot, stack.item, !there.has_value());
        if (!fits) {
            continue;
        }
        if (!there.has_value()) {
            there = stack;
            stack.count = 0;
        } else if (there->count <= maxStackOf(*there) && there->item == stack.item &&
                   there->components == stack.components) {
            const std::int32_t moved = std::min(stack.count, maxStackOf(stack) - there->count);
            if (moved > 0) {
                there->count += moved;
                stack.count -= moved;
            }
        }
    }
}

/// Puts the result into the container in front and answers what is left of it.
[[nodiscard]] server::world::loot::ItemStack insert(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                    const std::array<std::int32_t, 3>& step,
                                                    const server::world::loot::ItemStack& stack) {
    server::world::loot::ItemStack remaining = stack;
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    // How big the container in front is. This used to go through a callback that the one production
    // caller never filled in, so `insert` always answered no and a crafter threw every result on the
    // floor instead of putting it in the chest it was pointing at.
    const std::optional<std::int32_t> size = block == nullptr ? std::nullopt : containers::sizeOf(block->path());
    server::world::ChunkBlockEntity* entity = size.has_value() ? entityAt(world, x, y, z) : nullptr;
    if (entity == nullptr) {
        return remaining;
    }
    const std::string_view path = block->path();
    const registry::BlockFace face = oppositeOf(step);
    blockInventory::Slots slots = blockInventory::readFor(path, entity->data(), *size);
    const std::int32_t cap = std::min(containers::kSlotLimit, containers::maxStackFor(path, stack));
    // parity: `CrafterBlock.dispenseItem`: into another crafter, or more than one slot takes, one at a time.
    if (path == "crafter" || stack.count > cap) {
        while (remaining.count > 0) {
            server::world::loot::ItemStack one = remaining;
            one.count = 1;
            addItem(path, entity->data(), slots, one, face);
            if (one.count > 0) {
                break;
            }
            remaining.count -= 1;
        }
    } else {
        while (remaining.count > 0) {
            const std::int32_t before = remaining.count;
            addItem(path, entity->data(), slots, remaining, face);
            if (remaining.count == before) {
                break;
            }
        }
    }
    if (remaining.count != stack.count) {
        blockInventory::writeFor(path, entity->data(), slots);
        world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
        redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
    }
    return remaining;
}

/// parity: `dispenseItem`, which prefers the container in front and throws it out when there is none.
void deliver(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
             const server::world::loot::ItemStack& result, const Hooks& hooks) {
    const std::array<std::int32_t, 3> step = facingStep(stateId);
    const std::int32_t tx = x + step[0];
    const std::int32_t ty = y + step[1];
    const std::int32_t tz = z + step[2];
    const server::world::loot::ItemStack left = insert(world, tx, ty, tz, step, result);
    if (left.count <= 0) {
        return;
    }
    DroppedItem dropped{.entityId = hooks.allocateEntityId ? hooks.allocateEntityId() : 0,
                        .uuid = wire::common::Uuid::random(),
                        .item = left.item,
                        .count = left.count,
                        .x = tx + 0.5,
                        .y = ty + 0.5,
                        .z = tz + 0.5};
    dropped.components = left.components;
    world.spawnItem(std::move(dropped));
}

} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("crafter");
    return instance;
}

std::optional<BlockChange> onNeighbourChange(WorldState& world, std::string_view worldName, std::int32_t x,
                                             std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(*stateId);
    if (id == nullptr || *id != block()) {
        return std::nullopt;
    }
    const bool triggered = propertyOf(*stateId, "triggered") == "true";
    const bool signal = redstone::bestNeighbourSignal(world, worldName, x, y, z) > 0;
    if (signal == triggered) {
        return std::nullopt;
    }
    // parity: `CrafterBlockEntity` keeps its own `triggered` beside the state's, and the jar writes
    // it down. Measured in the bench circuit `crafter`.
    if (server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z); entity != nullptr) {
        entity->data().putInt(kTriggeredKey, signal ? 1 : 0);
    }
    std::map<std::string, std::string> wanted;
    if (signal) {
        world.scheduleBlockTick(x, y, z, block(), kTriggerDelay);
        wanted = {{"triggered", "true"}};
    } else {
        wanted = {{"triggered", "false"}, {"crafting", "false"}};
    }
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(*id, wanted, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *turned};
}

std::optional<BlockChange> craft(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                 const Hooks& hooks) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value() || !hooks.resultFor) {
        return std::nullopt;
    }
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(*stateId);
    if (id == nullptr || *id != block()) {
        return std::nullopt;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z);
    if (entity == nullptr) {
        return std::nullopt;
    }
    blockInventory::Slots slots = blockInventory::read(entity->data(), kSize);
    const std::optional<server::world::loot::ItemStack> result =
        hooks.resultFor(server::world::crafting::CraftingInput::of(kGrid, kGrid, slots));
    if (!result.has_value() || result->count <= 0) {
        // parity: `CrafterBlock.dispenseFrom` raises `levelEvent(1050, pos, 0)` when what is in it is
        // not a recipe, which is the click a crafter makes at nothing. The block does not move, so the
        // change carries the same state it already had and only the effect is new.
        return BlockChange{.x = x,
                           .y = y,
                           .z = z,
                           .stateId = *stateId,
                           .effects = {blockEffect::Effect{.type = blockEffect::Effect::kCrafterFail, .data = 0}}};
    }
    deliver(world, x, y, z, *stateId, *result, hooks);
    if (hooks.crafted) {
        hooks.crafted(server::world::crafting::CraftingInput::of(kGrid, kGrid, slots), slots);
    }
    for (std::optional<server::world::loot::ItemStack>& slot : slots) {
        if (!slot.has_value()) {
            continue;
        }
        slot = slot->count <= 1
                   ? std::nullopt
                   : std::optional{server::world::loot::ItemStack{.item = slot->item, .count = slot->count - 1}};
    }
    blockInventory::write(entity->data(), slots);
    entity->data().putInt(kCraftingTicksKey, kCraftingTicks);
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
    const std::optional<std::int32_t> crafting =
        registry::blockStateDefinitions::stateIdOf(*id, {{"crafting", "true"}}, *stateId);
    if (!crafting.has_value() || !world.setBlockState(x, y, z, *crafting)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *crafting};
}

std::optional<BlockChange> tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                wire::nbt::NbtCompound& data) {
    const std::int32_t left = data.getInt(kCraftingTicksKey, 0);
    if (left <= 0) {
        return std::nullopt;
    }
    data.putInt(kCraftingTicksKey, left - 1);
    if (left - 1 > 0) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(*stateId);
    if (id == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> still =
        registry::blockStateDefinitions::stateIdOf(*id, {{"crafting", "false"}}, *stateId);
    if (!still.has_value() || !world.setBlockState(x, y, z, *still)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *still};
}

std::int32_t comparatorSignal(const wire::nbt::NbtCompound& data) {
    const blockInventory::Slots slots = blockInventory::read(data, kSize);
    const std::set<std::int32_t> disabled = disabledSlots(data);
    std::int32_t taken = 0;
    for (std::int32_t slot = 0; slot < kSize; ++slot) {
        if (slots[static_cast<std::size_t>(slot)].has_value() || disabled.contains(slot)) {
            ++taken;
        }
    }
    return taken;
}

std::set<std::int32_t> disabledSlots(const wire::nbt::NbtCompound& data) {
    const std::span<const std::int32_t> stored = data.getIntArray(kDisabledSlotsKey);
    return std::set<std::int32_t>{stored.begin(), stored.end()};
}

bool setSlotState(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                  const std::int32_t slot, const bool enabled) {
    if (slot < 0 || slot >= kSize) {
        return false;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z);
    if (entity == nullptr) {
        return false;
    }
    // parity: `slotCanBeDisabled`, which is the slot being empty. A slot with something in it keeps
    // its switch wherever it was.
    const blockInventory::Slots slots = blockInventory::read(entity->data(), kSize);
    if (slots[static_cast<std::size_t>(slot)].has_value()) {
        return false;
    }
    std::set<std::int32_t> off = disabledSlots(entity->data());
    const bool changed = enabled ? off.erase(slot) > 0 : off.insert(slot).second;
    if (!changed) {
        return false;
    }
    const std::vector<std::int32_t> written{off.begin(), off.end()};
    entity->data().putIntArray(std::string{kDisabledSlotsKey}, written);
    if (server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4); chunk != nullptr) {
        chunk->markDirty();
    }
    return true;
}

bool accepts(const wire::nbt::NbtCompound& data, const blockInventory::Slots& slots, const std::int32_t slot,
             const server::world::loot::ItemStack& stack) {
    const std::set<std::int32_t> off = disabledSlots(data);
    if (off.contains(slot)) {
        return false;
    }
    const auto index = static_cast<std::size_t>(slot);
    if (index >= slots.size()) {
        return false;
    }
    const std::optional<server::world::loot::ItemStack>& here = slots[index];
    if (!here.has_value()) {
        return true;
    }
    if (here->count >= maxStackOf(*here)) {
        return false;
    }
    // parity: `smallerStackExist` only looks **forward**, so the grid fills left to right and then
    // starts again rather than topping up the first slot it finds.
    for (std::int32_t later = slot + 1; later < kSize; ++later) {
        if (off.contains(later)) {
            continue;
        }
        const std::optional<server::world::loot::ItemStack>& other = slots[static_cast<std::size_t>(later)];
        if (!other.has_value()) {
            return false;
        }
        if (other->count < here->count && other->item == here->item && other->components == here->components) {
            return false;
        }
    }
    static_cast<void>(stack);
    return true;
}

} // namespace basalt::server::runtime::world::crafters
