#pragma once

#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// Exact step-9 placement for `patch_bush`.
namespace bushPatchDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

/// The patch itself at one origin: what the step calls once it has chosen a spot, and what
/// `/place feature` calls with the spot it was given.
void placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin);

} // namespace bushPatchDecoration

} // namespace basalt::server::world::terrain::decoration
