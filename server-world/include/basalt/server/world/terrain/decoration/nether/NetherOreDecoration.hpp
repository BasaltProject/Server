#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/NetherBiomeSource.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration::nether {

/// parity: the ten Nether ore placed features in the underground decoration step.
namespace netherOreDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const NetherBiomeLookup& biomeSource);

/// One feature on its own, addressed by its global index in this step.
bool placeFeature(std::int32_t globalIndex, Chunk& chunk, random::RandomSource& random,
                  const NetherBiomeLookup& biomeSource);

/// The vanilla placed feature names this step runs, by their global index, in the order the step
/// declares them: a caller that iterates this sees the same order vanilla's own list has.
[[nodiscard]] const std::vector<std::pair<std::string, std::int32_t>>& placedIds();

/// One vein at one position, which is what `/place feature` runs.
bool placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
             std::string_view placedFeature);

} // namespace netherOreDecoration

} // namespace basalt::server::world::terrain::decoration::nether
