#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// parity: `desert_well`, global index 2 of the Overworld `SURFACE_STRUCTURES` step.
namespace desertWellDecoration {

/// The well and its sand are structure templates from the operator's jar: without `templates` the
/// feature places nothing, as it cannot know their shape.
void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              const structure::TemplateLoader& templates);

/// The feature itself at one position: what the pass calls per origin and what `/place feature`
/// calls once, at the sand block the well sits on. World coordinates.
void placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
             const structure::TemplateLoader& templates);

} // namespace desertWellDecoration

} // namespace basalt::server::world::terrain::decoration
