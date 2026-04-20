#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructureProcessor.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::decoration {

/// parity: `FossilFeature`.
namespace fossilFeature {

struct Spec {
    std::vector<std::string> structures;
    std::vector<std::string> overlays;
    structure::StructureProcessorList fossilProcessors;
    structure::StructureProcessorList overlayProcessors;
    std::int32_t maxEmptyCornersAllowed = 0;
};

using TemplateLookup = std::function<const structure::StructureTemplate*(std::string_view)>;

using OceanFloorAt = std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t)>;

bool place(Chunk& chunk, random::RandomSource& random, structure::StructurePos origin, const Spec& spec,
           const TemplateLookup& templates, const OceanFloorAt& oceanFloorAt);

} // namespace fossilFeature

} // namespace basalt::server::world::terrain::decoration
