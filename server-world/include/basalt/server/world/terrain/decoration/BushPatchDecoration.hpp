#pragma once

#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

namespace bushPatchDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace bushPatchDecoration

} // namespace basalt::server::world::terrain::decoration
