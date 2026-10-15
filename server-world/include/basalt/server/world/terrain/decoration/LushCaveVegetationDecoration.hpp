#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/Placement.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace basalt::server::world::terrain::decoration {

namespace lushCaveVegetationDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

void decorateFeature(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                     std::string_view name);

[[nodiscard]] const std::vector<std::string_view>& featureNames();

} // namespace lushCaveVegetationDecoration

} // namespace basalt::server::world::terrain::decoration
