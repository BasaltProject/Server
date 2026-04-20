#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace basalt::server::runtime::world {

/// parity: `ChestBlock.getConnectedDirection`.
namespace doubleChests {

using Half = std::array<std::int32_t, 3>;

[[nodiscard]] std::optional<std::pair<Half, Half>> halves(WorldState& world, std::int32_t x, std::int32_t y,
                                                          std::int32_t z);

} // namespace doubleChests

} // namespace basalt::server::runtime::world
