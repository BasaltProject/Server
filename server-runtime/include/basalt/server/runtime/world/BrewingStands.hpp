#pragma once

#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::world {

/// The brewing stand, one tick at a time. parity: `BrewingStandBlockEntity.serverTick`, in order:
/// powder is eaten only when fuel has run out, a brew whose ingredient stops matching is abandoned,
/// and the three bottle bits follow the contents. Shortcut: a bottle's potion is kept in the block
/// entity's NBT beside the item rather than as a component.
namespace brewingStands {

/// parity: `BrewingStandBlockEntity`'s five slots: three bottles, the ingredient and the fuel.
inline constexpr std::int32_t kSize = 5;
inline constexpr std::int32_t kBottles = 3;
inline constexpr std::int32_t kIngredientSlot = 3;
inline constexpr std::int32_t kFuelSlot = 4;

[[nodiscard]] const wire::common::Identifier& block();

/// A stand as it stands: five slots, the fuel left and how far along the brew is.
struct State {
    std::int32_t fuel = 0;
    std::int32_t brewTime = 0;
    /// parity: 26.3's `totalBrewTime`, `totalFuel` and `speedMultiplier`, with its load defaults.
    std::int32_t totalBrewTime = 400;
    std::int32_t totalFuel = 20;
    float speedMultiplier = 1.0F;
    /// parity: `ingredient`, what the brew in hand started with; a different one stops it.
    std::optional<wire::common::Identifier> ingredient;
    blockInventory::Slots items = blockInventory::Slots(kSize);
    std::array<std::optional<std::string>, kBottles> potions;
};

/// Where a remainder goes when its slot still holds more of what made it. parity:
/// `Containers.dropItemStack` at the stand, which only the caller can do.
using DropRemainder = std::function<void(const server::world::loot::ItemStack&)>;

/// The jar's `minecraft:brewing` recipes, set once at startup before any stand ticks. Until then,
/// or when the jar has none, the generated mix table stands in.
void useRecipes(std::vector<server::world::crafting::Recipe> recipes);

/// What a bottle holding `potion` becomes with `reagent`, or nothing. parity: 26.3
/// `BrewingRecipe.matches` and `assemble`, which hands out the recipe's own output.
[[nodiscard]] std::optional<server::world::loot::ItemStack> brewed(const server::world::loot::ItemStack& bottle,
                                                                   const std::optional<std::string>& potion,
                                                                   const wire::common::Identifier& reagent);

/// parity: the `brewing_reagent` property set, what the ingredient slot takes.
[[nodiscard]] bool isReagent(const wire::common::Identifier& item);

/// parity: `PotionIngredient.isPotionInput`: the `brewing_input` set or `#brewing_potion_inputs`.
[[nodiscard]] bool isPotionInput(const wire::common::Identifier& item);

/// One turn. Answers whether anything moved, so the caller writes it back only when it did.
bool tick(State& state, const DropRemainder& drop = {});

/// parity: `isBrewable`: an ingredient that brews, and at least one bottle it can turn.
[[nodiscard]] bool isBrewable(const State& state);

/// Which of the three bottle slots are taken, which is what the block itself shows.
[[nodiscard]] std::array<bool, kBottles> bottleBits(const State& state);

[[nodiscard]] State read(const wire::nbt::NbtCompound& data);

void write(const State& state, wire::nbt::NbtCompound& data);

} // namespace brewingStands

} // namespace basalt::server::runtime::world
