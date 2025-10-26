#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// The four Overworld `LAKES` nodes, step 1, in FeatureSorter index order. Every biome but
/// `deep_dark` lists the two lava lakes first, so the global order is forced and needs no
/// tie-breaking.
namespace overworldLakeDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

/// One node by name, which is what a test or `/place feature` reaches for.
void decorateFeature(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                     std::string_view name);

} // namespace overworldLakeDecoration

} // namespace basalt::server::world::terrain::decoration
