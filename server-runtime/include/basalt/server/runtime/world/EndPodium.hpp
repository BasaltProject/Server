#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/ChunkHeight.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: `EndPodiumFeature`.
namespace endPodium {

inline constexpr std::int32_t kPodiumRadius = 4;

/// `bounds` are the dimension's own, always the End's.
[[nodiscard]] bool exists(const WorldState& world, server::world::ChunkHeight bounds = server::world::kEndHeight);

[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
locate(const WorldState& world, server::world::ChunkHeight bounds = server::world::kEndHeight);

/// Whether the portal on that layer is lit. parity: `EnderDragonFight.hasActiveExitPortal`.
[[nodiscard]] bool isActive(const WorldState& world, std::int32_t originY);

/// parity: `EnderDragonFight.spawnExitPortal`'s choice of layer for a podium it has to build.
[[nodiscard]] std::int32_t exitPortalY(WorldState& world,
                                       server::world::ChunkHeight bounds = server::world::kEndHeight);

void placeAt(WorldState& world, std::int32_t originY, bool active);

[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
ensure(WorldState& world, bool active, server::world::ChunkHeight bounds = server::world::kEndHeight);

} // namespace endPodium

} // namespace basalt::server::runtime::world
