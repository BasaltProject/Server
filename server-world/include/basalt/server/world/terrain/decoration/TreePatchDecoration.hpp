#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration {

/// The global step-9 placement for the large tree and forest features.
///
/// Each placed feature owns its official seed and index and applies the biome filter at the
/// randomized origin, not at the chunk centre. A tree may still grow across a biome boundary, which
/// is what vanilla does; only its origin decides whether the placed feature runs.
namespace treePatchDecoration {

struct FeatureSpec {
    std::string name;
    std::int32_t index = 0;
    PlacementModifier frequency;
    std::set<std::string, std::less<>> biomes;
    std::int32_t maxWaterDepth = 0;
    std::string survivalSapling;
};

using PlacementObserver = std::function<void(std::string_view, const DecorationPos&)>;

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

/// One spec on its own, addressed by the placed feature's name.
void decorateFeature(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                     std::string_view name, PlacementObserver observer = nullptr);

[[nodiscard]] const std::vector<FeatureSpec>& featureSpecs();

} // namespace treePatchDecoration

} // namespace basalt::server::world::terrain::decoration
