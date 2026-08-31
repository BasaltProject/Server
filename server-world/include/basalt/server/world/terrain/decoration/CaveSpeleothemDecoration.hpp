#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// parity: the sulfur and dripstone cluster and point features, global indexes 2 to 5 of the
/// underground decoration step.
///
/// The pointed features share one configured-feature graph and the clusters share another; only the
/// biome, the base and point blocks, the replaceable tag and a few providers differ. Both families
/// reach past their own chunk, so the external reader models the surrounding region and virtual
/// writes are kept between attempts.
namespace caveSpeleothemDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr);

/// A contiguous run of this step's own global indexes, for a caller driving one at a time.
void decorateFeatures(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                      std::int32_t firstIndex, std::int32_t lastIndex,
                      ExternalStateReader externalBlockState = nullptr);

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

/// One of them at one position, which is what `/place feature` runs.
bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

} // namespace caveSpeleothemDecoration

} // namespace basalt::server::world::terrain::decoration
