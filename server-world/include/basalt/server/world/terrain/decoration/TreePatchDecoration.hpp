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

void decorateFeature(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                     std::string_view name, PlacementObserver observer = nullptr);

[[nodiscard]] const std::vector<FeatureSpec>& featureSpecs();

} // namespace treePatchDecoration

} // namespace basalt::server::world::terrain::decoration
