#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `RespawnAnchorBlock.charge`.
namespace respawnAnchors {

[[nodiscard]] bool isAnchor(std::int32_t stateId);

[[nodiscard]] std::optional<BlockChange> charge(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace respawnAnchors

} // namespace basalt::server::runtime::world
