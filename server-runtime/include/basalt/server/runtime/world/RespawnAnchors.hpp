#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// The block that takes glowstone by the piece, and that a comparator reads by how full it is.
///
/// parity: `RespawnAnchorBlock.charge`, four charges and no more. Setting a spawn point on it is the
/// player's half and lives with the rest of the respawn code.
namespace respawnAnchors {

[[nodiscard]] bool isAnchor(std::int32_t stateId);

/// One charge more, or nothing when this is not an anchor or it already holds four.
[[nodiscard]] std::optional<BlockChange> charge(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace respawnAnchors

} // namespace basalt::server::runtime::world
