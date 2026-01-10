#pragma once

#include <basalt/server/runtime/command/CommandTree.hpp>

#include <cstdint>
#include <memory>
#include <set>

namespace basalt::server::runtime::command {

/// Where a position means: the feet or the eyes. parity: `EntityAnchorArgument`.
enum class EntityAnchor : std::uint8_t { Feet, Eyes };

/// `minecraft:entity_anchor`: `feet` or `eyes`. The value is an `EntityAnchor`.
[[nodiscard]] std::shared_ptr<const ArgumentType> entityAnchorArgument();

/// `minecraft:swizzle`: a combination of `x`, `y` and `z`, each at most once. parity:
/// `SwizzleArgument`. The value is a `std::set<char>`.
[[nodiscard]] std::shared_ptr<const ArgumentType> swizzleArgument();

/// `minecraft:heightmap`: which surface `/execute positioned over` lands on. The value is a
/// `std::string`.
[[nodiscard]] std::shared_ptr<const ArgumentType> heightmapArgument();

/// `minecraft:item_slots`: one slot name or a group of them, kept as written. The value is a
/// `std::string`.
[[nodiscard]] std::shared_ptr<const ArgumentType> itemSlotsArgument();

/// `minecraft:loot_predicate`: the id of a predicate to test. The value is an `Identifier`.
[[nodiscard]] std::shared_ptr<const ArgumentType> lootPredicateArgument();

/// `minecraft:loot_table`: the id of a loot table. The value is an `Identifier`.
///
/// Not a `minecraft:resource` over the `minecraft:loot_table` registry, which is what this used to
/// be: loot tables are a datapack registry and the client has no copy of one, so a real client threw
/// `Registry minecraft:loot_table not found` building the command tree and dropped the connection.
[[nodiscard]] std::shared_ptr<const ArgumentType> lootTableArgument();

/// `minecraft:loot_modifier`: the id of an item modifier, for the same reason as the loot table.
[[nodiscard]] std::shared_ptr<const ArgumentType> lootModifierArgument();

/// `minecraft:float_range`: `1`, `1..`, `..2` or `1..2`, kept as written. The value is a
/// `std::string`.
[[nodiscard]] std::shared_ptr<const ArgumentType> floatRangeArgument();

/// `minecraft:uuid`: an entity or profile id, kept as written. The value is a `std::string`.
[[nodiscard]] std::shared_ptr<const ArgumentType> uuidArgument();

/// `minecraft:style`: the style half of a component, written as SNBT. The value is an
/// `NbtCompound`.
[[nodiscard]] std::shared_ptr<const ArgumentType> styleArgument();

} // namespace basalt::server::runtime::command
