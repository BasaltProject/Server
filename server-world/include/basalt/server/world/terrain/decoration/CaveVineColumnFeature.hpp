#pragma once

#include <basalt/server/world/terrain/decoration/Placement.hpp>

namespace basalt::server::world::terrain::decoration {

namespace caveVineColumnFeature {

bool place(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, bool inMoss = false);

} // namespace caveVineColumnFeature

} // namespace basalt::server::world::terrain::decoration
