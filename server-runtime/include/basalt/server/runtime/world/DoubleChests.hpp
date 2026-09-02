#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace basalt::server::runtime::world {

/// The two halves of a double chest, and which of them the window shows first.
///
/// parity: `ChestBlock.getConnectedDirection` and `DOUBLE_INVENTORY_COMBINER`. Which half comes first
/// is not "the one that was clicked": a right half leads, a left half follows, so both chests show
/// the same six rows in the same order whichever one you open.
namespace doubleChests {

using Half = std::array<std::int32_t, 3>;

/// The pair at these coordinates, first half first, or nothing when this is a single chest.
[[nodiscard]] std::optional<std::pair<Half, Half>> halves(WorldState& world, std::int32_t x, std::int32_t y,
                                                          std::int32_t z);

} // namespace doubleChests

} // namespace basalt::server::runtime::world
