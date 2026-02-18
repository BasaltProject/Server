#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// The copper chest, which is the one chest that pairs with a chest of a different oxidation.
namespace copperChests {

/// parity: every copper chest is in `#minecraft:copper_chests`, waxed included, and nothing else
/// ends in `copper_chest`.
[[nodiscard]] bool isCopperChest(std::string_view path);

/// How far along the copper is, from zero for unaffected to three for oxidized. Waxed counts the
/// same as bare: `CopperChestBlock`'s `weatherState` does not know about the wax.
[[nodiscard]] std::int32_t oxidationOf(std::string_view path);

/// The chest a copper block turns into. parity: `COPPER_TO_COPPER_CHEST_MAPPING`, which zips the
/// copper block family with the chest one, plus the plain `copper_chest` for anything else in
/// `#minecraft:copper`.
[[nodiscard]] std::string chestForBlock(std::string_view copperBlockPath);

/// The state a copper chest lands in at `x`, `y`, `z` facing `facing`.
///
/// parity: `CopperChestBlock.getFromCopperBlock`, which asks `getChestType` for the side and then
/// `getLeastOxidizedChestOfConnectedBlocks` for which of the two halves' copper wins.
[[nodiscard]] std::optional<std::int32_t> placedState(const WorldState& world, std::int32_t x, std::int32_t y,
                                                      std::int32_t z, std::string_view chestPath,
                                                      const std::string& facing);

} // namespace copperChests

} // namespace basalt::server::runtime::world
