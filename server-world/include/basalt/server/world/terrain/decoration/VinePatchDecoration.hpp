#pragma once

#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// Exact step-9 placement for the shared jungle `vines` feature.
namespace vinePatchDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

/// One vine at one position, which cave decoration reuses.
void placeCaveAt(Chunk& chunk, const DecorationPos& position);

} // namespace vinePatchDecoration

} // namespace basalt::server::world::terrain::decoration
