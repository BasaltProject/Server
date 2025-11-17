#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/Hoppers.hpp>

#include <algorithm>
#include <array>
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

/// parity: `Item.getMaxStackSize`, which this build does not read yet.
constexpr std::int32_t kMaxStack = 64;

/// parity: the sides `AbstractFurnaceBlockEntity.getSlotsForFace` opens to a hopper.
constexpr auto kFurnaceSlotsForUp = std::to_array({furnaces::kInputSlot});
constexpr auto kFurnaceSlotsForDown = std::to_array({furnaces::kResultSlot, furnaces::kFuelSlot});
constexpr auto kFurnaceSlotsForSides = std::to_array({furnaces::kFuelSlot});

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

/// A container this build keeps contents for, with the block entity that holds them.
struct Container {
    std::string_view path;
    std::int32_t size = 0;
    wire::nbt::NbtCompound* data = nullptr;
};

[[nodiscard]] std::optional<Container> containerAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
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
    if (!size.has_value()) {
        return std::nullopt;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z);
    if (entity == nullptr) {
        return std::nullopt;
    }
    return Container{.path = path, .size = *size, .data = &entity->data()};
}

/// parity: `WorldlyContainer.getSlotsForFace`: a furnace takes fuel from the side, ingredients
/// from above and gives its result downwards. Everything else opens every slot to every side.
[[nodiscard]] std::vector<std::int32_t> slotsFor(std::string_view path, std::int32_t size, registry::BlockFace from) {
    if (!furnaces::cookerFor(path).has_value()) {
        std::vector<std::int32_t> every(static_cast<std::size_t>(size));
        std::iota(every.begin(), every.end(), 0);
        return every;
    }
    if (from == registry::BlockFace::Down) {
        return {kFurnaceSlotsForDown.begin(), kFurnaceSlotsForDown.end()};
    }
    if (from == registry::BlockFace::Up) {
        return {kFurnaceSlotsForUp.begin(), kFurnaceSlotsForUp.end()};
    }
    return {kFurnaceSlotsForSides.begin(), kFurnaceSlotsForSides.end()};
}

/// parity: Kotlin's `getOrNull`: a face may name a slot the container does not have.
[[nodiscard]] std::optional<ItemStack>* slotAt(Slots& slots, std::int32_t slot) {
    return slot >= 0 && slot < static_cast<std::int32_t>(slots.size()) ? &slots[static_cast<std::size_t>(slot)]
                                                                       : nullptr;
}

[[nodiscard]] bool accepts(const Slots& slots, const wire::common::Identifier& item) {
    return std::ranges::any_of(slots, [&item](const std::optional<ItemStack>& there) {
        return !there.has_value() || (there->item == item && there->count < kMaxStack);
    });
}

void add(Slots& slots, const wire::common::Identifier& item) {
    for (std::optional<ItemStack>& there : slots) {
        if (there.has_value() && (there->item != item || there->count >= kMaxStack)) {
            continue;
        }
        there = there.has_value() ? ItemStack{.item = there->item, .count = there->count + 1}
                                  : ItemStack{.item = item, .count = 1};
        return;
    }
}

/// Puts one of `stack` into the container, answering whether it went in.
[[nodiscard]] bool insert(WorldState& world, std::int32_t x, std::int32_t z, const Container& target,
                          const ItemStack& stack, registry::BlockFace from) {
    Slots slots = blockInventory::read(*target.data, target.size);
    const std::vector<std::int32_t> allowed = slotsFor(target.path, target.size, from);
    for (const std::int32_t slot : allowed) {
        std::optional<ItemStack>* there = slotAt(slots, slot);
        if (there == nullptr) {
            continue;
        }
        if (there->has_value() && ((*there)->item != stack.item || (*there)->count >= kMaxStack)) {
            continue;
        }
        *there = there->has_value() ? ItemStack{.item = (*there)->item, .count = (*there)->count + 1}
                                    : ItemStack{.item = stack.item, .count = 1};
        blockInventory::write(*target.data, slots);
        world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
        return true;
    }
    return false;
}

/// parity: `ejectItems`: the first slot with something in it gives one item to what it faces.
[[nodiscard]] bool push(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, registry::BlockFace out,
                        Slots& slots) {
    const std::int32_t tx = x + registry::stepX(out);
    const std::int32_t ty = y + registry::stepY(out);
    const std::int32_t tz = z + registry::stepZ(out);
    const std::optional<Container> target = containerAt(world, tx, ty, tz);
    if (!target.has_value()) {
        return false;
    }
    for (std::optional<ItemStack>& slot : slots) {
        if (!slot.has_value()) {
            continue;
        }
        if (!insert(world, tx, tz, *target, *slot, registry::opposite(out))) {
            continue;
        }
        slot = slot->count <= 1 ? std::nullopt : std::optional{ItemStack{.item = slot->item, .count = slot->count - 1}};
        return true;
    }
    return false;
}

/// parity: `suckInItems` for the container half: one item out of whatever sits on top.
[[nodiscard]] bool pull(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, Slots& slots) {
    const std::optional<Container> above = containerAt(world, x, y + 1, z);
    if (!above.has_value()) {
        return false;
    }
    Slots source = blockInventory::read(*above->data, above->size);
    const std::vector<std::int32_t> takeable = slotsFor(above->path, above->size, registry::BlockFace::Down);
    for (const std::int32_t slot : takeable) {
        std::optional<ItemStack>* stack = slotAt(source, slot);
        if (stack == nullptr || !stack->has_value()) {
            continue;
        }
        if (!accepts(slots, (*stack)->item)) {
            continue;
        }
        add(slots, (*stack)->item);
        *stack = (*stack)->count <= 1 ? std::nullopt
                                      : std::optional{ItemStack{.item = (*stack)->item, .count = (*stack)->count - 1}};
        blockInventory::write(*above->data, source);
        return true;
    }
    return false;
}

/// parity: `suckInItems`' other half: what is lying in the box above the hopper goes in, whole
/// stack at a time. `Hopper.SUCK_AABB` is the block's own width, from 11/16 up to two blocks.
[[nodiscard]] bool suckIn(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, Slots& slots) {
    bool took = false;
    for (const DroppedItem& item : world.items()) {
        if (item.count <= 0) {
            continue;
        }
        if (item.x < x || item.x > static_cast<double>(x) + 1.0 || item.z < z ||
            item.z > static_cast<double>(z) + 1.0) {
            continue;
        }
        if (item.y < static_cast<double>(y) + kSuckBottom || item.y > static_cast<double>(y) + kSuckTop) {
            continue;
        }
        std::int32_t left = item.count;
        while (left > 0 && accepts(slots, item.item)) {
            add(slots, item.item);
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

} // namespace

bool tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
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
    const std::int32_t cooldown = entity->data().getInt(kCooldownKey, 0);
    if (cooldown > 0) {
        entity->data().putInt(kCooldownKey, cooldown - 1);
        return true;
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
    bool moved = push(world, x, y, z, *out, slots);
    if (!moved) {
        moved = pull(world, x, y, z, slots);
    }
    if (!moved) {
        moved = suckIn(world, x, y, z, slots);
    }
    if (!moved) {
        return false;
    }
    blockInventory::write(entity->data(), slots);
    entity->data().putInt(kCooldownKey, kCooldown);
    return true;
}

} // namespace basalt::server::runtime::world::hoppers
