#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `RailState`.
namespace rails {

using Changed = std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId)>;

[[nodiscard]] bool isRail(std::int32_t stateId);

/// parity: `RailState.place` with `first = true`, as `BaseRailBlock.onPlace` runs it.
std::optional<std::int32_t> place(WorldState& world, std::string_view worldName, std::int32_t x, std::int32_t y,
                                  std::int32_t z, const Changed& changed = {});

/// parity: `BaseRailBlock.updateState`.
std::optional<std::int32_t> onNeighbourChange(WorldState& world, std::string_view worldName, std::int32_t x,
                                              std::int32_t y, std::int32_t z, std::int32_t changedStateId);

} // namespace rails

} // namespace basalt::server::runtime::world
