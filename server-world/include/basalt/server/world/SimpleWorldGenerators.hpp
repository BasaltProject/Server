#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkHeight.hpp>

#include <cstdint>

namespace basalt::server::world {

/// The single hardcoded layer stack every chunk gets: bedrock, two layers of dirt, grass on top.
/// parity: vanilla's default flat preset, checked block for block against a real server's chunk.
/// `generator-settings` is not read yet.
namespace flatWorldGenerator {

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight verticalBounds = kOverworldHeight);

} // namespace flatWorldGenerator

/// Nothing at all: every chunk is air. A Basalt world type (vanilla's closest is an empty superflat)
/// for lobbies. `platformRadius` blocks of stone go under the spawn so nobody falls out; 0 gives a
/// truly empty world.
namespace voidWorldGenerator {

inline constexpr std::int32_t kDefaultPlatformRadius = 4;

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t spawnX = 8,
                             std::int32_t spawnY = 64, std::int32_t spawnZ = 8,
                             std::int32_t platformRadius = kDefaultPlatformRadius,
                             ChunkHeight verticalBounds = kOverworldHeight);

} // namespace voidWorldGenerator

/// parity: `DebugLevelSource`, every block state in id order on a grid of odd coordinates at y 70,
/// over a barrier floor at y 60.
namespace debugWorldGenerator {

inline constexpr std::int32_t kStateY = 70;
inline constexpr std::int32_t kBarrierY = 60;

/// The state at a column, air between the grid points and past the last state.
[[nodiscard]] std::int32_t stateAt(std::int32_t worldX, std::int32_t worldZ);

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight verticalBounds = kOverworldHeight);

} // namespace debugWorldGenerator

} // namespace basalt::server::world
