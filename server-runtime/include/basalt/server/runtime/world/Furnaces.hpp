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

/// parity: `AbstractFurnaceBlockEntity.serverTick`.
namespace furnaces {

inline constexpr std::int32_t kSize = 3;
inline constexpr std::int32_t kInputSlot = 0;
inline constexpr std::int32_t kFuelSlot = 1;
inline constexpr std::int32_t kResultSlot = 2;

inline constexpr std::int32_t kDefaultStackSize = 64;

/// parity: `getTotalCookTime` falls back to the plain furnace's time when nothing in the slot cooks.
inline constexpr std::int32_t kDefaultCookTime = 200;

/// parity: a campfire cooks too, but is not a block entity that smelts on its own clock.
[[nodiscard]] std::optional<server::world::crafting::Cooker> cookerFor(std::string_view block);

/// The recipe book belongs to the world, so the caller answers; nothing here keeps the recipe past the call.
using RecipeFor = std::function<const server::world::crafting::Recipe*(const server::world::loot::ItemStack&)>;

using RecipesUsed = std::vector<std::pair<std::string, std::int32_t>>;

struct State {
    std::int32_t litTimeRemaining = 0;
    std::int32_t litTotalTime = 0;
    std::int32_t cookingTimer = 0;
    std::int32_t cookingTotalTime = 0;
    /// parity: 26.3's `speedMultiplier`.
    float speedMultiplier = 1.0F;
    blockInventory::Slots items = blockInventory::Slots(kSize);
    /// parity: `AbstractFurnaceBlockEntity.recipesUsed`.
    RecipesUsed recipesUsed;

    [[nodiscard]] bool lit() const noexcept { return litTimeRemaining > 0; }
};

/// `changed` is vanilla's: set only on fuel spent, smelt finished or fire flip, so saved counters lag the live ones.
struct Ticked {
    bool moved = false;
    bool changed = false;
};

/// parity: `Containers.dropItemStack` at the furnace, which only the caller can do.
using DropRemainder = std::function<void(const server::world::loot::ItemStack&)>;

/// What a plugin may say about a furnace as it works: fuel about to burn, a smelt about to start and one about to
/// finish.
struct Notify {
    std::function<bool(const server::world::loot::ItemStack& fuel, std::int32_t& burnTicks)> burn;
    std::function<void(const server::world::loot::ItemStack& source, std::int32_t totalTicks, std::string_view recipe)>
        started;
    std::function<bool(const server::world::loot::ItemStack& source, server::world::loot::ItemStack& result,
                       std::string_view recipe)>
        smelt;
};

[[nodiscard]] Ticked tick(State& state, const RecipeFor& recipeFor, std::int32_t maxStackSize = kDefaultStackSize,
                          server::world::crafting::Cooker cooker = server::world::crafting::Cooker::Smelting,
                          const DropRemainder& drop = {}, const Notify& notify = {});

/// parity: the `minecraft:cooking_fuel` component's two providers.
struct FuelEffect {
    std::int32_t burnTicks = 0;
    float speedMultiplier = 1.0F;
};

[[nodiscard]] FuelEffect fuelEffect(const wire::common::Identifier& fuel, server::world::crafting::Cooker cooker);

/// parity: `AbstractFurnaceBlockEntity.getTotalCookTime`.
[[nodiscard]] std::int32_t cookTimeFor(const State& state, const RecipeFor& recipeFor);

/// parity: `AbstractFurnaceBlockEntity.setItem`.
void inputChanged(std::string_view block, wire::nbt::NbtCompound& data,
                  const std::function<const server::world::crafting::Recipe*(
                      server::world::crafting::Cooker, const server::world::loot::ItemStack&)>& recipeFor);

/// parity: `AbstractFurnaceBlockEntity.createExperience`.
[[nodiscard]] std::int32_t experienceFor(std::int32_t count, float perItem, random::RandomSource& random);

[[nodiscard]] RecipesUsed recipesUsedIn(const wire::nbt::NbtCompound& data);

[[nodiscard]] State read(const wire::nbt::NbtCompound& data);

void write(const State& state, wire::nbt::NbtCompound& data);

} // namespace furnaces

} // namespace basalt::server::runtime::world
