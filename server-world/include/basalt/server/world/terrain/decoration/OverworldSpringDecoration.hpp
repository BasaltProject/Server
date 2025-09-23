#pragma once

#include <basalt/server/world/terrain/decoration/PlacementRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// parity: the three Overworld `FLUID_SPRINGS` placed features at global indexes 0 through 2.
namespace overworldSpringDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr);

/// A contiguous range of global indexes, for callers that interleave other features between them.
void decorateFeatures(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                      std::int32_t firstIndex, std::int32_t lastIndex,
                      ExternalStateReader externalBlockState = nullptr);

[[nodiscard]] const std::set<std::string, std::less<>>& placedSpringIds();

/// One spring at one position: the feature without the modifiers that scatter it, which is what
/// `/place feature` runs.
bool placeAt(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::string_view placedFeature);

} // namespace overworldSpringDecoration

} // namespace basalt::server::world::terrain::decoration
