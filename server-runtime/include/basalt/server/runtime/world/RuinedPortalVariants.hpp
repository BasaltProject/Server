#pragma once

#include <basalt/server/world/terrain/RuinedPortalStructureGenerator.hpp>

#include <vector>

namespace basalt::server::runtime::world {

class BiomeTagLoader;

/// The seven Overworld ruined portal structures.
///
/// parity: `worldgen/structure/ruined_portal*.json`, whose numbers are these and whose biome lists
/// come out of the jar's own `has_structure/*` tags.
namespace ruinedPortalVariants {

[[nodiscard]] std::vector<server::world::terrain::RuinedPortalVariant> overworld(BiomeTagLoader& tags);

} // namespace ruinedPortalVariants

} // namespace basalt::server::runtime::world
