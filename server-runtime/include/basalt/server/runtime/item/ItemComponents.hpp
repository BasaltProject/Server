#pragma once

#include <string_view>

namespace basalt::server::runtime::item {

/// The names of the item components this build reads by hand.
///
/// parity: `DataComponents`' own registry names, which is what a stack keeps them under both in
/// `players.db` and in the NBT a command writes. `ItemComponentCodecs`, which turns each of them
/// into its network form, is not ported yet and will read these same names rather than its own.
namespace itemComponents {

/// Where an enchanted stack keeps its levels, by name.
inline constexpr std::string_view kEnchantments = "minecraft:enchantments";
inline constexpr std::string_view kStoredEnchantments = "minecraft:stored_enchantments";

/// parity: `DataComponents.DAMAGE`, the wear a client draws its bar from.
inline constexpr std::string_view kDamage = "minecraft:damage";

/// parity: `DataComponents.REPAIR_COST`, the prior work an anvil charges for.
inline constexpr std::string_view kRepairCost = "minecraft:repair_cost";

/// parity: `DataComponents.CUSTOM_NAME`, what an anvil writes when it renames something.
inline constexpr std::string_view kCustomName = "minecraft:custom_name";

/// parity: `DataComponents.UNBREAKABLE`, whose presence alone stops a tool wearing.
inline constexpr std::string_view kUnbreakable = "minecraft:unbreakable";

} // namespace itemComponents

} // namespace basalt::server::runtime::item
