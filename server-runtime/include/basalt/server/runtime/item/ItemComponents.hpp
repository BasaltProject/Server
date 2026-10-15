#pragma once

#include <string_view>

namespace basalt::server::runtime::item {

namespace itemComponents {

inline constexpr std::string_view kEnchantments = "minecraft:enchantments";
inline constexpr std::string_view kStoredEnchantments = "minecraft:stored_enchantments";

inline constexpr std::string_view kDamage = "minecraft:damage";

inline constexpr std::string_view kRepairCost = "minecraft:repair_cost";

inline constexpr std::string_view kCustomName = "minecraft:custom_name";

inline constexpr std::string_view kUnbreakable = "minecraft:unbreakable";

} // namespace itemComponents

} // namespace basalt::server::runtime::item
