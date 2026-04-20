#pragma once

#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

namespace aquaticPatchDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

void decorateFeature(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                     std::string_view name, ExternalStateReader externalBlockState = nullptr,
                     BlockWriteObserver observer = nullptr);

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

/// One of them at one position, through the same bodies the generation runs.
bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view name);

} // namespace aquaticPatchDecoration

} // namespace basalt::server::world::terrain::decoration
