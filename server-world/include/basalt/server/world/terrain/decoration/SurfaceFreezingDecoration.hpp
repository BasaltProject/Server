#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// parity: `SnowAndFreezeFeature`, the global Overworld `freeze_top_layer`.
///
/// Vanilla runs it in `TOP_LAYER_MODIFICATION`, after vegetal decoration, in one pass over all 256
/// columns using the motion-blocking heightmap. Block light is zero during generation, so its
/// light check is satisfied by construction and is not ported as a condition.
///
/// Snow also updates the `snowy` property of the grass block, podzol or mycelium beneath it.
namespace surfaceFreezingDecoration {

/// One complete 16 by 16 body at the given origin.
void placeAt(Chunk& chunk, const surface::SurfaceRuleEnvironment& environment, std::int32_t x, std::int32_t z);

void decorate(Chunk& chunk, const surface::SurfaceRuleEnvironment& environment);

} // namespace surfaceFreezingDecoration

} // namespace basalt::server::world::terrain::decoration
