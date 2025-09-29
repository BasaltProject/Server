#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

namespace basalt::server::world::terrain::decoration {

/// parity: the bamboo jungle's `bamboo_vegetation` random selector.
namespace bambooVegetationDecoration {

void decorate(Chunk& chunk, random::RandomSource& random);

void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& position,
             const PlacementContext& context);

} // namespace bambooVegetationDecoration

} // namespace basalt::server::world::terrain::decoration
