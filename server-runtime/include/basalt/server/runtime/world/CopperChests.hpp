#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

namespace copperChests {

/// parity: `#minecraft:copper_chests`.
[[nodiscard]] bool isCopperChest(std::string_view path);

/// Waxed counts as bare: `CopperChestBlock`'s `weatherState` ignores wax.
[[nodiscard]] std::int32_t oxidationOf(std::string_view path);

/// parity: `COPPER_TO_COPPER_CHEST_MAPPING`.
[[nodiscard]] std::string chestForBlock(std::string_view copperBlockPath);

/// parity: `CopperChestBlock.getFromCopperBlock`.
[[nodiscard]] std::optional<std::int32_t> placedState(const WorldState& world, std::int32_t x, std::int32_t y,
                                                      std::int32_t z, std::string_view chestPath,
                                                      const std::string& facing);

} // namespace copperChests

} // namespace basalt::server::runtime::world
