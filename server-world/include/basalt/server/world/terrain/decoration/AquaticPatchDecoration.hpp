#pragma once

#include <basalt/server/world/terrain/decoration/PatchRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// Exact step-9 placement for the eleven seagrass, sea pickle and kelp patches.
namespace aquaticPatchDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalStateReader externalBlockState = nullptr, BlockWriteObserver observer = nullptr);

/// One named patch on its own, for `/place feature`.
void decorateFeature(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                     std::string_view name, ExternalStateReader externalBlockState = nullptr,
                     BlockWriteObserver observer = nullptr);

/// The ids `/place feature` knows here: the eleven placed features and the configured ones behind
/// them (`seagrass_short` to `seagrass_tall`, `sea_pickle`, `kelp`).
[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

/// One of them at one position, through the same bodies the generation runs. parity:
/// `PlaceCommand.placeFeature` runs the feature there without its placement, so a placed id runs the
/// feature it names.
bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view name);

} // namespace aquaticPatchDecoration

} // namespace basalt::server::world::terrain::decoration
