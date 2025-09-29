#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/NetherBiomeSource.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration::nether {

/// parity: the Nether placed features in the vegetal decoration step.
namespace netherVegetalDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const NetherBiomeLookup& biomeSource);

/// One feature on its own, addressed by its global index in this step.
void decorateFeature(Chunk& chunk, std::int64_t seed, const NetherBiomeLookup& biomeSource, std::int32_t globalIndex);

/// parity: the order `FeatureSorter` gives this step, which is what each feature is seeded with. It
/// follows the five Nether biomes in parameter-list order, so crimson forest's features are numbered
/// before warped forest's even though the warped ones sort first.
[[nodiscard]] const std::vector<std::string>& featureOrder();

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

/// One of them at one position, which is the feature without the modifiers that scatter it over a
/// chunk: what `/place feature` runs.
bool placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
             std::string_view placedFeature);

} // namespace netherVegetalDecoration

} // namespace basalt::server::world::terrain::decoration::nether
