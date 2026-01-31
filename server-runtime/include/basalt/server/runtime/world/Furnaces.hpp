#pragma once

#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// The three blocks that smelt, one tick at a time.
///
/// parity: `AbstractFurnaceBlockEntity.serverTick`, followed line by line. The fire burns down
/// whether or not anything is cooking, the fuel is only eaten when there is something to cook and
/// room for the result, and a furnace whose fire went out loses its progress two ticks at a time
/// rather than all at once.
///
/// The counters live in the block entity's own NBT under vanilla's keys, so a furnace survives a
/// save and reload mid-smelt.
namespace furnaces {

/// parity: `AbstractFurnaceMenu`'s slot order, which the window and the NBT share.
inline constexpr std::int32_t kSize = 3;
inline constexpr std::int32_t kInputSlot = 0;
inline constexpr std::int32_t kFuelSlot = 1;
inline constexpr std::int32_t kResultSlot = 2;

/// parity: `AbstractFurnaceBlockEntity.getMaxStackSize`.
inline constexpr std::int32_t kDefaultStackSize = 64;

/// parity: `getTotalCookTime`'s own fallback when nothing in the slot cooks, which is the plain
/// furnace's time rather than zero.
inline constexpr std::int32_t kDefaultCookTime = 200;

/// Which of the three cookers a block is, or nothing when it does not cook.
///
/// parity: a campfire cooks too, but it is not a block entity that smelts and runs on its own clock.
[[nodiscard]] std::optional<server::world::crafting::Cooker> cookerFor(std::string_view block);

/// What the world's recipe book cooks a stack into, or nullptr when nothing does.
///
/// The recipe book belongs to the world and is not this file's to reach, so the caller answers it.
/// The recipe stays the caller's: nothing here holds on to it past the call.
using RecipeFor = std::function<const server::world::crafting::Recipe*(const server::world::loot::ItemStack&)>;

/// A furnace as it stands: three slots and four counters.
struct State {
    std::int32_t litTimeRemaining = 0;
    std::int32_t litTotalTime = 0;
    std::int32_t cookingTimer = 0;
    std::int32_t cookingTotalTime = 0;
    blockInventory::Slots items = blockInventory::Slots(kSize);

    [[nodiscard]] bool lit() const noexcept { return litTimeRemaining > 0; }
};

/// What one turn did.
///
/// The two answers are not the same question. `moved` is whether any number changed, and here the
/// state lives in NBT, so it has to be written back whenever it did. `changed` is vanilla's own
/// `changed`, which it only sets when the fuel is spent, a smelt finishes or the fire flips: that is
/// what calls `setChanged`, and `setChanged` is what dirties the chunk **and** what tells the
/// comparator. A furnace counting down does neither, so its saved numbers lag behind the live ones
/// exactly as in the jar.
struct Ticked {
    bool moved = false;
    bool changed = false;
};

/// One tick of the furnace.
[[nodiscard]] Ticked tick(State& state, const RecipeFor& recipeFor, std::int32_t maxStackSize = kDefaultStackSize);

/// What the next item will take, which vanilla asks for whenever the input slot changes rather than
/// every tick. parity: `AbstractFurnaceBlockEntity.getTotalCookTime`.
[[nodiscard]] std::int32_t cookTimeFor(const State& state, const RecipeFor& recipeFor);

/// Reads the counters and slots a saved furnace left behind.
[[nodiscard]] State read(const wire::nbt::NbtCompound& data);

/// Writes it back in the same shape, so the world file stays vanilla's.
void write(const State& state, wire::nbt::NbtCompound& data);

} // namespace furnaces

} // namespace basalt::server::runtime::world
