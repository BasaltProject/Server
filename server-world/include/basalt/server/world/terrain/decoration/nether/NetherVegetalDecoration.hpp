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

void decorateFeature(Chunk& chunk, std::int64_t seed, const NetherBiomeLookup& biomeSource, std::int32_t globalIndex);

/// parity: the order `FeatureSorter` gives this step.
[[nodiscard]] const std::vector<std::string>& featureOrder();

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

bool placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
             std::string_view placedFeature);

} // namespace netherVegetalDecoration

} // namespace basalt::server::world::terrain::decoration::nether
