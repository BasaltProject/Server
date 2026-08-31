#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration {

/// The step-9 placement shells for the seven lush cave vegetation features.
namespace lushCaveVegetationDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

void decorateFeature(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                     std::string_view name);

/// The placed feature names this step draws, in the order it draws them.
[[nodiscard]] const std::vector<std::string_view>& featureNames();

} // namespace lushCaveVegetationDecoration

} // namespace basalt::server::world::terrain::decoration
