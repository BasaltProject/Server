#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::endCityPieces {

/// One placed End City template: which one, where, how it is turned, and whether it overwrites the
/// air it carries.
struct Piece {
    std::string templateName;
    structure::StructurePos position;
    structure::StructureRotation rotation = structure::StructureRotation::None;
    bool overwrite = false;
    const structure::StructureTemplate* piece = nullptr;
    structure::StructureBoundingBox boundingBox;
    /// The tag the collision test compares: a child that hits a piece of another branch is refused.
    std::int32_t generationDepth = 0;
};

/// parity: `EndCityPieces`, the recursive template graph used by the End City structure.
[[nodiscard]] std::vector<Piece> assemble(const structure::TemplateLoader& templateLoader,
                                          structure::StructurePos origin, structure::StructureRotation rotation,
                                          random::RandomSource& random);

} // namespace basalt::server::world::terrain::endCityPieces
