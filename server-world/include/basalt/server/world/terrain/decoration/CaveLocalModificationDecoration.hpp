#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/decoration/PlacementRegion.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {

/// The surrounding chunks' frozen world surface, for the one spike that stops against it.
using ExternalSurfaceHeightReader = std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t)>;

/// parity: `amethyst_geode` and `large_dripstone`, global indexes 2 and 3 of `LOCAL_MODIFICATIONS`.
///
/// A geode is 33 blocks across, so most of one lands in the neighbours: the external readers model
/// the surrounding region and the writes outside are deferred rather than dropped.
namespace caveLocalModificationDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalSurfaceHeightReader externalSurfaceHeight = nullptr,
              ExternalStateReader externalBlockState = nullptr);

void decorateFeatures(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                      std::int32_t firstIndex, std::int32_t lastIndex,
                      ExternalSurfaceHeightReader externalSurfaceHeight = nullptr,
                      ExternalStateReader externalBlockState = nullptr);

/// The vanilla placed feature names this step draws.
[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

/// One of them at one position, which is what `/place feature` runs.
bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

/// parity: `AmethystClusterBlock.canSurvive`, exposed because the cluster growth is shared.
[[nodiscard]] bool canAmethystClusterGrowAtState(std::int32_t stateId);

[[nodiscard]] bool isSourceAmethystWater(std::int32_t stateId);

} // namespace caveLocalModificationDecoration

} // namespace basalt::server::world::terrain::decoration
