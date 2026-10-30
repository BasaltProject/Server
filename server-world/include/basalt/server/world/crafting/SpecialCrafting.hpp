#pragma once

#include <basalt/server/world/crafting/Crafting.hpp>

#include <cstdint>

namespace basalt::server::world::crafting {

/// The recipes whose result is a function of what went in. parity: one class each in vanilla.
namespace specialCrafting {

[[nodiscard]] std::optional<loot::ItemStack> assemble(const Recipe& recipe, const CraftingInput& input);

} // namespace specialCrafting

namespace specialRecipes {

inline constexpr std::string_view kDamage = "minecraft:damage";
inline constexpr std::string_view kDyedColor = "minecraft:dyed_color";
inline constexpr std::string_view kBannerPatterns = "minecraft:banner_patterns";
/// parity: `DataComponents.TRIM`, the pattern and material a piece of armour wears.
inline constexpr std::string_view kTrim = "minecraft:trim";

[[nodiscard]] std::int32_t damageOf(const loot::ItemStack& stack);

/// parity: `RepairItemRecipe.assemble`.
[[nodiscard]] std::optional<loot::ItemStack> repair(const loot::ItemStack& first, const loot::ItemStack& second);

/// parity: `DyedItemColor.applyDyes`.
[[nodiscard]] std::int32_t mixDyes(std::optional<std::int32_t> current, const std::vector<std::int32_t>& dyes);

/// Copies `from`'s components onto a new stack of `item`. parity: `createWithOriginalComponents`.
[[nodiscard]] loot::ItemStack withComponentsOf(const wire::common::Identifier& item, std::int32_t count,
                                               const loot::ItemStack& from);

} // namespace specialRecipes

} // namespace basalt::server::world::crafting
