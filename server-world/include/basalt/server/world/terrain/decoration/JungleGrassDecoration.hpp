#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

namespace basalt::server::world::terrain::decoration {

/// parity: `patch_grass_jungle` from `BiomeDefaultFeatures.addJungleGrass`. The block choice is a
/// `WeightedStateProvider` at short_grass 3, fern 1, which reduces to `nextInt(4) < 3`.
namespace jungleGrassDecoration {

void decorate(Chunk& chunk, random::RandomSource& random);

void placePatchAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                  const PlacementContext& context);

} // namespace jungleGrassDecoration

} // namespace basalt::server::world::terrain::decoration
