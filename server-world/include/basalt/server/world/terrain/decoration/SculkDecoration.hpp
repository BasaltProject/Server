#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/decoration/PlacementRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// parity: the deep-dark sculk entries at indexes 0 and 1 of `UNDERGROUND_DECORATION`.
///
/// A patch spreads well past its own chunk, so the external reader models the surrounding region
/// and the writes outside are deferred rather than dropped.
namespace sculkDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr);

void decorateFeature(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                     std::int32_t globalIndex, ExternalStateReader externalBlockState = nullptr);

/// The vanilla placed feature names this step draws.
[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

/// One of them at one position, which is what `/place feature` runs.
bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

} // namespace sculkDecoration

} // namespace basalt::server::world::terrain::decoration
