#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/MineshaftLayout.hpp>

#include <functional>
#include <string>

namespace basalt::server::world::structure {

/// parity: the four `postProcess` bodies of `MineshaftPieces`, which turn an assembled shaft into
/// blocks. Several rolls are conditional on the terrain, so the shaft needs seed-exact ground first.
namespace mineshaftFill {

/// The biome a piece sits in, which decides whether it is allowed to exist at all.
using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;

/// Writes whatever of `piece` falls inside `clip`, which is one chunk's share of it.
void place(const MineshaftPiece& piece, const MineshaftBlocks& blocks, Chunk& chunk, StructureBoundingBox clip,
           random::RandomSource& random, const BiomeAt& biomeAt = nullptr);

} // namespace mineshaftFill

} // namespace basalt::server::world::structure
