#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world::worldSpawnPoint {

/// parity: `setInitialSpawn`.

inline constexpr std::int32_t kSpiralChunks = 11;
inline constexpr std::int32_t kReach = 5;

using ChunkAt = std::function<const server::world::Chunk&(std::int32_t chunkX, std::int32_t chunkZ)>;

[[nodiscard]] std::optional<std::array<std::int32_t, 3>> find(std::int32_t spawnChunkX, std::int32_t spawnChunkZ,
                                                              const ChunkAt& chunkAt);

/// parity: `PlayerSpawnFinder.getSpawnPosInChunk`.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> spawnInChunk(const server::world::Chunk& chunk);

} // namespace basalt::server::runtime::world::worldSpawnPoint
