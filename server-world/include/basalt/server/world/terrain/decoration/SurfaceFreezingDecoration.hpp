#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// parity: `SnowAndFreezeFeature`, the global Overworld `freeze_top_layer`.
namespace surfaceFreezingDecoration {

void placeAt(Chunk& chunk, const surface::SurfaceRuleEnvironment& environment, std::int32_t x, std::int32_t z);

void decorate(Chunk& chunk, const surface::SurfaceRuleEnvironment& environment);

} // namespace surfaceFreezingDecoration

} // namespace basalt::server::world::terrain::decoration
