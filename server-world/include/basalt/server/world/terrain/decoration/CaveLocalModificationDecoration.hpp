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

using ExternalSurfaceHeightReader = std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t)>;

/// parity: `amethyst_geode` and `large_dripstone`.
namespace caveLocalModificationDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              ExternalSurfaceHeightReader externalSurfaceHeight = nullptr,
              ExternalStateReader externalBlockState = nullptr);

void decorateFeatures(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                      std::int32_t firstIndex, std::int32_t lastIndex,
                      ExternalSurfaceHeightReader externalSurfaceHeight = nullptr,
                      ExternalStateReader externalBlockState = nullptr);

[[nodiscard]] const std::set<std::string, std::less<>>& placedIds();

bool placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view placedFeature);

/// parity: `AmethystClusterBlock.canSurvive`.
[[nodiscard]] bool canAmethystClusterGrowAtState(std::int32_t stateId);

[[nodiscard]] bool isSourceAmethystWater(std::int32_t stateId);

} // namespace caveLocalModificationDecoration

} // namespace basalt::server::world::terrain::decoration
