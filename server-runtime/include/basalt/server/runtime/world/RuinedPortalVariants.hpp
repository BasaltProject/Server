#pragma once

#include <basalt/server/world/terrain/RuinedPortalStructureGenerator.hpp>

#include <vector>

namespace basalt::server::runtime::world {

class BiomeTagLoader;

/// parity: `worldgen/structure/ruined_portal*.json`.
namespace ruinedPortalVariants {

[[nodiscard]] std::vector<server::world::terrain::RuinedPortalVariant> overworld(BiomeTagLoader& tags);

} // namespace ruinedPortalVariants

} // namespace basalt::server::runtime::world
