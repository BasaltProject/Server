#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/ChunkHeight.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// The bedrock podium at the middle of the End, where the exit portal stands.
///
/// parity: `EndPodiumFeature`, closed, because in vanilla the dragon is still alive when it goes
/// down; the dragon fight lights it when the dragon dies. An operator may ask for it open from the
/// start (`worlds.toml`), which the fight then reads as a dragon already killed: deliberate
/// divergence, off by default.
///
/// *When* it goes down is a second divergence and not deliberate: vanilla builds it the first time a
/// player stands in the End, we build it at startup. Measured on seed 12345 as 65 cells around (0,0).
namespace endPodium {

inline constexpr std::int32_t kPodiumRadius = 4;

/// Whether the podium is already there, which is what stops it being rebuilt on every load.
///
/// The vertical bounds come in as an argument rather than off the world: `LoadedWorld` is not
/// ported, and they are the dimension's own, which for this one is always the End's.
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
