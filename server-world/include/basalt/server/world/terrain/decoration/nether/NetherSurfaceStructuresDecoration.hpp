#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/NetherBiomeSource.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration::nether {

/// parity: the three basalt deltas placed features in the surface structures step.
namespace netherSurfaceStructuresDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const NetherBiomeLookup& biomeSource);

/// One feature on its own, addressed by its global index in this step.
void decorateFeature(Chunk& chunk, std::int64_t seed, const NetherBiomeLookup& biomeSource, std::int32_t globalIndex);

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

/// One of them at one position, which is what `/place feature` runs.
bool placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
             std::string_view placedFeature);

} // namespace netherSurfaceStructuresDecoration

} // namespace basalt::server::world::terrain::decoration::nether
