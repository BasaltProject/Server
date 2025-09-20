#pragma once

#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// Exact step-9 placement for the nine short-grass and fern patches.
namespace grassPatchDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

/// One spec on its own, addressed by its global feature index.
void decorateFeature(std::int32_t index, Chunk& chunk, std::int64_t seed,
                     const surface::SurfaceRuleEnvironment& environment,
                     ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

/// The vanilla placed feature names, by the spec that draws them: the name says which provider and
/// how many tries, the biomes decide where generation puts one, and `/place feature` is told instead.
[[nodiscard]] const std::map<std::string, std::int32_t, std::less<>>& placedGrassIds();

/// The patch at one origin, which is what `/place feature` runs.
bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

} // namespace grassPatchDecoration

} // namespace basalt::server::world::terrain::decoration
