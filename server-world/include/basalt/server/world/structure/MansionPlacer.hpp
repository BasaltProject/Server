#pragma once

#include <basalt/server/world/structure/MansionRoomCollection.hpp>

namespace basalt::server::world::structure {

/// parity: `WoodlandMansionPieces.MansionPiecePlacer.createMansion`. Turns a floor plan into the
/// list of templates that make the house: the outer walls of the three storeys, the roofs, then per
/// floor the corridor tiles with their carpets and the walls and doors that close each room.
namespace mansionPlacer {

void createMansion(std::vector<MansionPiece>& pieces, random::RandomSource& random, StructurePos origin,
                   StructureRotation rotation, const MansionGrid& mansion);

} // namespace mansionPlacer

} // namespace basalt::server::world::structure
