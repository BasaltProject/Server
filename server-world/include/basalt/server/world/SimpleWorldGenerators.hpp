#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkHeight.hpp>

#include <cstdint>

namespace basalt::server::world {

/// The single hardcoded layer stack every chunk gets: bedrock, two layers of dirt, grass on top.
///
/// parity: matches vanilla's default flat preset layers, confirmed live rather than assumed: a real
/// 26.2 server with `level-type=minecraft:flat` and no explicit `generator-settings` was run, its
/// saved region read back with this project's own Anvil reader, and every one of the 98,304 blocks
/// in chunk (0,0) matched this generator's output exactly. Real flat worlds have a configurable
/// layer stack (`generator-settings`), which this does not read yet.
namespace flatWorldGenerator {

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight verticalBounds = kOverworldHeight);

} // namespace flatWorldGenerator

/// Nothing at all: every chunk is air. A Basalt world type rather than a parity one, since
/// vanilla's closest is a superflat preset with an empty layer stack. It exists because a lobby
/// wants an empty canvas and no generation cost, which today means installing a plugin.
///
/// `platformRadius` blocks of stone go under the world spawn so nobody falls out of a fresh void
/// world before building anything; 0 gives a truly empty one.
namespace voidWorldGenerator {

inline constexpr std::int32_t kDefaultPlatformRadius = 4;

[[nodiscard]] Chunk generate(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t spawnX = 8,
                             std::int32_t spawnY = 64, std::int32_t spawnZ = 8,
                             std::int32_t platformRadius = kDefaultPlatformRadius,
                             ChunkHeight verticalBounds = kOverworldHeight);

} // namespace voidWorldGenerator

} // namespace basalt::server::world
