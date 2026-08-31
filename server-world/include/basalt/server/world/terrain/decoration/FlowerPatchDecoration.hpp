#pragma once

#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// The step-9 dispatcher for the thirteen flower placed features. Each spec runs its own placement
/// modifiers from the chunk corner, filters what they yield by biome, and hands the surviving
/// positions to that feature's own body.
namespace flowerPatchDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

/// One named spec on its own, for `/place feature`.
void decorateFeature(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                     std::string_view name);

} // namespace flowerPatchDecoration

} // namespace basalt::server::world::terrain::decoration
