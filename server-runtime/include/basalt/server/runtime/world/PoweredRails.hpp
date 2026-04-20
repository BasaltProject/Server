#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `PoweredRailBlock.updateState`.
namespace poweredRails {

inline constexpr std::int32_t kSearchLimit = 8;

[[nodiscard]] std::optional<BlockChange> update(WorldState& world, std::string_view worldName, std::int32_t x,
                                                std::int32_t y, std::int32_t z);

} // namespace poweredRails

} // namespace basalt::server::runtime::world
