#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world::worldSpawnPoint {

/// parity: `MinecraftServer.setInitialSpawn` plus `PlayerSpawnFinder`. Where the climate search
/// lands is only the chunk; the block itself comes from a spiral of up to eleven by eleven chunks
/// around it, and the first column in the first chunk that can hold a player wins.

/// parity: the eleven by eleven the spiral walks, and the five chunks it reaches.
inline constexpr std::int32_t kSpiralChunks = 11;
inline constexpr std::int32_t kReach = 5;

/// A fully generated chunk at these coordinates.
using ChunkAt = std::function<const server::world::Chunk&(std::int32_t chunkX, std::int32_t chunkZ)>;

/// The spawn block, or nothing when nothing in reach can hold a player.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> find(std::int32_t spawnChunkX, std::int32_t spawnChunkZ,
                                                              const ChunkAt& chunkAt);

/// parity: `PlayerSpawnFinder.getSpawnPosInChunk`, which walks x first and takes the first hit.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> spawnInChunk(const server::world::Chunk& chunk);

} // namespace basalt::server::runtime::world::worldSpawnPoint
