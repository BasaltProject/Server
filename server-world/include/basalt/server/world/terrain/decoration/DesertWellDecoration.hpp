#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// parity: `desert_well`, global index 2 of the Overworld `SURFACE_STRUCTURES` step.
namespace desertWellDecoration {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              const structure::TemplateLoader& templates);

void placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
             const structure::TemplateLoader& templates);

} // namespace desertWellDecoration

} // namespace basalt::server::world::terrain::decoration
