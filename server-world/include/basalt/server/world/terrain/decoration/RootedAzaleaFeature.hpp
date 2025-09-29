#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>

namespace basalt::server::world::terrain::decoration {

/// parity: `RootSystemFeature`, the root body and the bending azalea tree `rooted_azalea_tree` grows.
namespace rootedAzaleaFeature {

bool place(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace rootedAzaleaFeature

} // namespace basalt::server::world::terrain::decoration
