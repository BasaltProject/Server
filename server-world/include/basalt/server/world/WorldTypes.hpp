#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkHeight.hpp>

#include <cstdint>

namespace basalt::server::world {

/// The single hardcoded layer stack every chunk gets: bedrock, two layers of dirt, grass on top.
///
/// parity: vanilla's default flat preset, confirmed live rather than assumed: a real 26.2 server
/// with `level-type=minecraft:flat` and no `generator-settings` was run, its region read back, and
/// all 98,304 blocks of chunk (0,0) matched. A real flat world has a configurable layer stack, which
/// this does not read yet: only the default preset is replicated.
namespace flatWorld {

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight bounds = kOverworldHeight);

} // namespace flatWorld

/// Nothing at all: every chunk is air. A Basalt world type rather than a parity one, since vanilla's
/// closest is a superflat preset with an empty layer stack. It exists because a lobby wants an empty
/// canvas and no generation cost, which today means installing a plugin.
namespace voidWorld {

/// Blocks of stone under the world spawn, so nobody falls out of a fresh void world before building
/// anything. Zero gives a truly empty one.
inline constexpr std::int32_t kDefaultPlatformRadius = 4;

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t spawnX = 8,
                             std::int32_t spawnY = 64, std::int32_t spawnZ = 8,
                             std::int32_t platformRadius = kDefaultPlatformRadius,
                             ChunkHeight bounds = kOverworldHeight);

} // namespace voidWorld

} // namespace basalt::server::world
