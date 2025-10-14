#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/FossilFeature.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

/// The Overworld `UNDERGROUND_STRUCTURES` nodes, step 3, in the order the biomes force:
/// `fossil_upper`, `fossil_lower`, `monster_room`, `monster_room_deep`. The fossils need a template
/// loader, and without one their draws do not happen, which is a different world: hence it is
/// threaded all the way here.
namespace overworldUndergroundStructures {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              const fossilFeature::TemplateLookup& templates = nullptr,
              const fossilFeature::OceanFloorAt& externalOceanFloorHeight = nullptr);

} // namespace overworldUndergroundStructures

} // namespace basalt::server::world::terrain::decoration
