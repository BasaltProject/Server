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

/// The block that crafts on its own when a signal reaches it.
/// parity: `CrafterBlock`: four ticks after the rising edge it crafts from its nine slots (each loses
/// exactly one) and ejects forward. A disabled slot counts as full for the comparator and refuses
/// hopper items.
namespace crafters {

/// parity: `CrafterBlock`'s nine slots and its four ticks of delay.
inline constexpr std::int32_t kSize = 9;

[[nodiscard]] const wire::common::Identifier& block();

/// parity: `CrafterBlock.neighborChanged`: on the rising edge it books a turn and remembers it, and
/// when the signal goes it forgets both that and the animation.
[[nodiscard]] std::optional<BlockChange> onNeighbourChange(WorldState& world, std::string_view worldName,
                                                           std::int32_t x, std::int32_t y, std::int32_t z);

/// What a crafter needs from the rest of the server to do its turn.
struct Hooks {
    /// What the nine slots make, or nothing when they make nothing.
    std::function<std::optional<server::world::loot::ItemStack>(const server::world::crafting::CraftingInput& input)>
        resultFor;
    /// A free entity id for the item it throws when there is nowhere to put the result.
    std::function<std::int32_t()> allocateEntityId;
    /// The recipe the grid made, with the grid as it was. parity: `CRAFTER_RECIPE_CRAFTED`.
    std::function<void(const server::world::crafting::CraftingInput& input, const blockInventory::Slots& grid)> crafted;
};

/// The turn itself. Answers what changed, so the caller can tell the clients.
///
/// parity: `CrafterBlock.dispenseFrom`: no recipe means the failure sound and nothing else, and a
/// recipe means the result goes out the front and every slot loses one.
[[nodiscard]] std::optional<BlockChange> craft(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                               const Hooks& hooks);

/// parity: `CrafterBlockEntity.serverTick`, six ticks of animation and then the block goes still.
[[nodiscard]] std::optional<BlockChange> tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                              wire::nbt::NbtCompound& data);

/// parity: `CrafterBlockEntity.getRedstoneSignal`, which counts slots rather than items: a slot with
/// one item in it reads the same as a full one, and a slot switched off reads as taken.
[[nodiscard]] std::int32_t comparatorSignal(const wire::nbt::NbtCompound& data);

[[nodiscard]] std::set<std::int32_t> disabledSlots(const wire::nbt::NbtCompound& data);

/// parity: `CrafterBlockEntity.setSlotState` over `slotCanBeDisabled`: only a slot that is **empty**
/// can be switched off, and the switch is what a comparator counts as a full slot. Answers whether
/// anything moved, so the caller knows to tell the clients and the comparators.
bool setSlotState(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t slot, bool enabled);

/// parity: `CrafterBlockEntity.canPlaceItem`, all three parts of it: a slot that is switched off
/// takes nothing, a full one takes nothing, and a slot takes nothing while a **later** enabled slot
/// is empty or holds fewer of the same thing (`smallerStackExist`). That last one is what spreads
/// what a hopper feeds a crafter across the nine slots instead of piling it into the first.
[[nodiscard]] bool accepts(const wire::nbt::NbtCompound& data, const blockInventory::Slots& slots, std::int32_t slot,
                           const server::world::loot::ItemStack& stack);

} // namespace crafters

} // namespace basalt::server::runtime::world
