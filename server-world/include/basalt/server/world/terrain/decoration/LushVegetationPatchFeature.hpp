#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

namespace basalt::server::world::terrain::decoration {

namespace lushVegetationPatchFeature {

bool placeCeilingMoss(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

bool placeFloorMoss(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

bool placeClay(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

bool placeDripleafAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace lushVegetationPatchFeature

} // namespace basalt::server::world::terrain::decoration
