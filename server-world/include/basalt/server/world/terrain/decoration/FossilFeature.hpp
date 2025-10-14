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

/// parity: `FossilFeature`. A fossil is two templates at one place: the bone base and an ore overlay
/// its own block rot thins. Both sit well below the placement's ground, which is why this scans its
/// whole footprint for the lowest ocean floor rather than trusting the one column it was handed.
namespace fossilFeature {

/// One of the two fossil features, which differ only in their overlay's processors.
struct Spec {
    std::vector<std::string> structures;
    std::vector<std::string> overlays;
    structure::StructureProcessorList fossilProcessors;
    structure::StructureProcessorList overlayProcessors;
    std::int32_t maxEmptyCornersAllowed = 0;
};

/// Resolves a structure path such as `fossil/spine_1` to its template.
using TemplateLookup = std::function<const structure::StructureTemplate*(std::string_view)>;

/// The `OCEAN_FLOOR` height of a column, or empty when it is out of reach.
using OceanFloorAt = std::function<std::optional<std::int32_t>(std::int32_t, std::int32_t)>;

/// Places one fossil at `origin`, or reports false without writing. The draw order is the jar's and
/// not the obvious one: rotation, then structure index, then burial depth.
bool place(Chunk& chunk, random::RandomSource& random, structure::StructurePos origin, const Spec& spec,
           const TemplateLookup& templates, const OceanFloorAt& oceanFloorAt);

} // namespace fossilFeature

} // namespace basalt::server::world::terrain::decoration
