#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/BiomeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/PlacementRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// parity: `BambooFeature` and the `bamboo` and `bamboo_light` placed features.
namespace bambooDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr);

void decorateFeature(std::int32_t index, Chunk& chunk, std::int64_t seed,
                     const surface::SurfaceRuleEnvironment& environment,
                     ExternalStateReader externalBlockState = nullptr);

[[nodiscard]] BiomeVegetalDecoration light();

[[nodiscard]] BiomeVegetalDecoration dense();

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

} // namespace bambooDecoration

} // namespace basalt::server::world::terrain::decoration
