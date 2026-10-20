#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/BiomePresence.hpp>
#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// parity: the sulfur and dripstone cluster and point features, global indexes 2 to 5 of the.
namespace caveSpeleothemDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, ExternalBiomeReader externalBiome = nullptr);

void decorateFeatures(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                      std::int32_t firstIndex, std::int32_t lastIndex, ExternalStateReader externalBlockState = nullptr,
                      ExternalBiomeReader externalBiome = nullptr);

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

} // namespace caveSpeleothemDecoration

} // namespace basalt::server::world::terrain::decoration
