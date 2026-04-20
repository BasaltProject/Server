#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkHeight.hpp>

#include <cstdint>

namespace basalt::server::world {

/// parity: vanilla's default flat preset, confirmed live rather than assumed: a real 26.2 server.
namespace flatWorld {

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight bounds = kOverworldHeight);

} // namespace flatWorld

namespace voidWorld {

inline constexpr std::int32_t kDefaultPlatformRadius = 4;

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t spawnX = 8,
                             std::int32_t spawnY = 64, std::int32_t spawnZ = 8,
                             std::int32_t platformRadius = kDefaultPlatformRadius,
                             ChunkHeight bounds = kOverworldHeight);

} // namespace voidWorld

} // namespace basalt::server::world
