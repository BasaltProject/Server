#pragma once

#include <basalt/server/world/crafting/Crafting.hpp>

namespace basalt::server::world::crafting {

/// The recipes whose result is a function of what went in. parity: one class each in vanilla, all of
/// the same shape: one slot holds the thing being changed, the rest hold what changes it, and the
/// result is that thing with a component put on it.
namespace specialCrafting {

[[nodiscard]] std::optional<loot::ItemStack> assemble(const Recipe& recipe, const CraftingInput& input);

} // namespace specialCrafting

/// The component names and the two arithmetic rules a special recipe writes with.
namespace specialRecipes {

inline constexpr std::string_view kDamage = "minecraft:damage";
inline constexpr std::string_view kDyedColor = "minecraft:dyed_color";
inline constexpr std::string_view kBannerPatterns = "minecraft:banner_patterns";
/// parity: `DataComponents.TRIM`, the pattern and material a piece of armour wears.
inline constexpr std::string_view kTrim = "minecraft:trim";

/// How much damage this stack has taken, from its own components.
[[nodiscard]] std::int32_t damageOf(const loot::ItemStack& stack);

/// parity: `RepairItemRecipe.assemble`: the two bars added together plus five per cent of the item's
/// own, capped at full.
[[nodiscard]] std::optional<loot::ItemStack> repair(const loot::ItemStack& first, const loot::ItemStack& second);

/// parity: `DyedItemColor.applyDyes`, the running average every dye pulls the colour towards.
[[nodiscard]] std::int32_t mixDyes(std::optional<std::int32_t> current, const std::vector<std::int32_t>& dyes);

/// Copies `from`'s components onto a new stack of `item`. parity: `createWithOriginalComponents`.
[[nodiscard]] loot::ItemStack withComponentsOf(const wire::common::Identifier& item, std::int32_t count,
                                               const loot::ItemStack& from);

} // namespace specialRecipes

} // namespace basalt::server::world::crafting
