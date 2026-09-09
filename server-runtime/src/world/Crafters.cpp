#include <basalt/server/registry/BlockStateDefinitions.hpp>
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
constexpr std::int32_t kMaxStack = 64;

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

/// Puts the result into the container in front, answering whether it fitted.
[[nodiscard]] bool insert(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                          const server::world::loot::ItemStack& stack) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return false;
    }
    // How big the container in front is. This used to go through a callback that the one production
    // caller never filled in, so `insert` always answered no and a crafter threw every result on the
    // floor instead of putting it in the chest it was pointing at.
    const std::optional<std::int32_t> size = containers::sizeOf(block->path());
    if (!size.has_value()) {
        return false;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z);
    if (entity == nullptr) {
        return false;
    }
    blockInventory::Slots slots = blockInventory::read(entity->data(), *size);
    for (std::size_t slot = 0; slot < slots.size(); ++slot) {
        if (slots[slot].has_value() &&
            (slots[slot]->item != stack.item || slots[slot]->count + stack.count > kMaxStack)) {
            continue;
        }
        slots[slot] =
            slots[slot].has_value()
                ? server::world::loot::ItemStack{.item = slots[slot]->item, .count = slots[slot]->count + stack.count}
                : stack;
        blockInventory::write(entity->data(), slots);
        world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
        redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
        return true;
    }
    return false;
}

/// parity: `dispenseItem`, which prefers the container in front and throws it out when there is none.
void deliver(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
             const server::world::loot::ItemStack& result, const Hooks& hooks) {
    const std::array<std::int32_t, 3> step = facingStep(stateId);
    const std::int32_t tx = x + step[0];
    const std::int32_t ty = y + step[1];
    const std::int32_t tz = z + step[2];
    if (insert(world, tx, ty, tz, result)) {
        return;
    }
    world.spawnItem(DroppedItem{.entityId = hooks.allocateEntityId ? hooks.allocateEntityId() : 0,
                                .uuid = wire::common::Uuid::random(),
                                .item = result.item,
                                .count = result.count,
                                .x = tx + 0.5,
                                .y = ty + 0.5,
                                .z = tz + 0.5});
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

bool accepts(const wire::nbt::NbtCompound& data, std::int32_t slot) { return !disabledSlots(data).contains(slot); }

} // namespace basalt::server::runtime::world::crafters
