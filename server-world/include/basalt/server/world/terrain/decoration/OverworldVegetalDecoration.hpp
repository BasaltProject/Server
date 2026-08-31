#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {

/// Runs the 106 Overworld `VEGETAL_DECORATION` nodes in FeatureSorter index order.
namespace overworldVegetalDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment);

/// The global index and name of every node, in the order this step runs them.
[[nodiscard]] const std::vector<std::pair<std::int32_t, std::string_view>>& featureOrder();

} // namespace overworldVegetalDecoration

} // namespace basalt::server::world::terrain::decoration
