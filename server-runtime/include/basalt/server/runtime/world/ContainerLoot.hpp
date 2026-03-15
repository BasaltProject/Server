#pragma once

#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

class LoadedWorld;
class RealLootTables;

/// A container's loot table, rolled the first time anything reads its slots.
///
/// parity: `RandomizableContainer`: a structure chest keeps a `LootTable` and a `LootTableSeed`
/// instead of items until it is opened, broken, emptied by a hopper or read by a comparator.
namespace containerLoot {

inline constexpr std::string_view kTableKey = "LootTable";
inline constexpr std::string_view kSeedKey = "LootTableSeed";

/// Rolls `table` into `slots`. A zero seed rolls on the table's own stream, as the jar does.
/// `player` is whoever opened it, which only decides whether the roll has an entity behind it.
void fill(LoadedWorld& world, RealLootTables& tables, std::string_view table, std::int64_t seed,
          std::vector<std::optional<server::world::loot::ItemStack>>& slots, bool byPlayer,
          const std::array<std::int32_t, 3>& at);

/// parity: `RandomizableContainer.unpackLootTable`, on the block entity at a position. Answers the
/// table it rolled, for the advancement that waits on one.
std::optional<wire::common::Identifier> unpack(LoadedWorld& world, RealLootTables& tables, std::int32_t x,
                                               std::int32_t y, std::int32_t z,
                                               const std::optional<wire::common::Uuid>& player);

} // namespace containerLoot

} // namespace basalt::server::runtime::world
