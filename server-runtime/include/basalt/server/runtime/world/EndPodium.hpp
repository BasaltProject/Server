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
/// down. There is no dragon here to open it, so an operator may ask for it open (`worlds.toml`):
/// deliberate divergence, off by default.
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

/// Builds it if it is not there yet, and answers where its top block ended up.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>>
ensure(WorldState& world, bool active, server::world::ChunkHeight bounds = server::world::kEndHeight);

} // namespace endPodium

} // namespace basalt::server::runtime::world
