#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

namespace basalt::server::world::terrain::decoration {

/// The configured moss and clay vegetation patch bodies the lush cave step-9 shells run.
namespace lushVegetationPatchFeature {

bool placeCeilingMoss(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

bool placeFloorMoss(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

/// The clay patch, which picks between the dry and the waterlogged configuration first.
bool placeClay(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

/// One dripleaf at one position, which the clay patch and the lone dripleaf feature share.
bool placeDripleafAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace lushVegetationPatchFeature

} // namespace basalt::server::world::terrain::decoration
