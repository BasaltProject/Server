#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkHeight.hpp>

#include <cstdint>

namespace basalt::server::world {

/// parity: vanilla's default flat preset, checked block for block against a real server's chunk.
namespace flatWorldGenerator {

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight verticalBounds = kOverworldHeight);

} // namespace flatWorldGenerator

namespace voidWorldGenerator {

inline constexpr std::int32_t kDefaultPlatformRadius = 4;

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t spawnX = 8,
                             std::int32_t spawnY = 64, std::int32_t spawnZ = 8,
                             std::int32_t platformRadius = kDefaultPlatformRadius,
                             ChunkHeight verticalBounds = kOverworldHeight);

} // namespace voidWorldGenerator

/// parity: `DebugLevelSource`.
namespace debugWorldGenerator {

inline constexpr std::int32_t kStateY = 70;
inline constexpr std::int32_t kBarrierY = 60;

[[nodiscard]] std::int32_t stateAt(std::int32_t worldX, std::int32_t worldZ);

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight verticalBounds = kOverworldHeight);

} // namespace debugWorldGenerator

} // namespace basalt::server::world
