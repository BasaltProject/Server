#pragma once

#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// Exact step-9 placement for the eight small-mushroom patches.
namespace mushroomPatchDecoration {

/// The specs whose global index runs before the mushroom island feature, and those after it. Vanilla
/// interleaves the island between them, so the split is what keeps the random stream in order.
void decorateBeforeMushroomIsland(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

void decorateAfterMushroomIsland(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

/// One named patch on its own, for `/place feature`.
void decorateFeature(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                     std::string_view name, ExternalStateReader externalBlockState = nullptr,
                     BlockWriteObserver observer = nullptr);

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

} // namespace mushroomPatchDecoration

} // namespace basalt::server::world::terrain::decoration
