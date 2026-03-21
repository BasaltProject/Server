#pragma once

#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::world {

/// The three blocks that smelt, one tick at a time.
/// parity: `AbstractFurnaceBlockEntity.serverTick`: the fire burns down regardless, fuel is eaten
/// only with something to cook and room for the result, and an unlit furnace loses progress two ticks
/// at a time. The counters live in the block entity's NBT under vanilla's keys.
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

/// How many times each recipe has cooked since a player last took the experience, by recipe name.
using RecipesUsed = std::vector<std::pair<std::string, std::int32_t>>;

/// A furnace as it stands: three slots, four counters and what it has cooked.
struct State {
    std::int32_t litTimeRemaining = 0;
    std::int32_t litTotalTime = 0;
    std::int32_t cookingTimer = 0;
    std::int32_t cookingTotalTime = 0;
    /// parity: 26.3's `speedMultiplier`, set from the fuel last lit and dividing every cook time.
    float speedMultiplier = 1.0F;
    blockInventory::Slots items = blockInventory::Slots(kSize);
    /// parity: `AbstractFurnaceBlockEntity.recipesUsed`.
    RecipesUsed recipesUsed;

    [[nodiscard]] bool lit() const noexcept { return litTimeRemaining > 0; }
};

/// What one turn did. `moved` is whether any number changed, which the NBT has to be written back
/// for. `changed` is vanilla's own, set only when the fuel is spent, a smelt finishes or the fire
/// flips: that is what calls `setChanged`, which dirties the chunk and tells the comparator. A
/// furnace counting down does neither, so its saved numbers lag the live ones as in the jar.
struct Ticked {
    bool moved = false;
    bool changed = false;
};

/// Where a fuel's remainder goes when the fuel slot still holds more of it. parity:
/// `Containers.dropItemStack` at the furnace, which only the caller can do.
using DropRemainder = std::function<void(const server::world::loot::ItemStack&)>;

/// One tick of the furnace. `cooker` says which of the three it is, which the fuel reads.
[[nodiscard]] Ticked tick(State& state, const RecipeFor& recipeFor, std::int32_t maxStackSize = kDefaultStackSize,
                          server::world::crafting::Cooker cooker = server::world::crafting::Cooker::Smelting,
                          const DropRemainder& drop = {});

/// How long a fuel keeps this cooker lit, and how fast it cooks while it does. parity: the
/// `minecraft:cooking_fuel` component's two providers.
struct FuelEffect {
    std::int32_t burnTicks = 0;
    float speedMultiplier = 1.0F;
};

[[nodiscard]] FuelEffect fuelEffect(const wire::common::Identifier& fuel, server::world::crafting::Cooker cooker);

/// What the next item will take at the furnace's own speed, which vanilla asks for whenever the
/// input slot changes rather than every tick. parity: `AbstractFurnaceBlockEntity.getTotalCookTime`.
[[nodiscard]] std::int32_t cookTimeFor(const State& state, const RecipeFor& recipeFor);

/// What a cooker owes whoever writes its input slot: the time comes from what is in there now and
/// the progress goes back to zero. parity: `AbstractFurnaceBlockEntity.setItem`, which every write
/// goes through. A call on anything that does not cook is free; the recipe book is the caller's.
void inputChanged(std::string_view block, wire::nbt::NbtCompound& data,
                  const std::function<const server::world::crafting::Recipe*(
                      server::world::crafting::Cooker, const server::world::loot::ItemStack&)>& recipeFor);

/// parity: `AbstractFurnaceBlockEntity.createExperience`: the whole points, and one more with the
/// fraction as its chance.
[[nodiscard]] std::int32_t experienceFor(std::int32_t count, float perItem, random::RandomSource& random);

/// The saved `RecipesUsed` of a block entity, for a furnace that is going away.
[[nodiscard]] RecipesUsed recipesUsedIn(const wire::nbt::NbtCompound& data);

/// Reads the counters and slots a saved furnace left behind.
[[nodiscard]] State read(const wire::nbt::NbtCompound& data);

/// Writes it back in the same shape, so the world file stays vanilla's.
void write(const State& state, wire::nbt::NbtCompound& data);

} // namespace furnaces

} // namespace basalt::server::runtime::world
