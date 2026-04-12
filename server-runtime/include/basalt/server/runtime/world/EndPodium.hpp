#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/ChunkHeight.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// The bedrock podium at the middle of the End, where the exit portal stands.
/// parity: `EndPodiumFeature`, closed, since the dragon is alive when it goes down; the fight lights
/// it on the dragon's death. `worlds.toml` may ask for it open from the start, read as a dragon
/// already killed: a deliberate divergence, off by default.
namespace endPodium {

inline constexpr std::int32_t kPodiumRadius = 4;

/// Whether the podium is already there, which stops it being rebuilt on every load. `bounds` are the
/// dimension's own, always the End's.
[[nodiscard]] bool exists(const WorldState& world, server::world::ChunkHeight bounds = server::world::kEndHeight);

/// Where the podium's portal layer is, found by its pillar, or nothing when there is no podium.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
locate(const WorldState& world, server::world::ChunkHeight bounds = server::world::kEndHeight);

/// Whether the portal on that layer is lit. parity: `EnderDragonFight.hasActiveExitPortal`.
[[nodiscard]] bool isActive(const WorldState& world, std::int32_t originY);

/// parity: `EnderDragonFight.spawnExitPortal`'s choice of layer for a podium it has to build.
[[nodiscard]] std::int32_t exitPortalY(WorldState& world,
                                       server::world::ChunkHeight bounds = server::world::kEndHeight);

/// Builds the podium on that layer, lit or not, over whatever is there.
void placeAt(WorldState& world, std::int32_t originY, bool active);

/// Builds it if it is not there yet, and answers where its top block ended up.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
ensure(WorldState& world, bool active, server::world::ChunkHeight bounds = server::world::kEndHeight);

} // namespace endPodium

} // namespace basalt::server::runtime::world
