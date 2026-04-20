#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/FossilFeature.hpp>
#include <basalt/server/world/terrain/surface/SurfaceRuleEnvironment.hpp>

#include <cstdint>

namespace basalt::server::world::terrain::decoration {

namespace overworldUndergroundStructures {

void decorate(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
              const fossilFeature::TemplateLookup& templates = nullptr,
              const fossilFeature::OceanFloorAt& externalOceanFloorHeight = nullptr);

} // namespace overworldUndergroundStructures

} // namespace basalt::server::world::terrain::decoration
