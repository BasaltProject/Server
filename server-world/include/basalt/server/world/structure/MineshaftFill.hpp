#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/MineshaftLayout.hpp>

#include <cstdint>
#include <functional>
#include <string>

namespace basalt::server::world::structure {

/// parity: the four `postProcess` bodies of `MineshaftPieces`.
namespace mineshaftFill {

using BiomeAt = std::function<std::string(std::int32_t, std::int32_t, std::int32_t)>;

void place(const MineshaftPiece& piece, const MineshaftBlocks& blocks, Chunk& chunk, StructureBoundingBox clip,
           random::RandomSource& random, const BiomeAt& biomeAt = nullptr);

} // namespace mineshaftFill

} // namespace basalt::server::world::structure
