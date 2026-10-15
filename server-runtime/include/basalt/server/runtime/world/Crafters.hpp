#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `CrafterBlock`.
namespace crafters {

inline constexpr std::int32_t kSize = 9;

[[nodiscard]] const wire::common::Identifier& block();

/// parity: `CrafterBlock.neighborChanged`.
[[nodiscard]] std::optional<BlockChange> onNeighbourChange(WorldState& world, std::string_view worldName,
                                                           std::int32_t x, std::int32_t y, std::int32_t z);

struct Hooks {
    std::function<std::optional<server::world::loot::ItemStack>(const server::world::crafting::CraftingInput& input)>
        resultFor;
    std::function<std::int32_t()> allocateEntityId;
    /// The recipe the grid made, with the grid as it was. parity: `CRAFTER_RECIPE_CRAFTED`.
    std::function<void(const server::world::crafting::CraftingInput& input, const blockInventory::Slots& grid)> crafted;
};

/// parity: `CrafterBlock.dispenseFrom`.
[[nodiscard]] std::optional<BlockChange> craft(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               const Hooks& hooks);

/// parity: `CrafterBlockEntity.serverTick`.
[[nodiscard]] std::optional<BlockChange> tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                              wire::nbt::NbtCompound& data);

/// parity: `CrafterBlockEntity.getRedstoneSignal` counts slots.
[[nodiscard]] std::int32_t comparatorSignal(const wire::nbt::NbtCompound& data);

[[nodiscard]] std::set<std::int32_t> disabledSlots(const wire::nbt::NbtCompound& data);

/// parity: `CrafterBlockEntity.setSlotState` over `slotCanBeDisabled`.
bool setSlotState(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t slot, bool enabled);

/// parity: `CrafterBlockEntity.canPlaceItem`.
[[nodiscard]] bool accepts(const wire::nbt::NbtCompound& data, const blockInventory::Slots& slots, std::int32_t slot,
                           const server::world::loot::ItemStack& stack);

} // namespace crafters

} // namespace basalt::server::runtime::world
